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
#include "ModConference.h"

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("conference.join", ConfJoinAction);
  DEF_CMD("conference.postEvent", ConfPostEventAction);
  DEF_CMD("conference.setPlayoutType", ConfSetPlayoutTypeAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

CONST_ACTION_2P(ConfPostEventAction, ',', true);
EXEC_ACTION_START(ConfPostEventAction) {
  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string ev_id = resolveVars(par2, sess, sc_sess, event_params);
  
  unsigned int ev;
  if (str2i(ev_id, ev)) {
    ERROR("decoding conference event id '%s'\n", ev_id.c_str());
    return false;
  }

  AmConferenceStatus::postConferenceEvent(channel_id, ev, sess->getLocalTag());
} EXEC_ACTION_END;


CONST_ACTION_2P(ConfJoinAction, ',', true);
EXEC_ACTION_START(ConfJoinAction) {
  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string mode = resolveVars(par2, sess, sc_sess, event_params);

  bool connect_play = false;
  bool connect_record = false;
  if (mode.empty()) {
    connect_play = true;
    connect_record = true;
  } else if (mode == "speakonly") {
    connect_play = true;
  } else if (mode == "listenonly") {
    connect_record = true;
  } 
  DBG("connect_play = %s, connect_rec = %s\n", 
      connect_play?"true":"false", 
      connect_record?"true":"false");
  
  AmConferenceChannel* chan = AmConferenceStatus::getChannel(channel_id, sess->getLocalTag());
  if (NULL == chan) {
    ERROR("obtaining conference channel\n");
    return false;
  }

  DSMConfChannel* dsm_chan = new DSMConfChannel(chan);
  AmAudio* play_item = NULL;
  AmAudio* rec_item = NULL;
  if (connect_play)
    play_item = chan;
  if (connect_record)
    rec_item = chan;

  sc_sess->addToPlaylist(new AmPlaylistItem(play_item, rec_item));

  sc_sess->transferOwnership(dsm_chan);
} EXEC_ACTION_END;


EXEC_ACTION_START(ConfSetPlayoutTypeAction) {
  string playout_type = resolveVars(arg, sess, sc_sess, event_params);
  if (playout_type == "adaptive")
    sess->rtp_str.setPlayoutType(ADAPTIVE_PLAYOUT);
  else if (playout_type == "jb")
    sess->rtp_str.setPlayoutType(JB_PLAYOUT);
  else 
    sess->rtp_str.setPlayoutType(SIMPLE_PLAYOUT);
} EXEC_ACTION_END;
