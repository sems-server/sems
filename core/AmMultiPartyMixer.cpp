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

#include "AmMultiPartyMixer.h"
#include "AmRtpStream.h"
#include "log.h"

#include <assert.h>
#include <math.h>

// PCM16 range: [-32767:32768]
#define MAX_LINEAR_SAMPLE 32737

// the internal delay of the mixer (between put and get)
#define MIXER_DELAY_MS 20

#define MAX_BUFFER_STATES 50 // 1 sec max @ 20ms

void DEBUG_MIXER_BUFFER_STATE(const MixerBufferState& mbs, const string& context)
{
  DBG("XXDebugMixerXX: dump of MixerBufferState %s", context.c_str());
  DBG("XXDebugMixerXX: sample_rate = %u", mbs.sample_rate);
  DBG("XXDebugMixerXX: last_ts = %u", mbs.last_ts);
  for (MixerBufferState::ChannelMap::const_iterator it = mbs.channels.begin(); it != mbs.channels.end(); it++) {
    DBG("XXDebugMixerXX: channel #%d present", it->first);
  }
  DBG("XXDebugMixerXX: end of MixerBufferState dump");
}

AmMultiPartyMixer::AmMultiPartyMixer()
  : sampleratemap(), samplerates(),
    channelids(), buffer_state(),
    audio_mut(), scaling_factor(16)
{
}

AmMultiPartyMixer::~AmMultiPartyMixer()
{
  for (std::deque<MixerBufferState>::iterator it = buffer_state.begin();
       it != buffer_state.end(); it++) {
    it->free_channels();
  }
}

unsigned int AmMultiPartyMixer::addChannel(unsigned int external_sample_rate)
{
  unsigned int cur_channel_id = 0;

  audio_mut.lock();
  ChannelIdSet::reverse_iterator rit = channelids.rbegin();
  if (rit != channelids.rend()) {
    cur_channel_id = *rit + 1;
  }

  channelids.insert(cur_channel_id);

  for (std::deque<MixerBufferState>::iterator it = buffer_state.begin(); it != buffer_state.end(); it++) {
    //DBG("XXDebugMixerXX: AmMultiPartyMixer::addChannel(): processing buffer state with sample rate %d", it->sample_rate);
    if (it->sample_rate >= external_sample_rate) {
      it->add_channel(cur_channel_id);
      break;
    }
  }

  //DBG("XXDebugMixerXX: added channel: #%i\n",cur_channel_id);

  sampleratemap.insert(std::make_pair(cur_channel_id,external_sample_rate));
  samplerates.insert(external_sample_rate);

  audio_mut.unlock();
  return cur_channel_id;
}

void AmMultiPartyMixer::removeChannel(unsigned int channel_id)
{
  audio_mut.lock();
  for (std::deque<MixerBufferState>::iterator it = buffer_state.begin(); it != buffer_state.end(); it++) {
    it->remove_channel(channel_id);
  }

  channelids.erase(channel_id);

  SampleRateMap::iterator sit = sampleratemap.find(channel_id);
  if (sit != sampleratemap.end()) {
	SampleRateSet::iterator it = samplerates.find(sit->second);
	samplerates.erase(it);
	sampleratemap.erase(channel_id);
  }
  //DBG("XXDebugMixerXX: removed channel: #%i\n",channel_id);
  audio_mut.unlock();
}

void AmMultiPartyMixer::PutChannelPacket(unsigned int   channel_id,
					 unsigned long long system_ts, 
					 unsigned char* buffer, 
					 unsigned int   size)
{
  if(!size)
    return;
  assert(size <= AUDIO_BUFFER_SIZE);

  std::deque<MixerBufferState>::iterator bstate = findOrCreateBufferState(GetCurrentSampleRate());

  SampleArrayShort* channel = 0;
  if((channel = bstate->get_channel(channel_id)) != 0) {

    unsigned samples = PCM16_B2S(size);
    unsigned long long put_ts = system_ts + (MIXER_DELAY_MS * WALLCLOCK_RATE / 1000);
    unsigned long long user_put_ts = put_ts * (GetCurrentSampleRate()/100) / (WALLCLOCK_RATE/100);

    channel->put(user_put_ts,(short*)buffer,samples);
    bstate->mixed_channel->get(user_put_ts,tmp_buffer,samples);

    mix_add(tmp_buffer,tmp_buffer,(short*)buffer,samples);
    bstate->mixed_channel->put(user_put_ts,tmp_buffer,samples);
    bstate->last_ts = put_ts + (samples * (WALLCLOCK_RATE/100) / (GetCurrentSampleRate()/100));
  } else {
    /*
    ERROR("XXDebugMixerXX: MultiPartyMixer::PutChannelPacket: "
	  "channel #%i doesn't exist\n",channel_id);
    DBG("XXDebugMixer:: PutChannelPacket failed ts=%u", ts);
    for (std::deque<MixerBufferState>::iterator it = buffer_state.begin(); it != buffer_state.end(); it++) {
      DEBUG_MIXER_BUFFER_STATE(*it, "on PutChannelPacket failure");
      }*/
  }
}

void AmMultiPartyMixer::GetChannelPacket(unsigned int   channel_id,
					 unsigned long long system_ts, 
					 unsigned char* buffer, 
					 unsigned int&  size,
					 unsigned int&  output_sample_rate)
{
  if (!size)
    return;
  assert(size <= AUDIO_BUFFER_SIZE);

  unsigned int last_ts = system_ts + (PCM16_B2S(size) * (WALLCLOCK_RATE/100) / (GetCurrentSampleRate()/100));
  std::deque<MixerBufferState>::iterator bstate = findBufferStateForReading(GetCurrentSampleRate(), last_ts);

  SampleArrayShort* channel = 0;
  if(bstate != buffer_state.end() && (channel = bstate->get_channel(channel_id)) != 0) {

    unsigned int samples = PCM16_B2S(size) * (bstate->sample_rate/100) / (GetCurrentSampleRate()/100);
    assert(samples <= PCM16_B2S(AUDIO_BUFFER_SIZE));

    unsigned long long cur_ts = system_ts * (bstate->sample_rate/100) / (WALLCLOCK_RATE/100);
    bstate->mixed_channel->get(cur_ts,tmp_buffer,samples);
    channel->get(cur_ts,(short*)buffer,samples);

    mix_sub(tmp_buffer,tmp_buffer,(short*)buffer,samples);
    scale((short*)buffer,tmp_buffer,samples);
    size = PCM16_S2B(samples);
    output_sample_rate = bstate->sample_rate;
  } else if (bstate != buffer_state.end()) {
    memset(buffer,0,size);
    output_sample_rate = GetCurrentSampleRate();
    //DBG("XXDebugMixerXX: GetChannelPacket returned zeroes, ts=%u, last_ts=%u, output_sample_rate=%u", ts, last_ts, output_sample_rate);
  } else {
    /*
    ERROR("XXDebugMixerXX: MultiPartyMixer::GetChannelPacket: "
	  "channel #%i doesn't exist\n",channel_id);
    DBG("XXDebugMixerXX: GetChannelPacket failed, ts=%u", ts);
    for (std::deque<MixerBufferState>::iterator it = buffer_state.begin(); it != buffer_state.end(); it++) {
      DEBUG_MIXER_BUFFER_STATE(*it, "on GetChannelPacket failure");
      }*/
  }

  cleanupBufferStates(last_ts);
}

int AmMultiPartyMixer::GetCurrentSampleRate()
{
  SampleRateSet::reverse_iterator sit = samplerates.rbegin();
  if (sit != samplerates.rend()) {
	return *sit;
  } else {
	return -1;
  }
}

// int   dest[size/2]
// int   src1[size/2]
// short src2[size/2]
//
void AmMultiPartyMixer::mix_add(int* dest,int* src1,short* src2,unsigned int size)
{
  int* end_dest = dest + size;

  while(dest != end_dest)
    *(dest++) = *(src1++) + int(*(src2++));
}

void AmMultiPartyMixer::mix_sub(int* dest,int* src1,short* src2,unsigned int size)
{
  int* end_dest = dest + size;

  while(dest != end_dest)
    *(dest++) = *(src1++) - int(*(src2++));
}

void AmMultiPartyMixer::scale(short* buffer,int* tmp_buf,unsigned int size)
{
  short* end_dest = buffer + size;
    
  if(scaling_factor<64)
    scaling_factor++;
    
  while(buffer != end_dest){
	
    int s = (*tmp_buf * scaling_factor) >> 6;
    if(abs(s) > MAX_LINEAR_SAMPLE){
      scaling_factor = abs( (MAX_LINEAR_SAMPLE<<6) / (*tmp_buf) );
      if(s < 0)
	s = -MAX_LINEAR_SAMPLE;
      else
	s = MAX_LINEAR_SAMPLE;
    }
    *(buffer++) = short(s);
    tmp_buf++;
  }
}

std::deque<MixerBufferState>::iterator AmMultiPartyMixer::findOrCreateBufferState(unsigned int sample_rate)
{
  for (std::deque<MixerBufferState>::iterator it = buffer_state.begin(); it != buffer_state.end(); it++) {
    if (it->sample_rate == sample_rate) {
      it->fix_channels(channelids);
      //DEBUG_MIXER_BUFFER_STATE(*it, "returned to PutChannelPacket");
      return it;
    }
  }

  //DBG("XXDebugMixerXX: Creating buffer state (from PutChannelPacket)");
  buffer_state.push_back(MixerBufferState(sample_rate, channelids));
  std::deque<MixerBufferState>::reverse_iterator rit = buffer_state.rbegin();
  //DEBUG_MIXER_BUFFER_STATE(*((rit + 1).base()), "returned to PutChannelPacket");
  return (rit + 1).base();
}

std::deque<MixerBufferState>::iterator 
AmMultiPartyMixer::findBufferStateForReading(unsigned int sample_rate, 
					     unsigned long long last_ts)
{
  for (std::deque<MixerBufferState>::iterator it = buffer_state.begin(); 
       it != buffer_state.end(); it++) {

    if (sys_ts_less()(last_ts,it->last_ts) 
	|| (last_ts == it->last_ts)) {
      it->fix_channels(channelids);
      //DEBUG_MIXER_BUFFER_STATE(*it, "returned to PutChannelPacket");
      return it;
    }
  }

  if (buffer_state.size() < MAX_BUFFER_STATES) {
    // DBG("XXDebugMixerXX: Creating buffer state (from GetChannelPacket)\n");
    buffer_state.push_back(MixerBufferState(sample_rate, channelids));
  } // else just reuse the last buffer - conference without a speaker
  std::deque<MixerBufferState>::reverse_iterator rit = buffer_state.rbegin();
  //DEBUG_MIXER_BUFFER_STATE(*((rit + 1).base()), "returned to PutChannelPacket");
  return (rit + 1).base();
}

void AmMultiPartyMixer::cleanupBufferStates(unsigned int last_ts)
{
  while (!buffer_state.empty() 
	 && (buffer_state.front().last_ts != 0 && buffer_state.front().last_ts < last_ts) 
	 && (unsigned int)GetCurrentSampleRate() != buffer_state.front().sample_rate) {

    //DEBUG_MIXER_BUFFER_STATE(buffer_state.front(), "freed in cleanupBufferStates");
    buffer_state.front().free_channels();
    buffer_state.pop_front();
  }
}

void AmMultiPartyMixer::lock()
{
  audio_mut.lock();
}

void AmMultiPartyMixer::unlock()
{
  audio_mut.unlock();
}

MixerBufferState::MixerBufferState(unsigned int sample_rate, std::set<int>& channelids)
  : sample_rate(sample_rate), last_ts(0), channels(), mixed_channel(NULL)
{
  for (std::set<int>::iterator it = channelids.begin(); it != channelids.end(); it++) {
    channels.insert(std::make_pair(*it,new SampleArrayShort()));
  }

  mixed_channel = new SampleArrayInt();
}

MixerBufferState::MixerBufferState(const MixerBufferState& other)
  : sample_rate(other.sample_rate), last_ts(other.last_ts), 
    channels(other.channels), mixed_channel(other.mixed_channel)
{
}

MixerBufferState::~MixerBufferState()
{
}

void MixerBufferState::add_channel(unsigned int channel_id)
{
  if (channels.find(channel_id) == channels.end())
    channels.insert(std::make_pair(channel_id,new SampleArrayShort()));
}

void MixerBufferState::remove_channel(unsigned int channel_id)
{
  ChannelMap::iterator channel_it = channels.find(channel_id);
  if (channel_it != channels.end()) {
    delete channel_it->second;
    channels.erase(channel_it);
  }
}

SampleArrayShort* MixerBufferState::get_channel(unsigned int channel_id)
{
  ChannelMap::iterator channel_it = channels.find(channel_id);
  if(channel_it == channels.end()){
    ERROR("XXMixerDebugXX: channel #%i does not exist\n",channel_id);
    return NULL;
  }

  return channel_it->second;
}

void MixerBufferState::fix_channels(std::set<int>& curchannelids)
{
  for (std::set<int>::iterator it = curchannelids.begin(); it != curchannelids.end(); it++) {
    if (channels.find(*it) == channels.end()) {
      DBG("XXMixerDebugXX: fixing channel #%d", *it);
      channels.insert(std::make_pair(*it,new SampleArrayShort()));
    }
  }
}

void MixerBufferState::free_channels()
{
  for (ChannelMap::iterator it = channels.begin(); it != channels.end(); it++) {
    if (it->second != NULL)
      delete it->second;
  }

  delete mixed_channel;
}
