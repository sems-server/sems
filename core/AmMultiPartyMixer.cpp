/*
 * $Id: AmMultiPartyMixer.cpp,v 1.1.2.1 2005/03/07 21:34:45 sayer Exp $
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

#include "AmMultiPartyMixer.h"
#include "AmRtpStream.h"
#include "log.h"

#include <assert.h>
#include <math.h>

// PCM16 range: [-32767:32768]
#define MAX_LINEAR_SAMPLE 32737

AmMultiPartyMixer::AmMultiPartyMixer()
    : channels(),
      cur_channel_id(0),
      scaling_factor(16)
{
}

AmMultiPartyMixer::~AmMultiPartyMixer()
{
}

unsigned int AmMultiPartyMixer::addChannel()
{
    for(;channels.find(cur_channel_id) != channels.end();cur_channel_id++)
	DBG("trying to add channel: #%i\n",cur_channel_id);

    DBG("added channel: #%i\n",cur_channel_id);
    channels.insert(std::make_pair(cur_channel_id,new SampleArrayShort()));

    return cur_channel_id++;
}

void AmMultiPartyMixer::removeChannel(unsigned int channel_id)
{
    delete get_channel(channel_id);
    channels.erase(channel_id);
    DBG("removed channel: #%i\n",channel_id);
}

SampleArrayShort* AmMultiPartyMixer::get_channel(unsigned int channel_id)
{
    ChannelMap::iterator channel_it = channels.find(channel_id);
    if(channel_it == channels.end()){
	ERROR("channel #%i does not exist\n",channel_id);
	return NULL;
    }
    
    return channel_it->second;
}

void AmMultiPartyMixer::PutChannelPacket(unsigned int   channel_id,
					 unsigned int   ts, 
					 unsigned char* buffer, 
					 unsigned int   size)
{
    if(!size) return;
    assert(size <= AUDIO_BUFFER_SIZE);
    
    SampleArrayShort* channel = 0;
    if((channel = get_channel(channel_id)) != 0){

	unsigned samples = PCM16_B2S(size);
	
	channel->put(ts,(short*)buffer,samples);
	mixed_channel.get(ts,tmp_buffer,samples);
	
	mix_add(tmp_buffer,tmp_buffer,(short*)buffer,samples);
	mixed_channel.put(ts,tmp_buffer,samples);
    }
    else {
	ERROR("MultiPartyMixer::PutChannelPacket: "
	      "channel #%i doesn't exist\n",channel_id);
    }

}

void AmMultiPartyMixer::GetChannelPacket(unsigned int   channel_id,
					 unsigned int   ts, 
					 unsigned char* buffer, 
					 unsigned int   size)
{
    if(!size) return;
    assert(size <= AUDIO_BUFFER_SIZE);

    SampleArrayShort* channel = 0;
    if((channel = get_channel(channel_id)) != 0){

	unsigned int samples = PCM16_B2S(size);

	mixed_channel.get(ts,tmp_buffer,samples);
	channel->get(ts,(short*)buffer,samples);

	mix_sub(tmp_buffer,tmp_buffer,(short*)buffer,samples);
	scale((short*)buffer,tmp_buffer,samples);
    }
    else {
	ERROR("MultiPartyMixer::GetChannelPacket: "
	      "channel #%i doesn't exist\n",channel_id);
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
