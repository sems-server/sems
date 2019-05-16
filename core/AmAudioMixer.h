/*
 * Copyright (C) 2008 IPTEGO GmbH
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

#ifndef _AmAudioMixer_h_
#define _AmAudioMixer_h_

#include "AmMultiPartyMixer.h"
#include "AmThread.h"

#include <map>
#include <set>

class AmAudioMixerConnector;

/**
 * \brief \ref AmAudio to mix input
 * 
 * AmAudio that mixes some sources' audio and writes into a set of sinks.
 * 
 * Can probably do lots of things together with AmAudioQueue and/or AmAudioMixIn.
 *
 * Attention: Sources (in fact AmAudioMixerConnector) are owned by the AmAudioMixer, 
 *            i.e. deleted on releaseSink/destructor.
 *            Sinks are not owned by the AmAudioMixer.
 */
class AmAudioMixer 
{
  AmMultiPartyMixer mixer;

  AmMutex srcsink_mut;
  std::map<AmAudioMixerConnector*, unsigned int> sources;

  unsigned int sink_channel;
  AmAudioMixerConnector* sink_connector;
  std::set<AmAudio*> sinks;

 public:
  AmAudioMixer(int external_sample_rate);
  ~AmAudioMixer();
  
  AmAudio* addSource(int external_sample_rate);
  void releaseSource(AmAudio* s);

  void addSink(AmAudio* s);
  void releaseSink(AmAudio* s);
};

class AmAudioMixerConnector 
: public AmAudio {
  AmMultiPartyMixer& mixer;
  unsigned int channel;
  AmMutex* audio_mut;
  std::set<AmAudio*>* sinks;
  AmAudio* mix_channel;

 protected:
  int get(unsigned long long system_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int nb_samples);
  int put(unsigned long long system_ts, unsigned char* buffer, 
	  int input_sample_rate, unsigned int size);

  // dummies for AmAudio's pure virtual methods
  int read(unsigned int user_ts, unsigned int size){ return -1; }
  int write(unsigned int user_ts, unsigned int size){ return -1; }

 public:
 AmAudioMixerConnector(AmMultiPartyMixer& mixer, unsigned int channel,
		       AmAudio* mix_channel, 
		       AmMutex* audio_mut = NULL, std::set<AmAudio*>* sinks = NULL) 
   : mixer(mixer), channel(channel), audio_mut(audio_mut),
    sinks(sinks), mix_channel(mix_channel) { }
  ~AmAudioMixerConnector() { }

};

#endif
