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
/** @file AmMultiPartyMixer.h */
#ifndef _MultiPartyMixer_h_
#define _MultiPartyMixer_h_

#include "AmAudio.h"
#include "AmThread.h"
#include "SampleArray.h"

//#define RORPP_PLC

#ifdef RORPP_PLC
#include "LowcFE.h"
#endif

#include <map>
#include <set>

struct MixerBufferState
{
  typedef std::map<int,SampleArrayShort*> ChannelMap;

  unsigned int sample_rate;
  unsigned int last_ts;
  ChannelMap channels;
  SampleArrayInt *mixed_channel;

  MixerBufferState(unsigned int sample_rate, std::set<int>& channelids);
  MixerBufferState(const MixerBufferState& other);
  ~MixerBufferState();

  void add_channel(unsigned int channel_id);
  void remove_channel(unsigned int channel_id);
  SampleArrayShort* get_channel(unsigned int channel_id);
  void fix_channels(std::set<int>& curchannelids);
  void free_channels();
};

/**
 * \brief Mixer for one conference.
 * 
 * AmMultiPartyMixer mixes the audio from all channels,
 * and returns the audio of all other channels. 
 */
class AmMultiPartyMixer
{
  typedef std::set<int> ChannelIdSet;
  typedef std::map<int,int> SampleRateMap;
  typedef std::multiset<int> SampleRateSet;

  SampleRateMap    sampleratemap;
  SampleRateSet    samplerates;
  ChannelIdSet     channelids;
  std::deque<MixerBufferState> buffer_state;

  AmMutex          audio_mut;
  int              scaling_factor; 
  int              tmp_buffer[AUDIO_BUFFER_SIZE/2];

  std::deque<MixerBufferState>::iterator findOrCreateBufferState(unsigned int sample_rate);
  std::deque<MixerBufferState>::iterator findBufferStateForReading(unsigned int sample_rate, 
								   unsigned long long last_ts);
  void cleanupBufferStates(unsigned int last_ts);

  void mix_add(int* dest,int* src1,short* src2,unsigned int size);
  void mix_sub(int* dest,int* src1,short* src2,unsigned int size);
  void scale(short* buffer,int* tmp_buf,unsigned int size);

public:
  AmMultiPartyMixer();
  ~AmMultiPartyMixer();
    
  unsigned int addChannel(unsigned int external_sample_rate);
  void removeChannel(unsigned int channel_id);

  void PutChannelPacket(unsigned int   channel_id,
			unsigned long long system_ts,
			unsigned char* buffer, 
			unsigned int   size);

  void GetChannelPacket(unsigned int   channel,
			unsigned long long system_ts,
			unsigned char* buffer, 
			unsigned int&  size,
			unsigned int&  output_sample_rate);

  int GetCurrentSampleRate();

  void lock();
  void unlock();
};

#endif

