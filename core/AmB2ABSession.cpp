/*
 * Copyright (C) 2006-2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "AmB2ABSession.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "AmMediaProcessor.h"
#include "ampi/MonitoringAPI.h"

#include <assert.h>

AmB2ABSession::AmB2ABSession()
  : AmSession(), connector(NULL)
{
}

AmB2ABSession::AmB2ABSession(const string& other_local_tag)
  : AmSession(),
    other_id(other_local_tag)
{}


AmB2ABSession::~AmB2ABSession()
{
}

void AmB2ABSession::clear_other()
{
#if __GNUC__ < 3
  string cleared ("");
  other_id.assign (cleared, 0, 0);
#else
  other_id.clear();
#endif
}

void AmB2ABSession::process(AmEvent* event)
{
  B2ABEvent* b2b_e = dynamic_cast<B2ABEvent*>(event);
  if(b2b_e){
    onB2ABEvent(b2b_e);
    return;
  }

  AmSession::process(event);
}

void AmB2ABSession::onB2ABEvent(B2ABEvent* ev)
{
  switch(ev->event_id){

  case B2ABTerminateLeg:
    terminateLeg();
    break;
  }
}

void AmB2ABSession::relayEvent(AmEvent* ev)
{
  DBG("AmB2ABSession::relayEvent: id=%s\n",
      other_id.c_str());

  if(!other_id.empty())
    AmSessionContainer::instance()->postEvent(other_id,ev);
}

void AmB2ABSession::connectSession()
{
  if (!connector) {
    DBG("error - trying to connect session, but no connector!\n");
    return;
  }
  connector->connectSession(this);
  AmMediaProcessor::instance()->addSession(this, callgroup);
}

void AmB2ABSession::disconnectSession()
{
  if (!connector)
    return;
  
  connector->disconnectSession(this);
}

void AmB2ABSession::onBye(const AmSipRequest& req) {
  terminateOtherLeg();
  disconnectSession();
  setStopped();
}

void AmB2ABSession::terminateLeg()
{
  dlg->bye();
  disconnectSession();
  setStopped();
}

void AmB2ABSession::terminateOtherLeg()
{
  relayEvent(new B2ABEvent(B2ABTerminateLeg));
  clear_other();
}

AmB2ABCallerSession::AmB2ABCallerSession()
  : AmB2ABSession(),
    callee_status(None)
{
  // owned by us
  connector = new AmSessionAudioConnector();
}

AmB2ABCallerSession::~AmB2ABCallerSession()
{
  delete connector;
}

void AmB2ABCallerSession::onBeforeDestroy() {
  DBG("Waiting for release from callee session...\n");
  connector->waitReleased();
  DBG("OK, got release from callee session.\n");
}

void AmB2ABCallerSession::terminateOtherLeg()
{
  if (callee_status != None)
    AmB2ABSession::terminateOtherLeg();

  callee_status = None;
}

void AmB2ABCallerSession::onB2ABEvent(B2ABEvent* ev)
{

  switch(ev->event_id) {
  case B2ABConnectAudio: {
    callee_status = Connected;

    DBG("ConnectAudio event received from other leg\n");
    B2ABConnectAudioEvent* ca = 
      dynamic_cast<B2ABConnectAudioEvent*>(ev);
    if (!ca) 
      return;
		
    connectSession();

    return;
  } break;

  case B2ABConnectEarlyAudio: {
    callee_status = Early;

    DBG("ConnectEarlyAudio event received from other leg\n");
    B2ABConnectEarlyAudioEvent* ca = 
      dynamic_cast<B2ABConnectEarlyAudioEvent*>(ev);
    if (!ca)
      return;
		
    connectSession();

    return;
  } break;

  case B2ABConnectOtherLegException:
  case B2ABConnectOtherLegFailed: {
    WARN("looks like callee leg could not be created. terminating our leg.\n");
    terminateLeg();
    callee_status = None;
    return;
  } break;

  case B2ABOtherLegRinging: {
    DBG("callee_status set to Ringing.\n");
    callee_status = Ringing;
    return;
  } break;
	
  }   
 
  AmB2ABSession::onB2ABEvent(ev);
}

void AmB2ABCallerSession::connectCallee(const string& remote_party,
					const string& remote_uri,
					const string& local_party,
					const string& local_uri,
					const string& headers)
{
  if(callee_status != None)
    terminateOtherLeg();

  B2ABConnectLegEvent* ev = new B2ABConnectLegEvent(remote_party,remote_uri,
						    local_party,local_uri,
						    getLocalTag(), // callgroup
						    headers);  // extra headers

  relayEvent(ev);
  callee_status = NoReply;
}

void AmB2ABCallerSession::relayEvent(AmEvent* ev)
{
  if(other_id.empty()){
    B2ABConnectLegEvent* co_ev  = dynamic_cast<B2ABConnectLegEvent*>(ev);   
    if( co_ev ) {
      setupCalleeSession(createCalleeSession(), co_ev);
      if (other_id.length()) {
	MONITORING_LOG(getLocalTag().c_str(), "b2b_leg", other_id.c_str());
      }
    }
  }

  AmB2ABSession::relayEvent(ev);
}

void AmB2ABCallerSession::setupCalleeSession(AmB2ABCalleeSession* callee_session,
					     B2ABConnectLegEvent* ev) 
{

  if (NULL == callee_session)
    return;

  other_id = AmSession::getNewId();
  //  return;
  assert(callee_session);

  AmSipDialog* callee_dlg = callee_session->dlg;

  callee_dlg->setCallid(AmSession::getNewId());
  callee_dlg->setLocalTag(other_id);


  MONITORING_LOG(other_id.c_str(), 
		 "dir",  "out");

  callee_session->start();

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}

AmB2ABCalleeSession* AmB2ABCallerSession::createCalleeSession()
{
  return new AmB2ABCalleeSession(getLocalTag(), connector);
}

AmB2ABCalleeSession::AmB2ABCalleeSession(const string& other_local_tag, 
					 AmSessionAudioConnector* callers_connector)
  : AmB2ABSession(other_local_tag),
    is_connected(false)
{ 
  connector=callers_connector;
  connector->block();
}

AmB2ABCalleeSession::~AmB2ABCalleeSession() 
{
}

void AmB2ABCalleeSession::onBeforeDestroy() {
  DBG("releasing caller session.\n");
  connector->release();
  // now caller session is released
}


void AmB2ABCalleeSession::onB2ABEvent(B2ABEvent* ev)
{
  if(ev->event_id == B2ABConnectLeg){

    try {
      B2ABConnectLegEvent* co_ev = dynamic_cast<B2ABConnectLegEvent*>(ev);
      assert(co_ev);

      MONITORING_LOG4(getLocalTag().c_str(), 
		      "b2b_leg", other_id.c_str(),
		      "from",    co_ev->local_party.c_str(),
		      "to",      co_ev->remote_party.c_str(),
		      "ruri",    co_ev->remote_uri.c_str());

      dlg->setLocalParty(co_ev->local_party);
      dlg->setLocalUri(co_ev->local_uri);
			
      dlg->setRemoteParty(co_ev->remote_party);
      dlg->setRemoteUri(co_ev->remote_uri);

      setCallgroup(co_ev->callgroup);
			
      //setNegotiateOnReply(true);
      if (sendInvite(co_ev->headers)) {
	throw string("INVITE could not be sent\n");
      }

      return;
    } 
    catch(const AmSession::Exception& e){
      ERROR("%i %s\n",e.code,e.reason.c_str());
      relayEvent(new B2ABConnectOtherLegExceptionEvent(e.code,e.reason));
      setStopped();
    }
    catch(const string& err){
      ERROR("startSession: %s\n",err.c_str());
      relayEvent(new B2ABConnectOtherLegExceptionEvent(500,err));
      setStopped();
    }
    catch(...){
      ERROR("unexpected exception\n");
      relayEvent(new B2ABConnectOtherLegExceptionEvent(500,"unexpected exception"));
      setStopped();
    }
  }    

  AmB2ABSession::onB2ABEvent(ev);
}

void AmB2ABCalleeSession::onEarlySessionStart() {
  DBG("onEarlySessionStart of callee session\n");
  connectSession();
  is_connected = true;
  relayEvent(new B2ABConnectEarlyAudioEvent());
}


void AmB2ABCalleeSession::onSessionStart() {
  DBG("onSessionStart of callee session\n");
  if (!is_connected) {
    is_connected = true;
    DBG("call connectSession\n");
    connectSession();
  }
  relayEvent(new B2ABConnectAudioEvent());
}

void AmB2ABCalleeSession::onSipReply(const AmSipRequest& req, const AmSipReply& rep,
				     AmBasicSipDialog::Status old_dlg_status) {
  AmB2ABSession::onSipReply(req, rep, old_dlg_status);
  AmSipDialog::Status status = dlg->getStatus();
 
  if ((old_dlg_status == AmSipDialog::Trying) ||
      (old_dlg_status == AmSipDialog::Proceeding) ||
      (old_dlg_status == AmSipDialog::Early)) {

    if (status == AmSipDialog::Disconnected) {
	  
      DBG("callee session creation failed. notifying caller session.\n");
      DBG("this happened with reply: %d.\n", rep.code);
      relayEvent(new B2ABConnectOtherLegFailedEvent(rep.code, rep.reason));

    } else if (rep.code == 180) {
      relayEvent(new B2ABOtherLegRingingEvent());
    }
  }
}

// ----------------------- SessionAudioConnector -----------------

void AmSessionAudioConnector::connectSession(AmSession* sess) 
{
    const string& tag = sess->getLocalTag();

    tag_mut.lock();

    if (connected[0] && tag_sess[0] == tag) {
      // re-connect to position 0
      sess->setInOut(&audio_connectors[0],&audio_connectors[1]);
    } else if (connected[1] && tag_sess[1] == tag) {
      // re-connect to position 1
      sess->setInOut(&audio_connectors[1],&audio_connectors[0]);
    } else if(!connected[0]){
      // connect to empty position 0
      connected[0] = true;
      tag_sess[0] = "";
      tag_sess[0].append(tag);
      sess->setInOut(&audio_connectors[0],&audio_connectors[1]);
    }
    else if(!connected[1]){
      // connect to empty position 1
      connected[1] = true;
      tag_sess[1] = "";
      tag_sess[1].append(tag);
      sess->setInOut(&audio_connectors[1],&audio_connectors[0]);
    }
    else {
	ERROR("connector full!\n");
    }

    tag_mut.unlock();
}

bool AmSessionAudioConnector::disconnectSession(AmSession* sess) 
{
  bool res = true;

  const string& tag = sess->getLocalTag();

  tag_mut.lock();
  if (connected[0] && (tag_sess[0] == tag)) {
    tag_sess[0].clear();
    connected[0] = false;
    sess->setInOut(NULL, NULL);
    res = connected[1];
  } else if (connected[1] && (tag_sess[1] == tag)) {
    tag_sess[1].clear();
    connected[1] = false;
    sess->setInOut(NULL, NULL);
    res = connected[0];
  } else {
    DBG("disconnecting from wrong AmSessionAudioConnector\n");
  }
  tag_mut.unlock();

  return res;
}

/* mark as in use by not owning entity */
void AmSessionAudioConnector::block() {
  released.set(false);
}
  
/* mark as released by not owning entity */
void AmSessionAudioConnector::release() {
  released.set(true);
}

/* wait until released  */
void AmSessionAudioConnector::waitReleased() {
  released.wait_for();
}

