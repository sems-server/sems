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

#include "PinAuthConference.h"
#include "AmConferenceStatus.h"
#include "AmUtils.h"
#include "log.h"

#define APP_NAME "pinauthconference"

EXPORT_SESSION_FACTORY(PinAuthConferenceFactory,APP_NAME);

PinAuthConferenceFactory::PinAuthConferenceFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

string PinAuthConferenceFactory::DigitsDir;
PlayoutType PinAuthConferenceFactory::m_PlayoutType = ADAPTIVE_PLAYOUT;

int PinAuthConferenceFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  // get prompts
  AM_PROMPT_START;
  AM_PROMPT_ADD(FIRST_PARTICIPANT, ANNOUNCE_PATH "first_paricipant.wav");
  AM_PROMPT_ADD(JOIN_SOUND,        ANNOUNCE_PATH "beep.wav");
  AM_PROMPT_ADD(DROP_SOUND,        ANNOUNCE_PATH "beep.wav");
  AM_PROMPT_ADD(ENTER_PIN,         ANNOUNCE_PATH "enter_pin.wav");
  AM_PROMPT_ADD(WRONG_PIN,         ANNOUNCE_PATH "wrong_pin.wav");
  AM_PROMPT_ADD(ENTERING_CONFERENCE, ANNOUNCE_PATH "entering_conference.wav");
  AM_PROMPT_END(prompts, cfg, APP_NAME);

  DigitsDir = cfg.getParameter("digits_dir");
  if (DigitsDir.length() && DigitsDir[DigitsDir.length()-1]!='/')
    DigitsDir+='/';

  if (!DigitsDir.length()) {
    WARN("No digits_dir specified in configuration.\n");
  }
  for (int i=0;i<10;i++) 
    prompts.setPrompt(int2str(i), DigitsDir+int2str(i)+".wav", APP_NAME);

  string playout_type = cfg.getParameter("playout_type");
  if (playout_type == "simple") {
    m_PlayoutType = SIMPLE_PLAYOUT;
    DBG("Using simple (fifo) buffer as playout technique.\n");
  } else 	if (playout_type == "adaptive_jb") {
    m_PlayoutType = JB_PLAYOUT;
    DBG("Using adaptive jitter buffer as playout technique.\n");
  } else {
    DBG("Using adaptive playout buffer as playout technique.\n");
  }

  return 0;
}

// incoming calls
AmSession* PinAuthConferenceFactory::onInvite(const AmSipRequest&, const string& app_name,
					      const map<string,string>& app_params)
{
  return new PinAuthConferenceDialog(prompts);
}

// outgoing calls
AmSession* PinAuthConferenceFactory::onInvite(const AmSipRequest& req, const string& app_name,
					      AmArg& session_params)
{
  return new PinAuthConferenceDialog(prompts);
}

PinAuthConferenceDialog::PinAuthConferenceDialog(AmPromptCollection& prompts)
  : play_list(this), separator(this, 0), prompts(prompts), state(None)
{
  // set configured playout type
  RTPStream()->setPlayoutType(PinAuthConferenceFactory::m_PlayoutType);
}

PinAuthConferenceDialog::~PinAuthConferenceDialog()
{
  play_list.flush();
  prompts.cleanup((long)this);
}

void PinAuthConferenceDialog::connectConference(const string& room) {
  // set the conference id ('conference room') 
  conf_id = room;

  // disconnect in/out for safety 
  setInOut(NULL, NULL);

  // we need to be in the same callgroup as the other 
  // people in the conference (important if we have multiple
  // MediaProcessor threads
  changeCallgroup(conf_id);

  // get a channel from the status 
  channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag()));

  // clear the playlist
  play_list.flush();

  // add the channel to our playlist
  play_list.addToPlaylist(new AmPlaylistItem(channel.get(),
					     channel.get()));

  // set the playlist as input and output
  setInOut(&play_list,&play_list);
}

void PinAuthConferenceDialog::onSessionStart()
{ 
  state = EnteringPin;

  prompts.addToPlaylist(ENTER_PIN,  (long)this, play_list);

  // set the playlist as input and output
  setInOut(&play_list,&play_list);

  AmSession::onSessionStart();
}
 
void PinAuthConferenceDialog::onBye(const AmSipRequest& req)
{
  play_list.flush();
  setInOut(NULL,NULL);
  channel.reset(NULL);
  setStopped();
}

void PinAuthConferenceDialog::process(AmEvent* ev)
{
  // check conference events 
  ConferenceEvent* ce = dynamic_cast<ConferenceEvent*>(ev);
  if(ce && (conf_id == ce->conf_id)){
    switch(ce->event_id){

    case ConfNewParticipant: {
      DBG("########## new participant #########\n");
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
      connectConference(pin_str);
      DBG("connectConference. **********************\n");
    }    
  }
  // audio events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if (audio_ev  && 
      audio_ev->event_id == AmAudioEvent::noAudio) {
    DBG("received noAudio event. **********************\n");
    return;
  }

  AmSession::process(ev);
}

void PinAuthConferenceDialog::onDtmf(int event, int duration)
{
  DBG("PinAuthConferenceDialog::onDtmf: event %d duration %d\n", 
      event, duration);

  if (EnteringPin == state) {
    // not yet in conference
    if (event<10) {
      pin_str += int2str(event);
      DBG("added '%s': PIN is now '%s'.\n", 
	  int2str(event).c_str(), pin_str.c_str());
    } else if (event==10 || event==11) {
      // pound and star key
      // if required add checking of pin here...
      if (!pin_str.length()) {

	prompts.addToPlaylist(WRONG_PIN, (long)this, play_list, true);
      } else {
	state = EnteringConference;
	setInOut(NULL, NULL);
	play_list.flush();
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

