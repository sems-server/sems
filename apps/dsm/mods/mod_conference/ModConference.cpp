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
  DEF_CMD("conference.leave", ConfLeaveAction);
  DEF_CMD("conference.rejoin", ConfRejoinAction);
  DEF_CMD("conference.postEvent", ConfPostEventAction);
  DEF_CMD("conference.setPlayoutType", ConfSetPlayoutTypeAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);


void DSMConfChannel::release() {
  chan.reset(NULL);
}

void DSMConfChannel::reset(AmConferenceChannel* channel) {
  chan.reset(channel);
}

CONST_ACTION_2P(ConfPostEventAction, ',', true);
EXEC_ACTION_START(ConfPostEventAction) {
  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string ev_id = resolveVars(par2, sess, sc_sess, event_params);
  
  unsigned int ev;
  if (str2i(ev_id, ev)) {
    ERROR("decoding conference event id '%s'\n", ev_id.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("decoding conference event id '"+ev_id+"%s'\n");
    return false;
  }

  AmConferenceStatus::postConferenceEvent(channel_id, ev, sess->getLocalTag());
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

static bool ConferenceJoinChannel(DSMConfChannel** dsm_chan, 
				  AmSession* sess,
				  DSMSession* sc_sess,
				  const string& channel_id, 
				  const string& mode) {
  bool connect_play = false;
  bool connect_record = false;
  if (mode.empty()) {
    connect_play = true;
    connect_record = true;
  } else if (mode == "speakonly") {
    connect_record = true;
  } else if (mode == "listenonly") {
    connect_play = true;
  } 
  DBG("connect_play = %s, connect_rec = %s\n", 
      connect_play?"true":"false", 
      connect_record?"true":"false");
  
  AmConferenceChannel* chan = AmConferenceStatus::getChannel(channel_id, 
							     sess->getLocalTag());
  if (NULL == chan) {
    ERROR("obtaining conference channel\n");
    throw DSMException("conference");
    return false;
  }
  if (NULL != *dsm_chan) {
    (*dsm_chan)->reset(chan);
  } else {
    *dsm_chan = new DSMConfChannel(chan);
  }

  AmAudio* play_item = NULL;
  AmAudio* rec_item = NULL;
  if (connect_play)
    play_item = chan;
  if (connect_record)
    rec_item = chan;

  sc_sess->addToPlaylist(new AmPlaylistItem(play_item, rec_item));

  return true;
}

CONST_ACTION_2P(ConfJoinAction, ',', true);
EXEC_ACTION_START(ConfJoinAction) {
  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string mode = resolveVars(par2, sess, sc_sess, event_params);

  DSMConfChannel* dsm_chan = NULL;

  if (ConferenceJoinChannel(&dsm_chan, sess, sc_sess, channel_id, mode)) {
      // save channel for later use
      AmArg c_arg;
      c_arg.setBorrowedPointer(dsm_chan);
      sc_sess->avar[CONF_AKEY_CHANNEL] = c_arg;
      
      // add to garbage collector
      sc_sess->transferOwnership(dsm_chan);

      sc_sess->CLR_ERRNO;
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
  }
} EXEC_ACTION_END;

static DSMConfChannel* getDSMConfChannel(DSMSession* sc_sess) {
  if (sc_sess->avar.find(CONF_AKEY_CHANNEL) == sc_sess->avar.end()) {
    return NULL;
  }
  ArgObject* ao = NULL; DSMConfChannel* res = NULL;
  try {
    if (!isArgAObject(sc_sess->avar[CONF_AKEY_CHANNEL])) {
      return NULL;
    }
    ao = sc_sess->avar[CONF_AKEY_CHANNEL].asObject();
  } catch (...){
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<DSMConfChannel*>(ao))) {
    return NULL;
  }
  return res;
}

EXEC_ACTION_START(ConfLeaveAction) {
  DSMConfChannel* chan = getDSMConfChannel(sc_sess);
  if (NULL == chan) {
    WARN("app error: trying to leave conference, but channel not found\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_SCRIPT);
    sc_sess->SET_STRERROR("trying to leave conference, but channel not found");
    return false;
  }
  chan->release();

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

CONST_ACTION_2P(ConfRejoinAction, ',', true);
EXEC_ACTION_START(ConfRejoinAction) {
  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string mode = resolveVars(par2, sess, sc_sess, event_params);

  DSMConfChannel* chan = getDSMConfChannel(sc_sess);
  if (NULL == chan) {
    WARN("app error: trying to rejoin conference, but channel not found\n");
  } else {
    chan->release();
  }

  if (ConferenceJoinChannel(&chan, sess, sc_sess, channel_id, mode)) {
  sc_sess->CLR_ERRNO;
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(ConfSetPlayoutTypeAction) {
  string playout_type = resolveVars(arg, sess, sc_sess, event_params);
  if (playout_type == "adaptive")
    sess->RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
  else if (playout_type == "jb")
    sess->RTPStream()->setPlayoutType(JB_PLAYOUT);
  else 
    sess->RTPStream()->setPlayoutType(SIMPLE_PLAYOUT);
} EXEC_ACTION_END;
