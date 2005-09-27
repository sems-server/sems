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
    : channels(),ch_offsets(),
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
    channels.insert(std::make_pair(cur_channel_id,SampleArrayShort()));

    return cur_channel_id++;
}

void AmMultiPartyMixer::removeChannel(int channel_id)
{
    channels.erase(channel_id);
    ch_offsets.erase(channel_id);
    DBG("removed channel: #%i\n",channel_id);
}

SampleArrayShort* AmMultiPartyMixer::get_channel(unsigned int channel_id)
{
    ChannelMap::iterator channel_it = channels.find(channel_id);
    if(channel_it == channels.end()){
	ERROR("channel %i does not exist\n",channel_id);
	return NULL;
    }
    
    return &channel_it->second;
}

void AmMultiPartyMixer::PutChannelPacket(unsigned int   channel_id,
					 unsigned int   ts, 
					 unsigned char* buffer, 
					 unsigned int   size,
					 bool           begin_talk)
{
    if(!size) return;
    assert(size <= AUDIO_BUFFER_SIZE);
    
    SampleArrayShort* channel = get_channel(channel_id);
    if(!channel){
	ERROR("MultiPartyMixer::PutChannelPacket: "
	      "channel #%i doesn't exist\n",channel_id);
	return;
    }

   if(ch_last_ts.find(channel_id) == ch_last_ts.end())
	ch_last_ts[channel_id] = ts;
    
    unsigned int& last_ts = ch_last_ts[channel_id];

#ifdef RORPP_PLC
    LowcFE& fec = fec_map[channel_id];
#endif
   
    size >>= 1;
    int* tmp_buf = new int[size];
    short* loss_buf = 0;
    short* tmp_loss_buf = 0;
    try {

	if(cmp_ts(last_ts,ts) && !begin_talk
	   && (ts-last_ts <= 160*4)){

#ifdef RORPP_PLC
  	    loss_buf = new short[size > FRAMESZ ? size : FRAMESZ];

  	    while(cmp_ts(last_ts,ts)){ // last_ts < ts
		
  		fec.dofe(loss_buf);
  		channel->put(last_ts,loss_buf,FRAMESZ);

  		mixed_channel.get(last_ts,tmp_buf,FRAMESZ);
  		mix_add(tmp_buf,tmp_buf,loss_buf,FRAMESZ);
  		mixed_channel.put(last_ts,tmp_buf,FRAMESZ);

  		last_ts += FRAMESZ;
  	    }
#else	    
	    float scale_factor = 10.0;
	    loss_buf = new short[size];
	    tmp_loss_buf = new short[size];

	    channel->get(last_ts-size,loss_buf,size);
	    memcpy(tmp_loss_buf,loss_buf,size);

	    while(cmp_ts(last_ts,ts)){ // last_ts < ts

		log_scale(tmp_loss_buf,loss_buf,size,scale_factor);

		channel->put(last_ts,tmp_loss_buf,size);
		mixed_channel.get(last_ts,tmp_buf,size);
		mix_add(tmp_buf,tmp_buf,loss_buf,size);
		mixed_channel.put(last_ts,tmp_buf,size);

		last_ts+=size;
		scale_factor -= 2.2;
	    }
#endif	    
	}
	
#ifdef RORPP_PLC
	for(unsigned int i=0; i<(size/FRAMESZ); i++)
	    fec.addtohistory(((short*)buffer) + i*FRAMESZ);
#endif

	channel->put(ts,(short*)buffer,size);

	mixed_channel.get(ts,tmp_buf,size);
	mix_add(tmp_buf,tmp_buf,(short*)buffer,size);
	mixed_channel.put(ts,tmp_buf,size);
    }
    catch(...){}
    delete [] tmp_buf;
    delete [] loss_buf;
    delete [] tmp_loss_buf;
    last_ts = ts + size;
}

void AmMultiPartyMixer::GetChannelPacket(unsigned int   channel_id,
					 unsigned int   ts, 
					 unsigned char* buffer, 
					 unsigned int   size)
{
    assert(size <= AUDIO_BUFFER_SIZE);

    SampleArrayShort* channel = 0;
    if((channel = get_channel(channel_id)) != 0){

	size >>= 1;
 	int* tmp_buf = new int[size];
 	try {
	    ts -= 160*4;
	    mixed_channel.get(ts,tmp_buf,size);
 	    channel->get(ts,(short*)buffer,size);
	    mix_sub(tmp_buf,tmp_buf,(short*)buffer,size);
     	    scale((short*)buffer,tmp_buf,size);
 	}
 	catch(...){}
	delete [] tmp_buf;
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

void AmMultiPartyMixer::log_scale(short* out_buf,short* in_buf,
				unsigned int size,float scale_factor)
{
    short* end_dest = out_buf + size;

    scale_factor = log(scale_factor);
    int fact = int(scale_factor*64.0);
    
    while(out_buf != end_dest){
	
	int s = (int(*in_buf) * fact) >> 6;
	*(out_buf++) = short(s);
 	in_buf++;
    }
}
