/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmConferenceStatus.h"
#include "AmConferenceChannel.h"
#include "AmMultiPartyMixer.h"
#include "AmSessionContainer.h"

#include "AmAudio.h"
#include "log.h"

#include <assert.h>
#include <unistd.h>

std::map<std::string,AmConferenceStatus*> AmConferenceStatus::cid2status;
AmMutex                         AmConferenceStatus::cid2s_mut;

//
// static methods
//
AmConferenceChannel* AmConferenceStatus::getChannel(const string& cid, 
						    const string& local_tag, int input_sample_rate)
{
  AmConferenceStatus*  st = 0;
  AmConferenceChannel* ch = 0;

  cid2s_mut.lock();
  std::map<std::string,AmConferenceStatus*>::iterator it = cid2status.find(cid);

  if(it != cid2status.end()){

    st = it->second;
  }
  else {
	
    st = new AmConferenceStatus(cid);
    cid2status[cid] = st;
  }

  ch = st->getChannel(local_tag, input_sample_rate);
  cid2s_mut.unlock();

  return ch;
}

size_t AmConferenceStatus::getConferenceSize(const string& cid) {

  cid2s_mut.lock();
  std::map<std::string,AmConferenceStatus*>::iterator it = cid2status.find(cid);

  size_t res = 0;
  if(it != cid2status.end())
    res = it->second->channels.size();

  cid2s_mut.unlock();

  return res;
}

void AmConferenceStatus::postConferenceEvent(const string& cid, 
					     int event_id, const string& sess_id) {
  AmConferenceStatus*  st = 0;

  cid2s_mut.lock();
  std::map<std::string,AmConferenceStatus*>::iterator it = cid2status.find(cid);

  if(it != cid2status.end()){

    st = it->second;
  }
  else {
	
    st = new AmConferenceStatus(cid);
    cid2status[cid] = st;
  }

  st->postConferenceEvent(event_id, sess_id);
  cid2s_mut.unlock();
}

void AmConferenceStatus::releaseChannel(const string& cid, unsigned int ch_id)
{
  cid2s_mut.lock();
  std::map<std::string,AmConferenceStatus*>::iterator it = cid2status.find(cid);

  if(it != cid2status.end()){

    AmConferenceStatus* st = it->second;
    if(!st->releaseChannel(ch_id)){
      cid2status.erase(it);
      delete st;
    }
  }
  else {
    ERROR("conference '%s' does not exists\n",cid.c_str());
  }
  cid2s_mut.unlock();
}

//
// instance methods
//

AmConferenceStatus::AmConferenceStatus(const string& conference_id)
  : conf_id(conference_id), mixer(), sessions(), channels()
{
}

AmConferenceStatus::~AmConferenceStatus()
{
  DBG("AmConferenceStatus::~AmConferenceStatus(): conf_id = %s\n",conf_id.c_str());
}

void AmConferenceStatus::postConferenceEvent(int event_id, const string& sess_id)
{
  sessions_mut.lock();
  int participants = sessions.size();
  for(std::map<std::string, unsigned int>::iterator it = sessions.begin(); 
      it != sessions.end(); it++){
    AmSessionContainer::instance()->postEvent(
					      it->first,
					      new ConferenceEvent(event_id,
								  participants,conf_id,sess_id)
					      );
  }
  sessions_mut.unlock();
}

AmConferenceChannel* AmConferenceStatus::getChannel(const string& sess_id, int input_sample_rate)
{
  AmConferenceChannel* ch = 0;

  sessions_mut.lock();
  std::map<std::string, unsigned int>::iterator it = sessions.find(sess_id);
  if(it != sessions.end()){
    ch = new AmConferenceChannel(this,it->second,sess_id,false);
  } else {
    if(!sessions.empty()){
      int participants = sessions.size()+1;
      for(it = sessions.begin(); it != sessions.end(); it++){
	AmSessionContainer::instance()->postEvent(
						  it->first,
						  new ConferenceEvent(ConfNewParticipant,
								      participants,conf_id,sess_id)
						  );
      }
    } else {
      // The First participant gets its own NewParticipant message
      AmSessionContainer::instance()->postEvent(
						sess_id, new ConferenceEvent(ConfNewParticipant,1,
									     conf_id,sess_id));
    }

    unsigned int ch_id = mixer.addChannel(input_sample_rate);
    SessInfo* si = new SessInfo(sess_id,ch_id);

    sessions[sess_id] = ch_id;
    channels[ch_id] = si;

    ch = new AmConferenceChannel(this,ch_id,sess_id, true);

  }
  sessions_mut.unlock();

  return ch;
}

int AmConferenceStatus::releaseChannel(unsigned int ch_id)
{
  unsigned int participants=0;

  sessions_mut.lock();
  std::map<unsigned int, SessInfo*>::iterator it = channels.find(ch_id);
  if(it != channels.end()){
	
    SessInfo* si = it->second;
    channels.erase(it);
    sessions.erase(si->sess_id);

    mixer.removeChannel(ch_id);

    participants = channels.size();
    std::map<std::string, unsigned int>::iterator s_it;
    for(s_it = sessions.begin(); s_it != sessions.end(); s_it++){
	    
      AmSessionContainer::instance()->postEvent(
						s_it->first,
						new ConferenceEvent(ConfParticipantLeft,
								    participants,
								    conf_id, si->sess_id));
    }
    delete si;
	
  }
  else {
    participants = channels.size();
    ERROR("bad channel id=%i within conference status '%s'\n",
	  ch_id,conf_id.c_str());
  }
  sessions_mut.unlock();

  return participants;
}
