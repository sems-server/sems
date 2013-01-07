/*
 * Copyright (C) 2008 Greger Viken Teigre
 * Based on auth_b2b, Copyright (C) 2008 Iptego Gmbh
 * Based on the concept of mycc, Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of sw_prepaid_sip, Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "b2b_connect.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"

bool b2b_connectFactory::TransparentHeaders = true; // default
bool b2b_connectFactory::TransparentDestination = false; // default

EXPORT_SESSION_FACTORY(b2b_connectFactory,MOD_NAME);

b2b_connectFactory::b2b_connectFactory(const string& _app_name)
: AmSessionFactory(_app_name)
{
}


int b2b_connectFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    INFO("configuration file '%s' not found. using defaults.\n",
	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
    return 0;
  }

  if (cfg.getParameter("transparent_headers")=="false")
    TransparentHeaders = false;

  if (cfg.getParameter("transparent_ruri")=="true")
    TransparentDestination = true;
  
  return 0;
}


AmSession* b2b_connectFactory::onInvite(const AmSipRequest& req, const string& app_name,
					const map<string,string>& app_params)
{
  if (!app_params.size())  {
    throw  AmSession::Exception(500, "b2b_connect: parameters not found");
  }

  return new b2b_connectDialog();
}


b2b_connectDialog::b2b_connectDialog()
  : AmB2ABCallerSession()

{
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT); 
}

b2b_connectDialog::~b2b_connectDialog()
{
}

void b2b_connectDialog::onInvite(const AmSipRequest& req)
{
  if (dlg->getStatus() == AmSipDialog::Connected) {
    // reinvites
    AmB2ABCallerSession::onInvite(req);
    return;
  }

  string remote_party, remote_uri;

  domain = getAppParam("d");
  user = getAppParam("u");
  password = getAppParam("p");

  if (domain.empty() || user.empty()) {
    throw AmSession::Exception(500, "b2b_connect: domain parameters not found");
  }

  from = "sip:"+user+"@"+domain;
  if (b2b_connectFactory::TransparentDestination) {
      remote_uri = req.r_uri;
      remote_party = to = req.to;
  } else {
      remote_uri = to = "sip:"+req.user+"@"+domain;
      remote_party = "<" + to + ">";
  }
  if(dlg->reply(req, 100, "Connecting") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  invite_req = req;
  if (b2b_connectFactory::TransparentHeaders) {
    removeHeader(invite_req.hdrs, PARAM_HDR);
    removeHeader(invite_req.hdrs, "P-App-Name");
    removeHeader(invite_req.hdrs, "User-Agent");
    removeHeader(invite_req.hdrs, "Max-Forwards");
  }

  recvd_req.insert(std::make_pair(req.cseq,req));
  
  connectCallee(remote_party, remote_uri, from, from, 
		b2b_connectFactory::TransparentHeaders ? invite_req.hdrs : "");

  MONITORING_LOG(other_id.c_str(), 
		 "app", MOD_NAME);
  
}

void b2b_connectDialog::onSessionStart()
{
  AmB2ABCallerSession::onSessionStart();
}

void b2b_connectDialog::onB2ABEvent(B2ABEvent* ev)
{
  if (ev->event_id == B2ABConnectOtherLegException) {
    B2ABConnectOtherLegExceptionEvent* ex_ev = 
      dynamic_cast<B2ABConnectOtherLegExceptionEvent*>(ev);
    if (ex_ev && dlg->getStatus() < AmSipDialog::Connected) {
      DBG("callee leg creation failed with exception '%d %s'\n",
	  ex_ev->code, ex_ev->reason.c_str());
      dlg->reply(invite_req, ex_ev->code, ex_ev->reason);
      setStopped();
      return;
    }
  }

  if (ev->event_id == B2ABConnectOtherLegFailed) {
    B2ABConnectOtherLegFailedEvent* f_ev = 
      dynamic_cast<B2ABConnectOtherLegFailedEvent*>(ev);
    if (f_ev && dlg->getStatus() < AmSipDialog::Connected) {
      DBG("callee leg creation failed with reply '%d %s'\n",
	  f_ev->code, f_ev->reason.c_str());
      dlg->reply(invite_req, f_ev->code, f_ev->reason);
      setStopped();
      return;
    }
  }

  if (ev->event_id == B2ABConnectAudio) {
    // delayed processing of first INVITE request
    AmSession::onInvite(invite_req);
  }

  AmB2ABCallerSession::onB2ABEvent(ev);
}

void b2b_connectDialog::process(AmEvent* ev)
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

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(ev);
	
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    DBG("ignoring end of prompt.\n");
    return;
  }

  AmB2ABCallerSession::process(ev);
}

void b2b_connectDialog::onDtmf(int event, int duration)
{
  DBG("DTMF event %d duration %d\n", event, duration);
  return;
  
}

void b2b_connectDialog::onBye(const AmSipRequest& req)
{
  terminateOtherLeg();
  setStopped();
}


void b2b_connectDialog::onCancel(const AmSipRequest& req)
{
  if(dlg->getStatus() < AmSipDialog::Connected) {
    DBG("Wait for leg B to terminate");
  }
  else {
    DBG("Canceling leg A on CANCEL since dialog is not pending");
    dlg->reply(invite_req, 487, "Request terminated");
    setStopped();
  }
}


AmB2ABCalleeSession* b2b_connectDialog::createCalleeSession()
{
  b2b_connectCalleeSession* sess =
    new b2b_connectCalleeSession(getLocalTag(), connector, user, password);

  AmUACAuth::enable(sess);

  return sess;
}

b2b_connectCalleeSession::b2b_connectCalleeSession(const string& other_tag,
						   AmSessionAudioConnector* connector,
						   const string& user, const string& pwd) 
  : credentials("", user, pwd), // domain (realm) is unused in credentials 
    AmB2ABCalleeSession(other_tag, connector) 
{
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT); 
  setDtmfDetectionEnabled(false);
}

b2b_connectCalleeSession::~b2b_connectCalleeSession() {

}

inline UACAuthCred* b2b_connectCalleeSession::getCredentials() {
  return &credentials;
}

void b2b_connectCalleeSession::onSipReply(const AmSipReply& reply,
					  AmSipDialog::Status old_dlg_status) {

  AmB2ABCalleeSession::onSipReply(reply, old_dlg_status);
 
  if ((old_dlg_status < AmSipDialog::Connected) &&
      (dlg->getStatus() == AmSipDialog::Disconnected)) {
    DBG("status change Pending -> Disconnected. Stopping session.\n");
    setStopped();
  }
}
