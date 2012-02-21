/*
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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
#include "AmAudioMixIn.h"

#include "DSMSession.h"
#include "ModConference.h"


SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("conference.join", ConfJoinAction);
  DEF_CMD("conference.leave", ConfLeaveAction);
  DEF_CMD("conference.rejoin", ConfRejoinAction);
  DEF_CMD("conference.postEvent", ConfPostEventAction);
  DEF_CMD("conference.setPlayoutType", ConfSetPlayoutTypeAction);
  DEF_CMD("conference.teejoin", ConfTeeJoinAction);
  DEF_CMD("conference.teeleave", ConfTeeLeaveAction);

  DEF_CMD("conference.setupMixIn", ConfSetupMixInAction);
  DEF_CMD("conference.playMixIn", ConfPlayMixInAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);


void DSMConfChannel::release() {
  chan.reset(NULL);
}

void DSMConfChannel::reset(AmConferenceChannel* channel) {
  chan.reset(channel);
}

DSMTeeConfChannel::DSMTeeConfChannel(AmConferenceChannel* channel) 
  : chan(channel) { 
  audio_queue.setOwning(false);
}

DSMTeeConfChannel::~DSMTeeConfChannel() {
}

void DSMTeeConfChannel::release() {
  chan.reset(NULL);
}

void DSMTeeConfChannel::reset(AmConferenceChannel* channel) {
  chan.reset(channel);
}

AmAudio* DSMTeeConfChannel::setupAudio(AmAudio* out) {
  DBG("out == %p, chan.get == %p\n", out, chan.get());
  if (!chan.get() || !out)
    return NULL;

  // send input audio (speak) to conf channel
  audio_queue.pushAudio(chan.get(), AmAudioQueue::InputQueue, AmAudioQueue::Back, true /* write */, false /* read */);
  // send input audio (speak) to out
  audio_queue.pushAudio(out, AmAudioQueue::InputQueue, AmAudioQueue::Back, true /* write */, false /* read */);

  return &audio_queue;
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
							     sess->getLocalTag(), sess->RTPStream()->getSampleRate());
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
 
template<class T> 
static T* getDSMConfChannel(DSMSession* sc_sess, const char* key_name) {
  if (sc_sess->avar.find(key_name) == sc_sess->avar.end()) {
    return NULL;
  }
  AmObject* ao = NULL; T* res = NULL;
  try {
    if (!isArgAObject(sc_sess->avar[key_name])) {
      return NULL;
    }
    ao = sc_sess->avar[key_name].asObject();
  } catch (...){
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<T*>(ao))) {
    return NULL;
  }
  return res;
}

EXEC_ACTION_START(ConfLeaveAction) {
  DSMConfChannel* chan = getDSMConfChannel<DSMConfChannel>(sc_sess, CONF_AKEY_CHANNEL);
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

  DSMConfChannel* chan = getDSMConfChannel<DSMConfChannel>(sc_sess, CONF_AKEY_CHANNEL);
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


CONST_ACTION_2P(ConfTeeJoinAction, ',', true);
EXEC_ACTION_START(ConfTeeJoinAction) {
  string channel_id = resolveVars(par1, sess, sc_sess, event_params);
  string conf_varname = resolveVars(par2, sess, sc_sess, event_params);
  if (conf_varname.empty()) 
    conf_varname = CONF_AKEY_DEF_TEECHANNEL;

  DBG("Speaking also in conference '%s' (with cvar '%s')\n",
      channel_id.c_str(), conf_varname.c_str());

  DSMTeeConfChannel* chan = 
    getDSMConfChannel<DSMTeeConfChannel>(sc_sess, conf_varname.c_str());
  if (NULL == chan) {
    DBG("not previously in tee-channel, creating new\n");
    AmConferenceChannel* conf_channel = AmConferenceStatus::getChannel(channel_id, 
								       sess->getLocalTag(), sess->RTPStream()->getSampleRate());
    if (NULL == conf_channel) {
      ERROR("obtaining conference channel\n");
      throw DSMException("conference");
    }

    chan = new DSMTeeConfChannel(conf_channel);
    // remember DSMTeeConfChannel in session avar
    AmArg c_arg;
    c_arg.setBorrowedPointer(chan);
    sc_sess->avar[conf_varname] = c_arg;
    
    // add to garbage collector
    sc_sess->transferOwnership(chan);

    // link channel audio before session's input (usually playlist)
    AmAudio* chan_audio = chan->setupAudio(sess->getInput());
    if (chan_audio == NULL) {
      ERROR("tee channel audio setup failed\n");
      throw DSMException("conference");
    } 

    sess->setInput(chan_audio);    

  } else {
    DBG("previously already in tee-channel, resetting\n");

    // temporarily switch back to playlist, 
    // while we are releasing the old channel
    sc_sess->setInputPlaylist();

    AmConferenceChannel* conf_channel = AmConferenceStatus::getChannel(channel_id, 
								       sess->getLocalTag(), sess->RTPStream()->getSampleRate());
    if (NULL == conf_channel) {
      ERROR("obtaining conference channel\n");
      throw DSMException("conference");
      return false;
    }

    chan->reset(conf_channel);

    // link channel audio before session's input (usually playlist)
    AmAudio* chan_audio = chan->setupAudio(sess->getInput());
    if (chan_audio == NULL) {
      ERROR("tee channel audio setup failed\n");
      throw DSMException("conference");
    } 

    sess->setInput(chan_audio);    
  }
  
} EXEC_ACTION_END;


EXEC_ACTION_START(ConfTeeLeaveAction) {
  string conf_varname = resolveVars(arg, sess, sc_sess, event_params);
  if (conf_varname.empty()) 
    conf_varname = CONF_AKEY_DEF_TEECHANNEL;

  DSMTeeConfChannel* chan = 
    getDSMConfChannel<DSMTeeConfChannel>(sc_sess, conf_varname.c_str());
  if (NULL == chan) {
    WARN("app error: trying to leave tee conference, but channel not found\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_SCRIPT);
    sc_sess->SET_STRERROR("trying to leave tee conference, but channel not found");
    return false;
  }

  // for safety, set back playlist to in/out
  sc_sess->setInOutPlaylist();

  // release conf channel 
  chan->release();

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

CONST_ACTION_2P(ConfSetupMixInAction, ',', true);
EXEC_ACTION_START(ConfSetupMixInAction) {
  string level = resolveVars(par1, sess, sc_sess, event_params);
  string seconds = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int s; double l; int flags = 0;

  l = atof(level.c_str());
  if (seconds.empty()) {
    s = 0;
  } else {
    if (str2i(seconds, s)) {
      throw DSMException("conference", 
			 "cause", "could not interpret seconds value");
    }
  }
  if (s == 0) {
    flags = AUDIO_MIXIN_IMMEDIATE_START | AUDIO_MIXIN_ONCE;
  }

  AmAudio* output = sess->getOutput();
  AmAudioMixIn* m = new AmAudioMixIn(output, NULL, s, l, flags);
  sess->setOutput(m);
  
  DSMDisposableT<AmAudioMixIn >* m_obj = 
    getDSMConfChannel<DSMDisposableT<AmAudioMixIn > >(sc_sess, CONF_AKEY_MIXER);
  if (NULL != m_obj) {
    DBG("releasing old MixIn (hope script write setInOutPlaylist before)\n");
    m_obj->reset(m);
  } else {
    DBG("creating new mixer container\n");
    m_obj = new DSMDisposableT<AmAudioMixIn >(m);
    AmArg c_arg;
    c_arg.setBorrowedPointer(m_obj);
    sc_sess->avar[CONF_AKEY_MIXER] = c_arg;
      
    // add to garbage collector
    sc_sess->transferOwnership(m_obj);
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(ConfPlayMixInAction) {
  string filename = resolveVars(arg, sess, sc_sess, event_params);

  DSMDisposableT<AmAudioMixIn >* m_obj = 
    getDSMConfChannel<DSMDisposableT<AmAudioMixIn > >(sc_sess, CONF_AKEY_MIXER);
  if (NULL == m_obj) {
    throw DSMException("conference", "cause", "mixer not setup!\n");
  } 

  AmAudioMixIn* m = m_obj->get();

  DSMDisposableAudioFile* af = new DSMDisposableAudioFile();
  if(af->open(filename,AmAudioFile::Read)) {
    ERROR("audio file '%s' could not be opened for reading.\n", 
	  filename.c_str());
    delete af;
    
    throw DSMException("file", "path", filename);
  }

  sc_sess->transferOwnership(af);

  DBG("starting mixin of file '%s'\n", filename.c_str());
  m->mixin(af);

} EXEC_ACTION_END;
