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
#include "AmB2BSession.h"
#include <string.h>
#include "AmSipHeaders.h"

SC_EXPORT(MOD_CLS_NAME);


MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("dlg.reply", DLGReplyAction);
  DEF_CMD("dlg.acceptInvite", DLGAcceptInviteAction);
  DEF_CMD("dlg.bye", DLGByeAction);
  DEF_CMD("dlg.connectCalleeRelayed", DLGConnectCalleeRelayedAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

bool DLGModule::onInvite(const AmSipRequest& req, DSMSession* sess) {
  // save inivital invite to last_req 
  // todo: save this in avar
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
EXEC_ACTION_START(DLGReplyAction) {
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
    sc_sess->SET_STRERROR("no last request to reply");
    return false;
  }

  if (sess->dlg.reply(*sc_sess->last_req.get(), code_i, reason)) {
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("error sending reply");
  } else
    sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

CONST_ACTION_2P(DLGAcceptInviteAction, ',', true);
EXEC_ACTION_START(DLGAcceptInviteAction) {
  // defaults to 200 OK
  unsigned int code_i=200;
  string reason = "OK";
  string code = resolveVars(par1, sess, sc_sess, event_params);
  DBG("replying with code %s\n", code.c_str());

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

  if (!sc_sess->last_req.get()) {
    ERROR("no last request to reply\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("no last request to reply");
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
  if (sc_sess->last_req.get()) {
    sc_sess->B2BaddReceivedRequest(*sc_sess->last_req.get());
  } else {
    WARN("internal error: initial INVITE request missing.\n");
  }
  AmB2BSession* b2b_sess = dynamic_cast<AmB2BSession*>(sess);
  if (b2b_sess) 
    b2b_sess->set_sip_relay_only(true);
  else 
    ERROR("getting B2B session.\n");

  sc_sess->B2BconnectCallee(remote_party, remote_uri, true);
} EXEC_ACTION_END;
