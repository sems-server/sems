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
#ifndef _MOD_SYS_H
#define _MOD_SYS_H
#include "DSMModule.h"
#include "AmConferenceStatus.h"
#include "DSMSession.h"
#include "AmAdvancedAudio.h"

#include <memory>
#define MOD_CLS_NAME ConfModule

DECLARE_MODULE(MOD_CLS_NAME);

#define CONF_AKEY_CHANNEL        "conf.chan" 
#define CONF_AKEY_DEF_TEECHANNEL "conf.teechan" 
#define CONF_AKEY_MIXER          "conf.mixer" 

/** holds a conference channel  */
class DSMConfChannel 
: public DSMDisposable,
  public AmObject {
  std::auto_ptr<AmConferenceChannel> chan;

 public:
 DSMConfChannel(AmConferenceChannel* channel) : chan(channel) { }
  ~DSMConfChannel() { }
  void release();
  void reset(AmConferenceChannel* channel);
};

/** hold conference channel and audio queue */
class DSMTeeConfChannel
: public DSMDisposable,
  public AmObject {
  std::auto_ptr<AmConferenceChannel> chan;
  AmAudioQueue audio_queue;

 public:
  DSMTeeConfChannel(AmConferenceChannel* channel);
  ~DSMTeeConfChannel();

  void release();
  void reset(AmConferenceChannel* channel);
  AmAudio* setupAudio(AmAudio* out);
};

template<class T> class DSMDisposableT
: public DSMDisposable,
  public AmObject {
  std::auto_ptr<T> pobj;

 public:
 DSMDisposableT(T* _pobj) : pobj(_pobj) { }
  ~DSMDisposableT() { }
  void release() { pobj.reset(NULL); }
  void reset(T* _pobj) { pobj.reset(_pobj); }

  T* get() { return pobj.get(); }
};

DEF_ACTION_2P(ConfJoinAction);
DEF_ACTION_1P(ConfLeaveAction);
DEF_ACTION_2P(ConfRejoinAction); 
DEF_ACTION_2P(ConfPostEventAction);
DEF_ACTION_1P(ConfSetPlayoutTypeAction);

DEF_ACTION_2P(ConfTeeJoinAction);
DEF_ACTION_1P(ConfTeeLeaveAction);

DEF_ACTION_2P(ConfSetupMixInAction);
DEF_ACTION_1P(ConfPlayMixInAction);

#endif
