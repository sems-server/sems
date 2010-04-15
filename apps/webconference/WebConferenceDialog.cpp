/*
 * $Id$
 *
 * Copyright (C) 2007 iptego GmbH
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

#include "WebConferenceDialog.h"
#include "WebConference.h"
#include "AmUtils.h"
#include "AmMediaProcessor.h"

WebConferenceDialog::WebConferenceDialog(AmPromptCollection& prompts,
					 WebConferenceFactory* my_f,
					 UACAuthCred* cred)
  : play_list(this), separator(this, 0), prompts(prompts), state(None),
    factory(my_f), cred(cred), muted(false), 
    connect_ts(-1), disconnect_ts(-1) 
{
  is_dialout = (cred != NULL);
  accept_early_session = is_dialout;
  // set configured playout type
  RTPStream()->setPlayoutType(WebConferenceFactory::m_PlayoutType);
}

WebConferenceDialog::WebConferenceDialog(AmPromptCollection& prompts,
					 WebConferenceFactory* my_f,
					 const string& room)
  : play_list(this), separator(this, 0), prompts(prompts), state(None),
    factory(my_f), muted(false), 
    connect_ts(-1), disconnect_ts(-1)
{
  conf_id = room;
  DBG("set conf_id to %s\n", conf_id.c_str());
  is_dialout = false;
  // set configured playout type
  RTPStream()->setPlayoutType(WebConferenceFactory::m_PlayoutType);
}

WebConferenceDialog::~WebConferenceDialog()
{
  // provide statistics
  if ((connect_ts == -1)||(disconnect_ts == -1)) {
    factory->callStats(false, 0);
  } else {
    factory->callStats(true, disconnect_ts - connect_ts);
  }

  prompts.cleanup((long)this);
  play_list.close(false);
  if (is_dialout || (InConference == state)) {
    factory->updateStatus(is_dialout?dlg.user:conf_id, 
			  getLocalTag(), 
			  ConferenceRoomParticipant::Finished,
			  "");
  }
}

void WebConferenceDialog::connectConference(const string& room) {
  // set the conference id ('conference room') 
  conf_id = room;

  // disconnect in/out for safety 
  setInOut(NULL, NULL);

  // we need to be in the same callgroup as the other 
  // people in the conference (important if we have multiple
  // MediaProcessor threads
  changeCallgroup(conf_id);

  // get a channel from the status 
  if (channel.get() == NULL) 
    channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag()));
  else 
    AmConferenceStatus::postConferenceEvent(conf_id,
					    ConfNewParticipant,getLocalTag());

  // clear the playlist
  play_list.close();

  // add the channel to our playlist
  play_list.addToPlaylist(new AmPlaylistItem(channel.get(),
					     channel.get()));

  // set the playlist as output and input 
  if (muted)
    setInOut(NULL, &play_list);  
  else
    setInOut(&play_list,&play_list);

}

// dial-in
void WebConferenceDialog::onSessionStart(const AmSipRequest& req) { 
  time(&connect_ts);

  // direct room access?
  if (conf_id.empty()) {
    state = EnteringPin;    
    prompts.addToPlaylist(ENTER_PIN,  (long)this, play_list);
    // set the playlist as input and output
    setInOut(&play_list,&play_list);
  } else {
    DBG("########## direct connect conference #########\n"); 
    factory->newParticipant(conf_id, 
			    getLocalTag(), 
			    dlg.remote_party);
    factory->updateStatus(conf_id, 
			  getLocalTag(), 
			  ConferenceRoomParticipant::Connected,
			  "direct access: entered");
    state = InConference;
    connectConference(conf_id);
  }
}

void WebConferenceDialog::onRinging(const AmSipReply& rep) { 
  if (None == state) {
    DBG("########## dialout: connect ringing session to conference '%s'  #########\n", 
	dlg.user.c_str()); 
    state = InConferenceRinging;
    connectConference(dlg.user);
  
    if(!RingTone.get())
      RingTone.reset(new AmRingTone(0,2000,4000,440,480)); // US
    
    setLocalInput(RingTone.get());
    setAudioLocal(AM_AUDIO_IN, true);
    setAudioLocal(AM_AUDIO_OUT, true);
  }
}

void WebConferenceDialog::onEarlySessionStart(const AmSipReply& rep) { 
  if ((None == state) || (InConferenceRinging == state)) {
    state = InConferenceEarly;
    DBG("########## dialout: connect early session to conference '%s'  #########\n", 
	dlg.user.c_str()); 
    setAudioLocal(AM_AUDIO_IN, false);
    setAudioLocal(AM_AUDIO_OUT, false);
    connectConference(dlg.user);
    setMute(true);
  }
}

void WebConferenceDialog::onSessionStart(const AmSipReply& rep) { 
  time(&connect_ts);
  setMute(false);
  DBG("########## dialout: connect to conference '%s' #########\n",
      dlg.user.c_str()); 
  state = InConference;
  setAudioLocal(AM_AUDIO_IN, false);
  setAudioLocal(AM_AUDIO_OUT, false);
  connectConference(dlg.user);
}

void WebConferenceDialog::onSipReply(const AmSipReply& reply) {
  int status = dlg.getStatus();

  AmSession::onSipReply(reply);

  if ((status < AmSipDialog::Connected) && 
      (dlg.getStatus() == AmSipDialog::Disconnected)) {
    DBG("Call failed.\n");
    setStopped();
  }

  // update status to map
  if (is_dialout) {
    // map AmSipDialog state to WebConferenceState
    ConferenceRoomParticipant::ParticipantStatus rep_st = 
      ConferenceRoomParticipant::Connecting;
    switch (dlg.getStatus()) {
    case AmSipDialog::Pending: {
      rep_st = ConferenceRoomParticipant::Connecting;
      if (reply.code == 180) 
	rep_st  = ConferenceRoomParticipant::Ringing;
    } break;
    case AmSipDialog::Connected: 
      rep_st = ConferenceRoomParticipant::Connected; break;
    case AmSipDialog::Disconnecting: 
      rep_st = ConferenceRoomParticipant::Disconnecting; break;    
    case AmSipDialog::Disconnected: 
      rep_st = ConferenceRoomParticipant::Finished; break;    
    }
    DBG("is dialout: updateing status\n");
    factory->updateStatus(dlg.user, getLocalTag(), 
			  rep_st, int2str(reply.code) + " " + reply.reason);
  }
}
 
void WebConferenceDialog::onBye(const AmSipRequest& req)
{
  if (InConference == state) {
    factory->updateStatus(conf_id, 
			  getLocalTag(), 
			  ConferenceRoomParticipant::Disconnecting,
			  req.method);
  }

  disconnectConference();
}

void WebConferenceDialog::disconnectConference() {
  play_list.close();
  setInOut(NULL,NULL);
  channel.reset(NULL);
  setStopped();
  time(&disconnect_ts);
}

void WebConferenceDialog::process(AmEvent* ev)
{
  // check conference events 
  ConferenceEvent* ce = dynamic_cast<ConferenceEvent*>(ev);
  if(ce && (conf_id == ce->conf_id)){
    switch(ce->event_id){

    case ConfNewParticipant: {
      DBG("########## new participant (%d) #########\n", ce->participants);
      if(ce->participants == 1){
	prompts.addToPlaylist(FIRST_PARTICIPANT, (long)this, play_list, true);
      } else {
	prompts.addToPlaylist(JOIN_SOUND, (long)this, play_list, true);
      }
    } break;
    
    case ConfParticipantLeft: {
      DBG("########## participant left ########\n");
      prompts.addToPlaylist(DROP_SOUND, (long)this, play_list, true);
    } break;

    default:
      break;
    }
    return;
  }

  // our item will fire this event
  AmPlaylistSeparatorEvent* sep_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(ev);
  if (NULL != sep_ev) {
    // don't care for the id here
    if (EnteringConference == state) {
      state = InConference;
      DBG("########## connectConference after pin entry #########\n");
      connectConference(pin_str);
      factory->newParticipant(pin_str, 
			      getLocalTag(), 
			      dlg.remote_party);
      factory->updateStatus(pin_str, 
			    getLocalTag(), 
			    ConferenceRoomParticipant::Connected,
			    "entered");
    }    
  }
  // audio events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if (audio_ev  && 
      audio_ev->event_id == AmAudioEvent::noAudio) {
    DBG("########## noAudio event #########\n");
    return;
  }

  WebConferenceEvent* webconf_ev = dynamic_cast<WebConferenceEvent*>(ev);
  if (NULL != webconf_ev) {
    switch(webconf_ev->event_id) {
    case WebConferenceEvent::Kick:  onKicked(); break;
    case WebConferenceEvent::Mute:  onMuted(true); break;
    case WebConferenceEvent::Unmute: onMuted(false); break;
    default: { WARN("ignoring unknown webconference event %d\n", webconf_ev->event_id); 
    } break;	
    }
  }
  
  AmSession::process(ev);
}

void WebConferenceDialog::onDtmf(int event, int duration)
{
  DBG("WebConferenceDialog::onDtmf: event %d duration %d\n", 
      event, duration);

  if (EnteringPin == state) {
    // not yet in conference
    if (event<10) {
      pin_str += int2str(event);
      DBG("added '%s': PIN is now '%s'.\n", 
	  int2str(event).c_str(), pin_str.c_str());
      play_list.close(false);
    } else if (event==10 || event==11) {
      // pound and star key
      // if required add checking of pin here...
      if (!pin_str.length()) {

	prompts.addToPlaylist(WRONG_PIN, (long)this, play_list, true);
      } else {
	state = EnteringConference;
	setInOut(NULL, NULL);
	play_list.close();
	for (size_t i=0;i<pin_str.length();i++) {
	  string num = "";
	  num[0] = pin_str[i];
	  DBG("adding '%s' to playlist.\n", num.c_str());

	  prompts.addToPlaylist(num,
				(long)this, play_list);
	}

       	setInOut(&play_list,&play_list);
	prompts.addToPlaylist(ENTERING_CONFERENCE,
			      (long)this, play_list);
	play_list.addToPlaylist(new AmPlaylistItem(&separator, NULL));
      }
    }
  }
}


void WebConferenceDialog::onKicked() {
  DBG("########## WebConference::onKick #########\n");
  dlg.bye(); 
  disconnectConference();
  factory->updateStatus(conf_id, 
			getLocalTag(), 
			ConferenceRoomParticipant::Disconnecting,
			"disconnect");
}

void WebConferenceDialog::onMuted(bool mute) {
  DBG("########## WebConference::onMuted('%s') #########\n",
      mute?"true":"false");

  if (muted != mute) {
    muted = mute;
    switch (state) {

    case InConference:
    case InConferenceEarly: {
      if (muted)
	setInOut(NULL, &play_list);  
      else 
	setInOut(&play_list, &play_list);  
    } break;
      
    case InConferenceRinging: {
      if (muted) {
	setLocalInOut(NULL, NULL);
      } else {
	if(!RingTone.get())
	  RingTone.reset(new AmRingTone(0,2000,4000,440,480)); // US
    
	setLocalInOut(RingTone.get(), NULL);
	if (getDetached())
	  AmMediaProcessor::instance()->addSession(this,
						   callgroup); 
      }
    } break;
    default: DBG("No default action for changing mute status.\n"); break;
	
    }
  }
}

