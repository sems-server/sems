/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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
#include "log.h"
#include "AmUtils.h"
#include "AmSession.h"
#include "AmPlaylist.h"
#include "AmConferenceChannel.h"
#include "AmRtpAudio.h"

#include "DSMSession.h"
#include "DSMCoreModule.h"

#include "ModConference.h"

SC_EXPORT(ConfModule);

ConfModule::ConfModule() {
}

ConfModule::~ConfModule() {
}

void splitCmd(const string& from_str, 
	      string& cmd, string& params) {
  size_t b_pos = from_str.find('(');
  if (b_pos != string::npos) {
    cmd = from_str.substr(0, b_pos);
    params = from_str.substr(b_pos + 1, from_str.rfind(')') - b_pos -1);
  } else 
    cmd = from_str;  
}

DSMAction* ConfModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

#define DEF_CMD(cmd_name, class_name) \
				      \
  if (cmd == cmd_name) {	      \
    class_name * a =		      \
      new class_name(params);	      \
    a->name = from_str;		      \
    return a;			      \
  }

  DEF_CMD("conference.join", ConfJoinAction);
  DEF_CMD("conference.postEvent", ConfPostEventAction);
  DEF_CMD("conference.setPlayoutType", ConfSetPlayoutTypeAction);

  return NULL;
}

DSMCondition* ConfModule::getCondition(const string& from_str) {
  return NULL;
}

#define GET_SCSESSION()					 \
  DSMSession* sc_sess = dynamic_cast<DSMSession*>(sess); \
  if (!sc_sess) {					 \
    ERROR("wrong session type\n");			 \
    return false;					 \
  }


CONST_ACTION_2P(ConfPostEventAction, ',', true);

bool ConfPostEventAction::execute(AmSession* sess, 
			     DSMCondition::EventType event,
			     map<string,string>* event_params) {
  GET_SCSESSION();

  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string ev_id = resolveVars(par2, sess, sc_sess, event_params);
  
  unsigned int ev;
  if (str2i(ev_id, ev)) {
    ERROR("decoding conference event id '%s'\n", ev_id.c_str());
    return false;
  }

  AmConferenceStatus::postConferenceEvent(channel_id, ev, sess->getLocalTag());

  return false;
}



bool ConfJoinAction::execute(AmSession* sess, 
			     DSMCondition::EventType event,
			     map<string,string>* event_params) {
  GET_SCSESSION();

  string channel_id = resolveVars(arg, sess, sc_sess, event_params);
  
  AmConferenceChannel* chan = AmConferenceStatus::getChannel(channel_id, sess->getLocalTag());
  if (NULL == chan) {
    ERROR("obtaining conference channel\n");
    return false;
  }

  DSMConfChannel* dsm_chan = new DSMConfChannel(chan);
  
  sc_sess->addToPlaylist(new AmPlaylistItem(chan, chan));

  sc_sess->transferOwnership(dsm_chan);

  return false;
}


bool ConfSetPlayoutTypeAction::execute(AmSession* sess, 
				       DSMCondition::EventType event,
				       map<string,string>* event_params) {
  GET_SCSESSION();

  string playout_type = resolveVars(arg, sess, sc_sess, event_params);
  if (playout_type == "adaptive")
    sess->rtp_str.setPlayoutType(ADAPTIVE_PLAYOUT);
  else if (playout_type == "jb")
    sess->rtp_str.setPlayoutType(JB_PLAYOUT);
  else 
    sess->rtp_str.setPlayoutType(SIMPLE_PLAYOUT);

  return false;
}
