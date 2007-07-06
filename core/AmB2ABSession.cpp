/*
 * $Id: AmB2ABSession.cpp 145 2006-11-26 00:01:18Z sayer $
 *
 * Copyright (C) 2006-2007 iptego GmbH
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

#include "AmB2ABSession.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "AmMediaProcessor.h"

#include <assert.h>

AmB2ABSession::AmB2ABSession()
  : AmSession(),
    connector(NULL)
{
}

AmB2ABSession::AmB2ABSession(const string& other_local_tag)
  : other_id(other_local_tag),
    AmSession()
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
    if (!connector)
	return;

    connector->connectSession(this);
    AmMediaProcessor::instance()->addSession(this, callgroup);
}

void AmB2ABSession::disconnectSession()
{
    if (!connector)
	return;

    if (connector->disconnectSession(this)){
	delete connector;
	connector = NULL;
    }
}

void AmB2ABSession::onBye(const AmSipRequest& req) {
  terminateOtherLeg();
  disconnectSession();
  setStopped();
}

void AmB2ABSession::terminateLeg()
{
  dlg.bye();
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
}
AmB2ABCallerSession::~AmB2ABCallerSession()
{ }

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
    B2ABConnectAudioEvent* ca = dynamic_cast<B2ABConnectAudioEvent*>(ev);
    assert(ca);
		
    connector = ca->connector;
    connectSession();

    return;
  } break;

  case B2ABConnectOtherLegException:
  case B2ABConnectOtherLegFailed: {
    DBG("looks like callee leg could not be created. terminating our leg.\n");
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
					const string& local_uri)
{
  if(callee_status != None)
    terminateOtherLeg();

  B2ABConnectLegEvent* ev = new B2ABConnectLegEvent(remote_party,remote_uri,
						    local_party,local_uri,
						    getLocalTag());

  relayEvent(ev);
  callee_status = NoReply;
}

void AmB2ABCallerSession::relayEvent(AmEvent* ev)
{
  if(other_id.empty()){
    B2ABConnectLegEvent* co_ev  = dynamic_cast<B2ABConnectLegEvent*>(ev);   
    if( co_ev ) {
      setupCalleeSession(createCalleeSession(), co_ev);
    }
  }

  AmB2ABSession::relayEvent(ev);
}

void AmB2ABCallerSession::setupCalleeSession(AmB2ABCalleeSession* callee_session,
					     B2ABConnectLegEvent* ev) 
{
  other_id = AmSession::getNewId();
  //  return;
  assert(callee_session);

  AmSipDialog& callee_dlg = callee_session->dlg;
  callee_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;
  callee_dlg.local_tag    = other_id; 

  callee_session->start();

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}

AmB2ABCalleeSession* AmB2ABCallerSession::createCalleeSession()
{
  return new AmB2ABCalleeSession(getLocalTag());
}

AmB2ABCalleeSession::AmB2ABCalleeSession(const string& other_local_tag)
  : AmB2ABSession(other_local_tag)
{ }

AmB2ABCalleeSession::~AmB2ABCalleeSession() 
{ }

void AmB2ABCalleeSession::onB2ABEvent(B2ABEvent* ev)
{
  if(ev->event_id == B2ABConnectLeg){

    try {
      B2ABConnectLegEvent* co_ev = dynamic_cast<B2ABConnectLegEvent*>(ev);
      assert(co_ev);

      dlg.local_party  = co_ev->local_party;
      dlg.local_uri    = co_ev->local_uri;
			
      dlg.remote_party = co_ev->remote_party;
      dlg.remote_uri   = co_ev->remote_uri;
			
      setCallgroup(co_ev->callgroup);
			
      setNegotiateOnReply(true);
      sendInvite();	
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

void AmB2ABCalleeSession::onSessionStart(const AmSipReply& rep) {
  DBG("onSessionStart of callee session\n");
  // connect our audio
  connector = new AmSessionAudioConnector();
  connectSession();
  relayEvent(new B2ABConnectAudioEvent(connector));
}

// void AmB2ABCalleeSession::reconnectAudio() {
//   if (connector) {
//     connector->connectSession(this, true);
//     AmMediaProcessor::instance()->addSession(this, callgroup);
//   } else {
//     ERROR("can not connect audio of not connected session.\n");
//   }
// }

// void AmB2ABCallerSession::reconnectAudio() {
//   if (connector) {
//     connector->connectSession(this, false);
//     AmMediaProcessor::instance()->addSession(this, callgroup);
//   } else {
//     ERROR("can not connect audio of not connected session.\n");
//   }
// }

void AmB2ABCalleeSession::onSipReply(const AmSipReply& rep) {
  int status_before = dlg.getStatus();
  AmB2ABSession::onSipReply(rep);
  int status = dlg.getStatus();
 
  if ((status_before == AmSipDialog::Pending)&&
      (status == AmSipDialog::Disconnected)) {
	  
    DBG("callee session creation failed. notifying callersession.\n");
    relayEvent(new B2ABConnectOtherLegFailedEvent(rep.code, rep.reason));

  } else if ((status == AmSipDialog::Pending) && (rep.code == 180)) {
    relayEvent(new B2ABOtherLegRingingEvent());
  }
}


// ----------------------- SessionAudioConnector -----------------

void AmSessionAudioConnector::connectSession(AmSession* sess) 
{
    const string& tag = sess->getLocalTag();

    if(!tag_sess[0].length()){
	tag_sess[0] = tag;
	sess->setInOut(&audio_connectors[0],&audio_connectors[1]);
    }
    else if(!tag_sess[1].length()){
	tag_sess[1] = tag;
	sess->setInOut(&audio_connectors[1],&audio_connectors[0]);
    }
    else {
	ERROR("connector full!\n");
    }
}

bool AmSessionAudioConnector::disconnectSession(AmSession* sess) 
{
    const string& tag = sess->getLocalTag();

    if (tag_sess[0] == tag) {
	tag_sess[0].clear();
	sess->setInOut(NULL, NULL);
	return !tag_sess[1].length();
    }
    else if (tag_sess[1] == tag) {
	tag_sess[1].clear();
	sess->setInOut(NULL, NULL);
	return !tag_sess[0].length();
    }
    else {
	ERROR("disconnecting from wrong AmSessionAudioConnector\n");
    }
    
    return false;
}

// ----------------------- AudioDelayBridge -----------------
/** BRIDGE_DELAY is needed because of possible different packet sizes */ 
#define BRIDGE_DELAY 320 // 30ms 

/* AudioBridge */
AmAudioDelayBridge::AmAudioDelayBridge()
  : AmAudio(new AmAudioSimpleFormat(CODEC_PCM16))
{
  sarr.clear_all();
}

AmAudioDelayBridge::~AmAudioDelayBridge() { 
}

int AmAudioDelayBridge::write(unsigned int user_ts, unsigned int size) {  
  //	DBG("bridge write %u - this = %lu\n", user_ts + BRIDGE_DELAY, (unsigned long) this);
  sarr.write(user_ts + BRIDGE_DELAY, (short*) ((unsigned char*) samples), size >> 1); 
  return size; 
}

int AmAudioDelayBridge::read(unsigned int user_ts, unsigned int size) { 
  //	DBG("bridge read %u - this = %lu\n", user_ts, (unsigned long) this);
  sarr.read(user_ts, (short*) ((unsigned char*) samples), size >> 1); 
  return size;
}
