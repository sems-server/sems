/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 * Based on the concept of mycc, Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of sw_prepaid_sip, Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AuthB2B.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"

string AuthB2BFactory::user;
string AuthB2BFactory::domain;
string AuthB2BFactory::pwd;

EXPORT_SESSION_FACTORY(AuthB2BFactory,MOD_NAME);

AuthB2BFactory::AuthB2BFactory(const string& _app_name)
: AmSessionFactory(_app_name)
// , user_timer_fact(NULL)
{
}


int AuthB2BFactory::onLoad()
{
   AmConfigReader cfg;
   if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
     INFO("No configuration for auth_b2b present (%s)\n",
	  (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str()
	  );
   } else {
     user = cfg.getParameter("user");
     domain = cfg.getParameter("domain");
     pwd = cfg.getParameter("pwd");
   }

//   user_timer_fact = AmPlugIn::instance()->getFactory4Di("user_timer");
//   if(!user_timer_fact) {
//     ERROR("could not load user_timer from session_timer plug-in\n");
//     return -1;
//   }

  return 0;
}


AmSession* AuthB2BFactory::onInvite(const AmSipRequest& req)
{
//   AmDynInvoke* user_timer = user_timer_fact->getInstance();
//   if(!user_timer) {
//     ERROR("could not get a user timer reference\n");
//     throw AmSession::Exception(500,"could not get a user timer reference");
//   }

  return new AuthB2BDialog(); //user_timer);
}


AuthB2BDialog::AuthB2BDialog() // AmDynInvoke* user_timer)
: m_state(BB_Init),
  //m_user_timer(user_timer),
  AmB2BCallerSession()

{
  set_sip_relay_only(false);
}


AuthB2BDialog::~AuthB2BDialog()
{
}


void AuthB2BDialog::onInvite(const AmSipRequest& req)
{
  // TODO: do reinvites get here? if so, don't set a timer then
  // -> yes, they do.

  // TODO: errors thrown as exception don't seem to trigger a reply?
  // -> only in SessionFactory::onInvite they do. todo: move the logic to 
  //    session factory 

  // this will prevent us from being added to media processor
  setInOut(NULL,NULL); 

  if (AuthB2BFactory::user.empty()) {
    string app_param = getHeader(req.hdrs, PARAM_HDR);

    if (!app_param.length()) {
      AmSession::Exception(500, "auth_b2b: parameters not found");
    }
    
    domain = get_header_keyvalue(app_param,"d");
    user = get_header_keyvalue(app_param,"u");
    password = get_header_keyvalue(app_param,"p");
  } else {
    domain = AuthB2BFactory::domain;
    user = AuthB2BFactory::user;
    password = AuthB2BFactory::pwd;
  }

  from = "sip:"+user+"@"+domain;
  to = "sip:"+req.user+"@"+domain;

   // DBG("-----------------------------------------------------------------\n");
   // DBG("domain = %s, user = %s, pwd = %s, from = %s, to = %s;",  
   //     domain.c_str(), user.c_str(), password.c_str(), from.c_str(), to.c_str());
   // DBG("-----------------------------------------------------------------\n");

  m_state = BB_Dialing;

  if(dlg.reply(req, 100, "Connecting") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  invite_req = req;

  removeHeader(invite_req.hdrs,PARAM_HDR);
  removeHeader(invite_req.hdrs,"P-App-Name");

  dlg.updateStatus(req);
  recvd_req.insert(std::make_pair(req.cseq,req));
  
  set_sip_relay_only(true);
  connectCallee("<" + to + ">", to, true);
}


void AuthB2BDialog::process(AmEvent* ev)
{
//   AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
//   if(plugin_event && plugin_event->name == "timer_timeout") {
//     int timer_id = plugin_event->data.get(0).asInt();
//     if (timer_id == TIMERID_CREDIT_TIMEOUT) {
//       DBG("timer timeout, no more credit\n");
//       stopAccounting();
//       terminateOtherLeg();
//       terminateLeg();

//       ev->processed = true;
//       return;
//     }
//   }

  AmB2BCallerSession::process(ev);
}


bool AuthB2BDialog::onOtherReply(const AmSipReply& reply)
{
  bool ret = false;

  if ((m_state == BB_Dialing) && (reply.cseq == invite_req.cseq)) {
    if (reply.code < 200) {
      DBG("Callee is trying... code %d\n", reply.code);
    }
    else if(reply.code < 300) {
      if(getCalleeStatus()  == Connected) {
        m_state = BB_Connected;
        setInOut(NULL, NULL);

//         // set the call timer
//         AmArg di_args,ret;
//         di_args.push(TIMERID_CREDIT_TIMEOUT);
//         di_args.push(m_credit);  // in seconds
//         di_args.push(dlg.local_tag.c_str());
//         m_user_timer->invoke("setTimer", di_args, ret);
      }
    }
    else if(reply.code == 487 && dlg.getStatus() == AmSipDialog::Pending) {
      DBG("Stopping leg A on 487 from B with 487\n");
      dlg.reply(invite_req, 487, "Request terminated");
      setStopped();
      ret = true;
    }
    else if (reply.code >= 300 && dlg.getStatus() == AmSipDialog::Connected) {
      DBG("Callee final error in connected state with code %d\n",reply.code);
      terminateLeg();
    }
    else {
      DBG("Callee final error with code %d\n",reply.code);
      AmB2BCallerSession::onOtherReply(reply);
    }
  }
  return ret;
}


void AuthB2BDialog::onOtherBye(const AmSipRequest& req)
{
//   stopAccounting();
  AmB2BCallerSession::onOtherBye(req);
}


void AuthB2BDialog::onBye(const AmSipRequest& req)
{
  if (m_state == BB_Connected) {
//     stopAccounting();
  }
  terminateOtherLeg();
  setStopped();
}


void AuthB2BDialog::onCancel()
{
  if(dlg.getStatus() == AmSipDialog::Pending) {
    DBG("Wait for leg B to terminate");
  }
  else {
    DBG("Canceling leg A on CANCEL since dialog is not pending");
    dlg.reply(invite_req, 487, "Request terminated");
    setStopped();
  }
}

void AuthB2BDialog::createCalleeSession()
{
  AuthB2BCalleeSession* callee_session = new AuthB2BCalleeSession(this, user, password);
  
  // adding auth handler
  AmSessionEventHandlerFactory* uac_auth_f = 
    AmPlugIn::instance()->getFactory4Seh("uac_auth");
  if (NULL == uac_auth_f)  {
    INFO("uac_auth module not loaded. uac auth NOT enabled for callee session.\n");
  } else {
    AmSessionEventHandler* h = uac_auth_f->getHandler(callee_session);

    // we cannot use the generic AmSessionEventHandler hooks, 
    // because the hooks don't work in AmB2BSession
    callee_session->setAuthHandler(h);
    DBG("uac auth enabled for callee session.\n");
  }
  
  AmSipDialog& callee_dlg = callee_session->dlg;
  
  other_id = AmSession::getNewId();
  
  callee_dlg.local_tag    = other_id;
  callee_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;
  
  // this will be overwritten by ConnectLeg event 
  callee_dlg.remote_party = dlg.local_party;
  callee_dlg.remote_uri   = dlg.local_uri;

  // if given as parameters, use these
  callee_dlg.local_party  = from; 
  callee_dlg.local_uri    = from; 
  
  DBG("Created B2BUA callee leg, From: %s\n",
      from.c_str());

  if (AmConfig::LogSessions) {
    INFO("Starting B2B callee session %s app %s\n",
	 callee_session->getLocalTag().c_str(), invite_req.cmd.c_str());
  }

  MONITORING_LOG5(other_id.c_str(), 
		  "app",  invite_req.cmd.c_str(),
		  "dir",  "out",
		  "from", callee_dlg.local_party.c_str(),
		  "to",   callee_dlg.remote_party.c_str(),
		  "ruri", callee_dlg.remote_uri.c_str());

  callee_session->start();
  
  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}

AuthB2BCalleeSession::AuthB2BCalleeSession(const AmB2BCallerSession* caller,
					   const string& user, const string& pwd) 
  : auth(NULL), 
    credentials("", user, pwd), // domain (realm) is unused in credentials 
    AmB2BCalleeSession(caller) {
}

AuthB2BCalleeSession::~AuthB2BCalleeSession() {
  if (auth) 
    delete auth;
}

inline UACAuthCred* AuthB2BCalleeSession::getCredentials() {
  return &credentials;
}

void AuthB2BCalleeSession::onSipReply(const AmSipReply& reply) {
  if (NULL == auth) {    
    AmB2BCalleeSession::onSipReply(reply);
    return;
  }
  
  int cseq_before = dlg.cseq;
  if (!auth->onSipReply(reply)) {
    AmB2BCalleeSession::onSipReply(reply);
  } else {
    if (cseq_before != dlg.cseq) {
      DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
          "updating relayed_req map\n", 
	  reply.cseq, cseq_before);
      TransMap::iterator it=relayed_req.find(reply.cseq);
      if (it != relayed_req.end()) {
	relayed_req[cseq_before] = it->second;
	relayed_req.erase(it);
      }
    }
  }
}

void AuthB2BCalleeSession::onSendRequest(const string& method, const string& content_type,
			      const string& body, string& hdrs, int flags, unsigned int cseq)
{
  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", cseq);
    auth->onSendRequest(method, content_type,
			body, hdrs, flags, cseq);
  }
  
  AmB2BCalleeSession::onSendRequest(method, content_type,
				     body, hdrs, flags, cseq);
}

