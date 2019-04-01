/*
 * Copyright (C) 2007 iptego GmbH
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

#include "WebConferenceDialog.h"
#include "WebConference.h"
#include "AmUtils.h"
#include "AmMediaProcessor.h"

#define LONELY_USER_TIMER_ID 50

// room name unknown
WebConferenceDialog::WebConferenceDialog(AmPromptCollection& prompts,
					 WebConferenceFactory* my_f,
					 UACAuthCred* cred)
  : play_list(this), separator(this, 0), prompts(prompts), state(None),
    factory(my_f), cred(cred), muted(false), 
    connect_ts(-1), disconnect_ts(-1),
    local_input(NULL)
{
  // with SIP credentials -> outgoing ('dialout') call
  is_dialout = (cred != NULL);
  accept_early_session = is_dialout;
  // set configured playout type
  RTPStream()->setPlayoutType(WebConferenceFactory::m_PlayoutType);
}

// room name known
WebConferenceDialog::WebConferenceDialog(AmPromptCollection& prompts,
					 WebConferenceFactory* my_f,
					 const string& room)
  : play_list(this), separator(this, 0), prompts(prompts), state(None),
    factory(my_f), cred(NULL),
    muted(false), connect_ts(-1),
    disconnect_ts(-1), local_input(NULL),
    lonely_user(true)
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
  play_list.flush();
  if (is_dialout || (InConference == state)) {
    factory->updateStatus(is_dialout?dlg->getUser():conf_id, 
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
    channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag(),RTPStream()->getSampleRate()));
  else 
    AmConferenceStatus::postConferenceEvent(conf_id,
					    ConfNewParticipant,getLocalTag());

  // clear the playlist
  play_list.flush();

  // add the channel to our playlist
  play_list.addToPlaylist(new AmPlaylistItem(channel.get(), channel.get()));

  // set the playlist as output and input 
  if (muted)
    setInOut(NULL, &play_list);
  else
    setInOut(&play_list, &play_list);
}

void WebConferenceDialog::onInvite(const AmSipRequest& req) { 
  if (state == None) {
    if (WebConferenceFactory::participant_id_paramname.length()) {
      string appparams = getHeader(req.hdrs, PARAM_HDR);
      if (appparams.length()) {
	participant_id = get_header_param(appparams,
					  WebConferenceFactory::participant_id_paramname);
      }
    } else if (WebConferenceFactory::participant_id_hdr.length()) {
      participant_id = getHeader(req.hdrs, WebConferenceFactory::participant_id_hdr);
    }

    if (participant_id.empty()) {
      DBG("no Participant ID set\n");
    } else {
      DBG("Participant ID set to '%s'\n", participant_id.c_str());
    }
  }

  AmSession::onInvite(req);
}

void WebConferenceDialog::onSessionStart() {
  DBG("WebConferenceDialog::onSessionStart (state = %d)\n", state);

  if (None == state || InConferenceRinging == state || InConferenceEarly == state) {

    // set the playlist as input and output
    setInOut(&play_list,&play_list);

    if (!is_dialout) {
      // direct room access?
      if (conf_id.empty()) {
	state = EnteringPin;
	prompts.addToPlaylist(ENTER_PIN,  (long)this, play_list);
      } else {
	DBG("########## direct connect conference '%s'  #########\n", conf_id.c_str());

	string orig_pin_str = conf_id;
	pin_str = conf_id;

	if (WebConferenceFactory::room_pin_split) {
	  if (pin_str.length() <= WebConferenceFactory::room_pin_split_pos) {
	    DBG("short conference room/pin combination ('%s', want at least %d)\n",
		pin_str.c_str(), WebConferenceFactory::room_pin_split_pos);
	    setInOut(&play_list,&play_list);
	    play_list.flush();
	    prompts.addToPlaylist(WRONG_PIN, (long)this, play_list);
	    pin_str ="";
	    return;
	  }

	  participant_id = pin_str.substr(WebConferenceFactory::room_pin_split_pos);
	  conf_id = pin_str.substr(0, WebConferenceFactory::room_pin_split_pos);
	  DBG("split entered pin into room '%s' and PIN '%s'\n", conf_id.c_str(), participant_id.c_str());
	}

	if (!factory->newParticipant(conf_id, getLocalTag(), dlg->getRemoteParty(),
				     participant_id)) {
	  DBG("inexisting conference room '%s\n", conf_id.c_str());
	  state = PlayErrorFinish;

	  prompts.addToPlaylist(WRONG_PIN_BYE, (long)this, play_list);
	} else {
	  factory->updateStatus(conf_id, getLocalTag(), ConferenceRoomParticipant::Connected,
				"direct access: entered");
	  state = InConference;
	  time(&connect_ts);
	  connectConference(conf_id);
	}
      }
    } else {
      setMute(false);
      DBG("########## dialout: connect to conference '%s' #########\n", dlg->getUser().c_str()); 
      state = InConference;
      setLocalInput(NULL);
      time(&connect_ts);
      connectConference(dlg->getUser());
    }
  }

  AmSession::onSessionStart();
}

void WebConferenceDialog::onRinging(const AmSipReply& rep) { 
  if (None == state || InConferenceEarly == state) {
    DBG("########## dialout: connect ringing session to conference '%s'  #########\n", 
	dlg->getUser().c_str()); 

    if(!RingTone.get())
      RingTone.reset(new AmRingTone(0,2000,4000,440,480)); // US

    setLocalInput(RingTone.get());

    if (None == state) {
      connectConference(dlg->getUser());
    }
    state = InConferenceRinging;
  }
  AmSession::onRinging(rep);
}

void WebConferenceDialog::onEarlySessionStart() { 
  if (None == state || InConferenceRinging == state) {

    DBG("########## dialout: connect early session to conference '%s'  #########\n", 
	dlg->getUser().c_str());
    setLocalInput(NULL);
    if (None == state) {
      connectConference(dlg->getUser());
    }
    //    setMute(true);

    state = InConferenceEarly;
  }

  AmSession::onEarlySessionStart();
}

void WebConferenceDialog::onSipReply(const AmSipRequest& req,
				     const AmSipReply& reply, 
				     AmBasicSipDialog::Status old_dlg_status)
{
  AmSession::onSipReply(req,reply,old_dlg_status);

  DBG("reply: %u %s, old_dlg_status = %s, status = %s\n",
      reply.code, reply.reason.c_str(),
      AmBasicSipDialog::getStatusStr(old_dlg_status),
      dlg->getStatusStr());

  if ((old_dlg_status < AmSipDialog::Connected) && 
      (dlg->getStatus() == AmSipDialog::Disconnected)) {
    DBG("Call failed.\n");
    setStopped();
  }

  // update status to map
  if (is_dialout) {
    // map AmSipDialog state to WebConferenceState
    ConferenceRoomParticipant::ParticipantStatus rep_st = 
      ConferenceRoomParticipant::Connecting;
    switch (dlg->getStatus()) {
    case AmSipDialog::Trying:
    case AmSipDialog::Proceeding:
    case AmSipDialog::Early:
      {
      rep_st = ConferenceRoomParticipant::Connecting;
      if (reply.code == 180 || reply.code == 183) 
	rep_st  = ConferenceRoomParticipant::Ringing;
    } break;
    case AmSipDialog::Connected: 
      rep_st = ConferenceRoomParticipant::Connected; break;
    case AmSipDialog::Cancelling:
    case AmSipDialog::Disconnecting: 
      rep_st = ConferenceRoomParticipant::Disconnecting; break;    
    case AmSipDialog::Disconnected: 
      rep_st = ConferenceRoomParticipant::Finished; break;
    default:break;
    }
    DBG("is dialout: updateing status\n");
    factory->updateStatus(dlg->getUser(), getLocalTag(), 
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

void WebConferenceDialog::onRtpTimeout() {
  DBG("RTP timeout, removing from conference\n");
  disconnectConference();
  AmSession::onRtpTimeout();
}

void WebConferenceDialog::onSessionTimeout() {
  DBG("Session Timer: Timeout, removing from conference.\n");
  disconnectConference();
  AmSession::onSessionTimeout();
}

void WebConferenceDialog::disconnectConference() {
  play_list.flush();
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
	prompts.addToPlaylist(FIRST_PARTICIPANT, (long)this, play_list, true,
			      WebConferenceFactory::LoopFirstParticipantPrompt);

	if (WebConferenceFactory::LonelyUserTimer) {
	  DBG("only person in the room - setting LonelyUserTimer %u sec\n",
	      WebConferenceFactory::LonelyUserTimer);
	  setTimer(LONELY_USER_TIMER_ID, WebConferenceFactory::LonelyUserTimer);
	  lonely_user = true;
	}

      } else {
	// someone else joined
	lonely_user = false;

	if (WebConferenceFactory::LoopFirstParticipantPrompt) {
	  // -- reset the channel (flush playlist)
	  // get a channel from the status 
	  if (channel.get() == NULL) 
	    channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag(),RTPStream()->getSampleRate()));

	  // clear the playlist
	  play_list.flush();

	  // add the channel to our playlist
	  play_list.addToPlaylist(new AmPlaylistItem(channel.get(), channel.get()));
	}

	prompts.addToPlaylist(JOIN_SOUND, (long)this, play_list, true);
      }
    } break;
    
    case ConfParticipantLeft: {
      DBG("########## participant left ########\n");
      prompts.addToPlaylist(DROP_SOUND, (long)this, play_list, true);

      if(ce->participants == 1) {
	if (WebConferenceFactory::LonelyUserTimer) {
	  DBG("only person in the room - setting LonelyUserTimer %u sec\n",
	      WebConferenceFactory::LonelyUserTimer);
	  setTimer(LONELY_USER_TIMER_ID, WebConferenceFactory::LonelyUserTimer);
	  lonely_user = true;
	}
      }

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

      if (!factory->newParticipant(pin_str, getLocalTag(), dlg->getRemoteParty(),
				   participant_id)) {
	DBG("inexisting conference room '%s' or pin wrong\n", pin_str.c_str());
	state = PlayErrorFinish;
	setInOut(&play_list,&play_list);
	prompts.addToPlaylist(WRONG_PIN_BYE, (long)this, play_list);
	return;
      }

      time(&connect_ts);
      connectConference(pin_str);
      factory->updateStatus(pin_str, getLocalTag(), ConferenceRoomParticipant::Connected,
			    "entered");
    }
  }

  // audio events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if (audio_ev  && 
      audio_ev->event_id == AmAudioEvent::noAudio) {
    DBG("########## noAudio event #########\n");
    if (PlayErrorFinish == state) {
      DBG("Finished playing bye message, ending call.\n");
      dlg->bye();
      setStopped();
      return;
    }

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


  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    if (timer_id == LONELY_USER_TIMER_ID && lonely_user) {
      DBG("LonelyUserTimer of %u sec expired - kicking lonely user\n",
	  WebConferenceFactory::LonelyUserTimer);
      onKicked();
      return;
    }
  }
  
  AmSession::process(ev);
}

void WebConferenceDialog::onDtmf(int event, int duration) {
  DBG("WebConferenceDialog::onDtmf: event %d duration %d\n", event, duration);

  if (EnteringPin == state) {
    // not yet in conference
    if (event<10) {
      pin_str += int2str(event);
      DBG("added '%s': PIN is now '%s'.\n", 
	  int2str(event).c_str(), pin_str.c_str());
      play_list.flush();
    } else if (event==10 || event==11) {
      // pound and star key
      if (!pin_str.length()) {
	prompts.addToPlaylist(WRONG_PIN, (long)this, play_list, true);
	return;
      }
      string orig_pin_str = pin_str;

      if (WebConferenceFactory::room_pin_split) {
	if (pin_str.length() <= WebConferenceFactory::room_pin_split_pos) {
	  DBG("short conference room/pin combination ('%s', want at least %d)\n",
	      pin_str.c_str(), WebConferenceFactory::room_pin_split_pos);
	  setInOut(&play_list,&play_list);
	  play_list.flush();
	  prompts.addToPlaylist(WRONG_PIN, (long)this, play_list);
	  pin_str ="";
	  return;
	}

	participant_id = pin_str.substr(WebConferenceFactory::room_pin_split_pos);
	pin_str = pin_str.substr(0, WebConferenceFactory::room_pin_split_pos);
	DBG("split entered pin into room '%s' and PIN '%s'\n", pin_str.c_str(), participant_id.c_str());
      }

      if (!factory->isValidConference(pin_str, WebConferenceFactory::room_pin_split ?
				      participant_id : "")) {
	setInOut(&play_list,&play_list);
	play_list.flush();
	prompts.addToPlaylist(WRONG_PIN, (long)this, play_list);
	pin_str ="";
	return;	
      }

      state = EnteringConference;
      setInOut(NULL, NULL);
      play_list.flush();
      for (size_t i=0;i<orig_pin_str.length();i++) {
	string num = "";
	num[0] = orig_pin_str[i];
	DBG("adding '%s' to playlist.\n", num.c_str());
	prompts.addToPlaylist(num, (long)this, play_list);
      }

      setInOut(&play_list,&play_list);
      prompts.addToPlaylist(ENTERING_CONFERENCE, (long)this, play_list);
      play_list.addToPlaylist(new AmPlaylistItem(&separator, NULL));

    }
  }
}


void WebConferenceDialog::onKicked() {
  DBG("########## WebConference::onKick #########\n");
  dlg->bye(); 
  disconnectConference();
  factory->updateStatus(conf_id, getLocalTag(), ConferenceRoomParticipant::Disconnecting,
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
	setLocalInput(NULL);
      } else {
	if(!RingTone.get())
	  RingTone.reset(new AmRingTone(0,2000,4000,440,480)); // US
    
	setLocalInput(RingTone.get());
	if (isDetached())
	  AmMediaProcessor::instance()->addSession(this, callgroup); 
      }
    } break;
    default: DBG("No default action for changing mute status.\n"); break;
	
    }
  }
}

void WebConferenceDialog::setLocalInput(AmAudio* in)
{
  lockAudio();
  local_input = in;
  unlockAudio();
}

int WebConferenceDialog::readStreams(unsigned long long ts, unsigned char *buffer) 
{ 
  int res = 0;
  lockAudio();

  AmRtpAudio *stream = RTPStream();
  unsigned int f_size = stream->getFrameSize();
  if (stream->checkInterval(ts)) {
    int got = 0;
    if (local_input) got = local_input->get(ts, buffer, stream->getSampleRate(), f_size);
    else got = stream->get(ts, buffer, stream->getSampleRate(), f_size);
    if (got < 0) res = -1;
    if (got > 0) {
      if (isDtmfDetectionEnabled())
        putDtmfAudio(buffer, got, ts);

      if (input) res = input->put(ts, buffer, stream->getSampleRate(), got);
    }
  }
  
  unlockAudio();
  return res;
}

bool WebConferenceDialog::isAudioSet()
{
  lockAudio();
  bool set = input || output || local_input;
  unlockAudio();
  return set;
}

void WebConferenceDialog::clearAudio()
{
  lockAudio();
  if (local_input) {
    local_input->close();
    local_input = NULL;
  }
  unlockAudio();
  AmSession::clearAudio(); // locking second time but called not so often?
}

