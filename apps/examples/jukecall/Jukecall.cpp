/*
 * Copyright (C) 2006 iptego GmbH
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

#include "Jukecall.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "sems.h"
#include "log.h"

#define MOD_NAME "jukecall"

EXPORT_SESSION_FACTORY(JukecallFactory,MOD_NAME);

// you can replace this with configurable file
#define INITIAL_ANNOUNCEMENT "../apps/examples/jukecall/wav/greeting.wav"
#define JUKE_DIR             "../apps/examples/jukecall/wav/"

JukecallFactory::JukecallFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}


int JukecallFactory::onLoad()
{
  // read configuration

  DBG("JukecallFactory loaded.\n");
  return 0;
}

AmSession* JukecallFactory::onInvite(const AmSipRequest& req, const string& app_name,
				     const map<string,string>& app_params)
{
  if (req.user.length() <= 3) {
    throw AmSession::Exception(403, "Need a number to call");
  }

  JukecallSession* dlg = new JukecallSession();

  return dlg;
}

JukecallSession::JukecallSession() 
  : AmB2ABCallerSession(), state(JC_none)
{
}

JukecallSession::~JukecallSession() 
{
}

void JukecallSession::onSessionStart()
{
  if (state != JC_none) {
    // reinvite
    AmB2ABCallerSession::onSessionStart();
    return;
  }

  DBG("-----------------------------------------------------------------\n");
  DBG("playing file\n");

  if(initial_announcement.open(INITIAL_ANNOUNCEMENT,AmAudioFile::Read)) {
    dlg->bye();
    throw string("CTConfDDialog::onSessionStart: Cannot open file '%s'\n", INITIAL_ANNOUNCEMENT);
  }

  // set this as our output
  setOutput(&initial_announcement);

  state = JC_initial_announcement;

  AmB2ABCallerSession::onSessionStart();
}

void JukecallSession::onDtmf(int event, int duration_msec) {
  DBG("got DTMF %d\n", event);

  // no jukebox if other party is not connected
  if (getCalleeStatus()!=AmB2ABCallerSession::Connected)
    return;

  // no jukebox in the beginning and while playing
  if (state != JC_connect) 
    return;

  DBG("playing back file...\n");

  song.reset(new AmAudioFile());
  if (song->open(JUKE_DIR+int2str(event)+".wav",AmAudioFile::Read)) {
    ERROR("could not open file\n");
    return;
  }
  setOutput(song.get());
  state = JC_juke;

  relayEvent(new JukeEvent(event));
}

void JukecallSession::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    switch(state) {
    case JC_initial_announcement: {
      state = JC_connect;
      string callee = "sip:" + dlg->user.substr(3) + "@" + dlg->domain;
      DBG("-------------------------- connecting %s ------------------------\n", callee.c_str());
      connectCallee(callee, callee, 
		    dlg->remote_party, dlg->remote_uri);

      return;

    } break;

    case JC_juke: {
      DBG("reconnecting audio\n");
      connectSession();
      state = JC_connect;
      return;
    }; break;

    default: {
      DBG("cleared in other state.\n");
      return;
    };
    }
		
  }

  AmB2ABCallerSession::process(event);
}

AmB2ABCalleeSession* JukecallSession::createCalleeSession() {
  AmB2ABCalleeSession* sess = new JukecalleeSession(getLocalTag(), connector);
  return sess;
}

JukecalleeSession::JukecalleeSession(const string& other_tag, 
				     AmSessionAudioConnector* connector) 
  : AmB2ABCalleeSession(other_tag, connector)
{
  setDtmfDetectionEnabled(false);
}


void JukecalleeSession::process(AmEvent* event) {
  JukeEvent* juke_event = dynamic_cast<JukeEvent*>(event);
  if(juke_event) {
    song.reset(new AmAudioFile());
    if (song->open(JUKE_DIR+int2str(event->event_id)+".wav",AmAudioFile::Read)) {
      ERROR("could not open file\n");
      return;
    }
    setOutput(song.get());
    return;
  }

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    DBG("reconnecting audio\n");
    connectSession();
    return;
  }
	
  AmB2ABCalleeSession::process(event);
}
