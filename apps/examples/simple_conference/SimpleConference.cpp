/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "SimpleConference.h"
#include "AmConferenceStatus.h"

#include "log.h"

#define APP_NAME "simple_conference"

EXPORT_SESSION_FACTORY(SimpleConferenceFactory,APP_NAME);

SimpleConferenceFactory::SimpleConferenceFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int SimpleConferenceFactory::onLoad()
{
  return 0;
}

// incoming calls - req is INVITE
AmSession* SimpleConferenceFactory::onInvite(const AmSipRequest& req)
{
  return new SimpleConferenceDialog();
}

// outgoing calls - rep is 200 class response to INVITE
AmSession* SimpleConferenceFactory::onInvite(const AmSipReply& rep)
{
  return new SimpleConferenceDialog();
}

SimpleConferenceDialog::SimpleConferenceDialog()
  : play_list(this)
{
  // use adaptive playout - its the best method around
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
}

SimpleConferenceDialog::~SimpleConferenceDialog()
{
  // clean playlist items
  play_list.close(false);
}

void SimpleConferenceDialog::onSessionStart(const AmSipRequest& req)
{
  // set the conference id ('conference room') to user part of ruri
  conf_id = dlg.user;

  // open the beep file
  BeepSound.reset(new AmAudioFile());
  if(BeepSound->open(BEEP_FILE_NAME, AmAudioFile::Read)) {
    BeepSound.reset(0);	  
  }

  // get a channel from the status 
  channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag()));
  
  // add the channel to our playlist
  play_list.addToPlaylist(new AmPlaylistItem(channel.get(),
					     channel.get()));
  
  // set the playlist as input and output
  setInOut(&play_list,&play_list);

  // we need to be in the same callgroup as the other 
  // people in the conference (important if we have multiple
  // MediaProcessor threads
  setCallgroup(conf_id);
}

void SimpleConferenceDialog::onBye(const AmSipRequest& req)
{
  play_list.close();
  setInOut(NULL,NULL);
  channel.reset(NULL);
  setStopped();
}

void SimpleConferenceDialog::process(AmEvent* ev)
{
  // check conference events 
  ConferenceEvent* ce = dynamic_cast<ConferenceEvent*>(ev);
  if(ce && (conf_id == ce->conf_id)){
    switch(ce->event_id){

    case ConfNewParticipant: {
      if(BeepSound.get()){
	// rewind in case we already played it
	BeepSound->rewind();
	// add to front of playlist - after the file is played 
	// we will be connected again to conference channel
	play_list.addToPlayListFront(new AmPlaylistItem(BeepSound.get(), NULL));
      }  
    } break;
    
    case ConfParticipantLeft: {
      if(BeepSound.get()){
	BeepSound->rewind();
	play_list.addToPlayListFront(new AmPlaylistItem( BeepSound.get(), NULL));
      }  
    } break;

    default:
      break;
    }
    return;
  }

  AmSession::process(ev);
}

void SimpleConferenceDialog::onDtmf(int event, int duration)
{
  DBG("SimpleConferenceDialog::onDtmf: event %d duration %d\n", 
      event, duration);
}

