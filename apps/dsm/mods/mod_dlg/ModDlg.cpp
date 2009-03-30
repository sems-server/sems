/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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
#include <string.h>

SC_EXPORT(DLGModule);

DLGModule::DLGModule() {
}

DLGModule::~DLGModule() {
}

void splitCmd(const string& from_str, 
	      string& cmd, string& params) {
  size_t b_pos = from_str.find('(');
  if (b_pos != string::npos) {
    cmd = from_str.substr(0, b_pos);
    params = from_str.substr(b_pos + 1, from_str.rfind(')') - b_pos -1);
  } else 
    cmd = from_str;  
}

DSMAction* DLGModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

#define DEF_CMD(cmd_name, class_name) \
				      \
  if (cmd == cmd_name) {	      \
    class_name * a =		      \
      new class_name(params);	      \
    a->name = from_str;		      \
    return a;			      \
  }

  DEF_CMD("dlg.reply", DLGReplyAction);
  DEF_CMD("dlg.acceptInvite", DLGAcceptInviteAction);

  return NULL;
}

DSMCondition* DLGModule::getCondition(const string& from_str) {
  return NULL;
}

bool DLGModule::onInvite(const AmSipRequest& req, DSMSession* sess) {
  // save inivital invite to last_req 
  sess->last_req.reset(new AmSipRequest(req));
  return true;
}

#define GET_SCSESSION()					 \
  DSMSession* sc_sess = dynamic_cast<DSMSession*>(sess); \
  if (!sc_sess) {					 \
    ERROR("wrong session type\n");			 \
    return false;					 \
  }

CONST_ACTION_2P(DLGReplyAction, ',', true);

bool DLGReplyAction::execute(AmSession* sess, 
			     DSMCondition::EventType event,
			     map<string,string>* event_params) {
  GET_SCSESSION();

  string code = resolveVars(par1, sess, sc_sess, event_params);
  string reason = resolveVars(par2, sess, sc_sess, event_params);
  unsigned int code_i;
  if (str2i(code, code_i)) {
    ERROR("decoding reply code '%s'\n", code.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return false;
  }

  if (!sc_sess->last_req.get()) {
    ERROR("no last request to reply\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    return false;
  }

  if (sess->dlg.reply(*sc_sess->last_req.get(), code_i, reason))    
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
  else
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);

  return false;
}

CONST_ACTION_2P(DLGAcceptInviteAction, ',', true);

bool DLGAcceptInviteAction::execute(AmSession* sess, 
				     DSMCondition::EventType event,
				     map<string,string>* event_params) {
  GET_SCSESSION();

  // defaults to 200 OK
  unsigned int code_i=200;
  string reason = "OK";
  string code = resolveVars(par1, sess, sc_sess, event_params);
  DBG("GOT CODE %s\n", code.c_str());
  if (code.length()) {
    reason = resolveVars(par2, sess, sc_sess, event_params);
    if (str2i(code, code_i)) {
      ERROR("decoding reply code '%s'\n", code.c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      return false;
    }
  }

  if (!sc_sess->last_req.get()) {
    ERROR("no last request to reply\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    return false;
  }

  try {
    string sdp_reply;

    sess->acceptAudio(sc_sess->last_req.get()->body,
		      sc_sess->last_req.get()->hdrs,&sdp_reply);
    if(sess->dlg.reply(*sc_sess->last_req.get(),code_i, reason,
		 "application/sdp",sdp_reply) != 0)
      throw AmSession::Exception(500,"could not send response");
	
  }catch(const AmSession::Exception& e){

    ERROR("%i %s\n",e.code,e.reason.c_str());
    sess->setStopped();
    AmSipDialog::reply_error(*sc_sess->last_req.get(),e.code,e.reason);
  }

  return false;
}
