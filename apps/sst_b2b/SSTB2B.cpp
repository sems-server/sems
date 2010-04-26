/*
 * $Id: SSTB2B.cpp 1784 2010-04-15 13:01:00Z sayer $
 *
 * Copyright (C) 2010 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "SSTB2B.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"

string SSTB2BFactory::user;
string SSTB2BFactory::domain;
string SSTB2BFactory::pwd;
AmConfigReader SSTB2BFactory::cfg;
AmSessionEventHandlerFactory* SSTB2BFactory::session_timer_fact = NULL;

EXPORT_SESSION_FACTORY(SSTB2BFactory,MOD_NAME);

SSTB2BFactory::SSTB2BFactory(const string& _app_name)
: AmSessionFactory(_app_name)
{
}


int SSTB2BFactory::onLoad()
{
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    INFO("No configuration for sst_b2b present (%s)\n",
	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str()
	 );
  }

  session_timer_fact = AmPlugIn::instance()->getFactory4Seh("session_timer");
  if(!session_timer_fact) {
    ERROR("could not load session_timer from session_timer plug-in\n");
    return -1;
  }

  return 0;
}


AmSession* SSTB2BFactory::onInvite(const AmSipRequest& req)
{

  SSTB2BDialog* b2b_dlg = new SSTB2BDialog();
  AmSessionEventHandler* h = session_timer_fact->getHandler(b2b_dlg);
  if(!h) {
    ERROR("could not get a session timer event handler\n");
    throw AmSession::Exception(500,"Server internal error");
  }
  if(h->configure(cfg)){
    ERROR("Could not configure the session timer: disabling session timers.\n");
    delete h;
  } else {
    b2b_dlg->addHandler(h);
  }

  return b2b_dlg;
}


SSTB2BDialog::SSTB2BDialog() // AmDynInvoke* user_timer)
: m_state(BB_Init),
  AmB2BCallerSession()

{
  set_sip_relay_only(false);
}


SSTB2BDialog::~SSTB2BDialog()
{
}


void SSTB2BDialog::onInvite(const AmSipRequest& req)
{
  DBG("onINVITE -------------------------------\n");
  // this will prevent us from being added to media processor
  setInOut(NULL,NULL); 

  from = req.from;
  to = req.to;

  m_state = BB_Dialing;

  if(dlg.reply(req, 100, "Connecting") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  invite_req = req;

  removeHeader(invite_req.hdrs,PARAM_HDR);
  removeHeader(invite_req.hdrs,"P-App-Name");

  //dlg.updateStatus(req);
  recvd_req.insert(std::make_pair(req.cseq,req));
  
  set_sip_relay_only(true);
  connectCallee("<" + req.r_uri + ">", req.r_uri, true);
}

void SSTB2BDialog::sendReinvite(bool updateSDP, const string& headers) {
  if (sip_relay_only) {
    // we send empty reinvite 
    DBG("sending empty reinvite in callee session\n");
    dlg.reinvite(headers, "", ""); 
  } else {
    AmB2BCallerSession::sendReinvite(updateSDP, headers);
  }

  // // we send empty reinvite
  // dlg.reinvite(headers, "", "");
  // we send reinvite with the last body we got from the other side
  // last_otherleg_content_type, last_otherleg_body);
}

void SSTB2BDialog::process(AmEvent* ev)
{
  AmB2BCallerSession::process(ev);
}

void SSTB2BDialog::onSipRequest(const AmSipRequest& req) {
  // AmB2BSession does not call AmSession::onSipRequest for 
  // forwarded requests - so lets call event handlers here
  // todo: this is a hack, replace this by calling proper session 
  // event handler in AmB2BSession
  bool fwd = sip_relay_only &&
    (req.method != "BYE") &&
    (req.method != "CANCEL");
  if (fwd) {
      CALL_EVENT_H(onSipRequest,req);
  }

  AmB2BCallerSession::onSipRequest(req);
}

void SSTB2BDialog::onSipReply(const AmSipReply& reply, int old_dlg_status) 
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();

  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.content_type.c_str());
  if (fwd) {
      CALL_EVENT_H(onSipReply,reply);    
  }

  AmB2BCallerSession::onSipReply(reply,old_dlg_status);
}

bool SSTB2BDialog::onOtherReply(const AmSipReply& reply)
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


void SSTB2BDialog::onOtherBye(const AmSipRequest& req)
{
//   stopAccounting();
  AmB2BCallerSession::onOtherBye(req);
}


void SSTB2BDialog::onBye(const AmSipRequest& req)
{
  if (m_state == BB_Connected) {
//     stopAccounting();
  }
  terminateOtherLeg();
  setStopped();
}


void SSTB2BDialog::onCancel()
{
  if(dlg.getStatus() == AmSipDialog::Pending) {
    DBG("Wait for leg B to terminate");
  } else {
    DBG("Canceling leg A on CANCEL since dialog is not pending");
    dlg.reply(invite_req, 487, "Request terminated");
    setStopped();
  }
}

void SSTB2BDialog::createCalleeSession()
{
  SSTB2BCalleeSession* callee_session = new SSTB2BCalleeSession(this, user, password);
  
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

  AmSessionEventHandler* h = SSTB2BFactory::session_timer_fact->getHandler(callee_session);
  if(!h) {
    ERROR("could not get a session timer event handler\n");
    delete callee_session;
    throw AmSession::Exception(500,"Server internal error");
  }
  if(h->configure(SSTB2BFactory::cfg)){
    ERROR("Could not configure the session timer: disabling session timers.\n");
    delete h;
  } else {
    callee_session->addHandler(h);
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

SSTB2BCalleeSession::SSTB2BCalleeSession(const AmB2BCallerSession* caller,
					   const string& user, const string& pwd) 
  : auth(NULL), 
    credentials("", user, pwd), // domain (realm) is unused in credentials 
    AmB2BCalleeSession(caller) {
}

SSTB2BCalleeSession::~SSTB2BCalleeSession() {
  if (auth) 
    delete auth;
}

inline UACAuthCred* SSTB2BCalleeSession::getCredentials() {
  return &credentials;
}

void SSTB2BCalleeSession::onSipRequest(const AmSipRequest& req) {
  // AmB2BSession does not call AmSession::onSipRequest for 
  // forwarded requests - so lets call event handlers here
  // todo: this is a hack, replace this by calling proper session 
  // event handler in AmB2BSession
  bool fwd = sip_relay_only &&
    (req.method != "BYE") &&
    (req.method != "CANCEL");
  if (fwd) {
      CALL_EVENT_H(onSipRequest,req);
  }

  AmB2BCalleeSession::onSipRequest(req);
}

void SSTB2BCalleeSession::onSipReply(const AmSipReply& reply, int old_dlg_status) 
{
  // call event handlers where it is not done 
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();
  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.content_type.c_str());
  if(fwd) {
      CALL_EVENT_H(onSipReply,reply);    
  }

  if (NULL == auth) {    
      AmB2BCalleeSession::onSipReply(reply,old_dlg_status);
    return;
  }
  
  unsigned int cseq_before = dlg.cseq;
  if (!auth->onSipReply(reply)) {
      AmB2BCalleeSession::onSipReply(reply,old_dlg_status);
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

void SSTB2BCalleeSession::onSendRequest(const string& method, const string& content_type,
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

void SSTB2BCalleeSession::sendReinvite(bool updateSDP, const string& headers) {
  if (sip_relay_only) {
    // we send empty reinvite 
    DBG("sending empty reinvite in callee session\n");
    dlg.reinvite(headers, "", ""); 
  } else {
    AmB2BCalleeSession::sendReinvite(updateSDP, headers);
  }
}

