/*
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "ModDlg.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "AmB2BSession.h"
#include <string.h>
#include "AmSipHeaders.h"

#include "AmUAC.h"
#include "ampi/UACAuthAPI.h"

SC_EXPORT(MOD_CLS_NAME);


MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("dlg.reply", DLGReplyAction);
  DEF_CMD("dlg.replyRequest", DLGReplyRequestAction);
  DEF_CMD("dlg.acceptInvite", DLGAcceptInviteAction);
  DEF_CMD("dlg.bye", DLGByeAction);
  DEF_CMD("dlg.connectCalleeRelayed", DLGConnectCalleeRelayedAction);
  DEF_CMD("dlg.dialout", DLGDialoutAction);

  DEF_CMD("dlg.getRequestBody", DLGGetRequestBodyAction)
  DEF_CMD("dlg.getReplyBody", DLGGetReplyBodyAction)
} MOD_ACTIONEXPORT_END;

//MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);


MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {
  if (cmd == "dlg.replyHasContentType")
    return new DLGReplyHasContentTypeCondition(params, false);

  if (cmd == "dlg.requestHasContentType")
    return new DLGRequestHasContentTypeCondition(params, false);

} MOD_CONDITIONEXPORT_END;

bool DLGModule::onInvite(const AmSipRequest& req, DSMSession* sess) {
  // save inivital invite to last_req 
  // todo: save this in avar
 sess->last_req.reset(new AmSipRequest(req));
 return true;
}

string replaceLineEnds(string input)
{
  string result;
  size_t last = 0;
  size_t pos;
  while ((pos = input.find("\\r\\n", last)) != string::npos) {
    result += input.substr(last, pos-last);
    result += "\r\n";
    last = pos + 4;
  }
  if (!input.substr(last).empty()) {
    result += input.substr(last);
    result += "\r\n";
  }
  return result;
}

// todo: convert errors to exceptions
void replyRequest(DSMSession* sc_sess, AmSession* sess, 
		  EventParamT* event_params,
		  const string& par1, const string& par2,
		  const AmSipRequest& req) {
  string code = resolveVars(par1, sess, sc_sess, event_params);
  string reason = resolveVars(par2, sess, sc_sess, event_params);
  string hdrs = replaceLineEnds(resolveVars("$dlg.reply.hdrs", sess, sc_sess, event_params));
  unsigned int code_i;
  if (str2i(code, code_i)) {
    ERROR("decoding reply code '%s'\n", code.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return;
  }

  DBG("replying with %i %s, hdrs='%s'\n", code_i, reason.c_str(), hdrs.c_str());

  if (sess->dlg.reply(req, code_i, reason, NULL, hdrs)) {
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("error sending reply");
  } else
    sc_sess->CLR_ERRNO;
}

CONST_ACTION_2P(DLGReplyAction, ',', true);
EXEC_ACTION_START(DLGReplyAction) {

  if (!sc_sess->last_req.get()) {
    ERROR("no last request to reply\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("no last request to reply");
    return false;
  }

  replyRequest(sc_sess, sess, event_params, par1, par2, *sc_sess->last_req.get());
} EXEC_ACTION_END;

// todo (?) move replyRequest to core module (?)
CONST_ACTION_2P(DLGReplyRequestAction, ',', true);
EXEC_ACTION_START(DLGReplyRequestAction) {
  DSMSipRequest* sip_req;

  AVarMapT::iterator it = sc_sess->avar.find(DSM_AVAR_REQUEST);
  if (it == sc_sess->avar.end() ||
      !isArgAObject(it->second) || 
      !(sip_req = dynamic_cast<DSMSipRequest*>(it->second.asObject()))) {
    throw DSMException("dlg", "cause", "no request");
  }
    
  replyRequest(sc_sess, sess, event_params, par1, par2, *sip_req->req);
} EXEC_ACTION_END;

CONST_ACTION_2P(DLGAcceptInviteAction, ',', true);
EXEC_ACTION_START(DLGAcceptInviteAction) {
  // defaults to 200 OK
  unsigned int code_i=200;
  string reason = "OK";
  string code = resolveVars(par1, sess, sc_sess, event_params);
  string hdrs = replaceLineEnds(resolveVars("$dlg.reply.hdrs", sess, sc_sess, event_params));

  if (code.length()) {
    reason = resolveVars(par2, sess, sc_sess, event_params);
    if (str2i(code, code_i)) {
      ERROR("decoding reply code '%s'\n", code.c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("decoding reply code '"+
			    code+"%s'\n");
      return false;
    }
  }

  DBG("replying with %i %s, hdrs='%s'\n", code_i, reason.c_str(), hdrs.c_str());

  if (!sc_sess->last_req.get()) {
    ERROR("no last request to reply\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("no last request to reply");
    return false;
  }

  try {
    AmMimeBody sdp_body;
    if(sess->dlg.reply(*sc_sess->last_req.get(),code_i, reason,
		       sdp_body.addPart(SIP_APPLICATION_SDP), hdrs) != 0)
      throw AmSession::Exception(500,"could not send response");

  }catch(const AmSession::Exception& e){

    ERROR("%i %s\n",e.code,e.reason.c_str());
    sess->setStopped();
    sess->dlg.reply(*sc_sess->last_req.get(),e.code,e.reason);
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(DLGByeAction) {
  string hdrs = resolveVars(arg, sess, sc_sess, event_params);

  if (sess->dlg.bye(hdrs)) {
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("Error sending bye");
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);
  }
} EXEC_ACTION_END;


CONST_ACTION_2P(DLGConnectCalleeRelayedAction,',', false);
EXEC_ACTION_START(DLGConnectCalleeRelayedAction) {  
  string remote_party = resolveVars(par1, sess, sc_sess, event_params);
  string remote_uri = resolveVars(par2, sess, sc_sess, event_params);

  // if (sc_sess->last_req.get()) {
  //   sc_sess->B2BaddReceivedRequest(*sc_sess->last_req.get());
  // } else {
  //   WARN("internal error: initial INVITE request missing.\n");
  // }
  // AmB2BSession* b2b_sess = dynamic_cast<AmB2BSession*>(sess);
  // if (b2b_sess) 
  //   b2b_sess->set_sip_relay_only(true);
  // else 
  //   ERROR("getting B2B session.\n");

  sc_sess->B2BconnectCallee(remote_party, remote_uri, true);
} EXEC_ACTION_END;

EXEC_ACTION_START(DLGDialoutAction) {  
  string arrayname = resolveVars(arg, sess, sc_sess, event_params);

#define GET_VARIABLE_MANDATORY(varname_suffix, outvar)			\
  it = sc_sess->var.find(arrayname+varname_suffix); \
  if (it == sc_sess->var.end()) {					\
    WARN("%s", std::string("need " + arrayname + varname_suffix " set for dlg.dialoutSimple("+arrayname+")").c_str()); \
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);				\
    return false;							\
  }									\
  outvar = it->second;

#define GET_VARIABLE_OPTIONAL(varname_suffix, outvar) \
  it = sc_sess->var.find(arrayname+varname_suffix);  \
  if (it != sc_sess->var.end())		      \
    outvar = it->second;

  map<string, string>::iterator it; 

  string v_from;
  GET_VARIABLE_MANDATORY("_caller", v_from);
  string v_to;
  GET_VARIABLE_MANDATORY("_callee", v_to);
  string v_domain;
  GET_VARIABLE_MANDATORY("_domain", v_domain);
  string app_name;
  GET_VARIABLE_MANDATORY("_app", app_name);

  string user = v_from;
  string r_uri = "sip:"+v_to+"@"+v_domain;

  GET_VARIABLE_OPTIONAL("_r_uri", r_uri);

  string from = "<sip:"+v_from+"@"+v_domain+">"; 
  GET_VARIABLE_OPTIONAL("_from", from);

  string from_uri = "sip:"+v_from+"@"+v_domain; 
  GET_VARIABLE_OPTIONAL("_from_uri", from_uri);

  string to = "<sip:"+v_to+"@"+v_domain+">";
  GET_VARIABLE_OPTIONAL("_to", to);

  string auth_user; 
  GET_VARIABLE_OPTIONAL("_auth_user", auth_user);

  string auth_pwd; 
  GET_VARIABLE_OPTIONAL("_auth_pwd", auth_pwd);
   
  string ltag; 
  GET_VARIABLE_OPTIONAL("_ltag", ltag);

  string hdrs; 
  GET_VARIABLE_OPTIONAL("_hdrs", hdrs);
  
  if (hdrs.length()) {
    size_t crpos;
    while ((crpos=hdrs.find("\\r\\n")) != string::npos) {
      hdrs.replace(crpos, 4, "\r\n");
    }
  }

#undef GET_VARIABLE_MANDATORY
#undef GET_VARIABLE_OPTIONAL

  DBG("placing UAC call: user <%s>, app <%s>, ruri <%s>, from <%s> "
      "from_uri <%s>, to <%s>, ltag <%s>, hdrs <%s>, auth_user <%s>, auth_pwd <not shown>\n",
      user.c_str(), app_name.c_str(), r_uri.c_str(), from.c_str(),
      from_uri.c_str(), to.c_str(), ltag.c_str(), hdrs.c_str(), auth_user.c_str());

  AmArg* sess_params = new AmArg();
  bool has_auth = false;
  if (!auth_user.empty() && !auth_pwd.empty()) {
    AmArg auth_param;
    auth_param.setBorrowedPointer(new UACAuthCred("", auth_user,auth_pwd));
    sess_params->push(auth_param);
    has_auth = true;
  }
  AmArg var_struct;
  string varprefix = arrayname+"_var.";
  bool has_vars = false;
  map<string, string>::iterator lb = sc_sess->var.lower_bound(varprefix);
  while (lb != sc_sess->var.end()) {
    if ((lb->first.length() < varprefix.length()) ||
	strncmp(lb->first.c_str(), varprefix.c_str(),varprefix.length()))
      break;
    string varname = lb->first.substr(varprefix.length());
    if (!has_auth) // sess_params is variable struct
      (*sess_params)[varname] = lb->second;
    else // variable struct is in sess_params array
      var_struct[varname] = lb->second;

    lb++;
    has_vars = true;
  }

  if (has_vars && has_auth)
      sess_params->push(var_struct);
 
  DBG("sess_params: '%s'\n", AmArg::print(*sess_params).c_str());

  string new_sess_tag = AmUAC::dialout(user, app_name, r_uri, from, from_uri, to, ltag, hdrs, sess_params);

  if (!new_sess_tag.empty()) {
    sc_sess->var[arrayname + "_ltag"] = new_sess_tag;
  } else {
    sc_sess->var[arrayname + "_ltag"] = "";
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
  }

} EXEC_ACTION_END;

MATCH_CONDITION_START(DLGReplyHasContentTypeCondition) {
  AVarMapT::iterator it = sc_sess->avar.find(DSM_AVAR_REPLY);
  if (it == sc_sess->avar.end()) {
    ERROR("DSM script error: dlg.replyHasContentType condition used for "
	  "other event than sipReply event\n");
    return false;
  }

  DSMSipReply* dsm_reply = NULL;
  if (!isArgAObject(sc_sess->avar[DSM_AVAR_REPLY]) ||
      (NULL ==
       (dsm_reply = dynamic_cast<DSMSipReply*>(sc_sess->avar[DSM_AVAR_REPLY].asObject())))) {
    ERROR("internal: DSM could not get DSMSipReply\n");
    return false;
  }

  bool res = dsm_reply->reply->body.hasContentType(arg);

  DBG("checking for content_type '%s': %s\n", arg.c_str(), res?"has it":"doesn't have it");
  return res;
} MATCH_CONDITION_END;

MATCH_CONDITION_START(DLGRequestHasContentTypeCondition) {
  AVarMapT::iterator it = sc_sess->avar.find(DSM_AVAR_REQUEST);
  if (it == sc_sess->avar.end()) {
    ERROR("DSM script error: dlg.requestHasContentType condition used for "
	  "other event than sipRequest event\n");
    return false;
  }

  DSMSipRequest* dsm_req = NULL;
  if (!isArgAObject(sc_sess->avar[DSM_AVAR_REQUEST]) ||
      (NULL ==
       (dsm_req = dynamic_cast<DSMSipRequest*>(sc_sess->avar[DSM_AVAR_REQUEST].asObject())))) {
    ERROR("internal: DSM could not get DSMSipRequest\n");
    return false;
  }

  bool res = dsm_req->req->body.hasContentType(arg);

  DBG("checking for content_type '%s': %s\n", arg.c_str(), res?"has it":"doesn't have it");
  return res;
} MATCH_CONDITION_END;

CONST_ACTION_2P(DLGGetRequestBodyAction, ',', false);
EXEC_ACTION_START(DLGGetRequestBodyAction) {
  DSMSipRequest* sip_req;

  AVarMapT::iterator it = sc_sess->avar.find(DSM_AVAR_REQUEST);
  if (it == sc_sess->avar.end() ||
      !isArgAObject(it->second) ||
      !(sip_req = dynamic_cast<DSMSipRequest*>(it->second.asObject()))) {
    throw DSMException("dlg", "cause", "no request");
  }

  string content_type = resolveVars(par1, sess, sc_sess, event_params);
  string dstvar = resolveVars(par2, sess, sc_sess, event_params);

  const AmMimeBody* msg_body = sip_req->req->body.hasContentType(content_type);
  if (NULL == msg_body) {
    DBG("body with content_type %s not found\n", content_type.c_str());
    sc_sess->var.erase(dstvar);
  } else {
    sc_sess->var[dstvar] = string((const char*)msg_body->getPayload());
    DBG("set $%s='%s'\n", dstvar.c_str(), sc_sess->var[dstvar].c_str());
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(DLGGetReplyBodyAction, ',', false);
EXEC_ACTION_START(DLGGetReplyBodyAction) {
  DSMSipReply* sip_req;

  AVarMapT::iterator it = sc_sess->avar.find(DSM_AVAR_REPLY);
  if (it == sc_sess->avar.end() ||
      !isArgAObject(it->second) ||
      !(sip_req = dynamic_cast<DSMSipReply*>(it->second.asObject()))) {
    throw DSMException("dlg", "cause", "no reply");
  }

  string content_type = resolveVars(par1, sess, sc_sess, event_params);
  string dstvar = resolveVars(par2, sess, sc_sess, event_params);

  const AmMimeBody* msg_body = sip_req->reply->body.hasContentType(content_type);
  if (NULL == msg_body) {
    DBG("body with content_type %s not found\n", content_type.c_str());
    sc_sess->var.erase(dstvar);
  } else {
    sc_sess->var[dstvar] = string((const char*)msg_body->getPayload());
    DBG("set $%s='%s'\n", dstvar.c_str(), sc_sess->var[dstvar].c_str());
  }
} EXEC_ACTION_END;
