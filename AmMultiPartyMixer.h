/*
 * $Id: AmMultiPartyMixer.h,v 1.1.2.2 2005/03/10 15:22:55 rco Exp $
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
using std::map;

class AmMultiPartyMixer
{
    typedef map<int,SampleArrayShort> ChannelMap;
    typedef map<int,unsigned int>     ChannelOff;

    ChannelMap       channels;
    ChannelOff       ch_offsets;
    ChannelOff       ch_last_ts;
    unsigned int     cur_channel_id;

    SampleArrayInt   mixed_channel;
    int              scaling_factor; 

#ifdef RORPP_PLC
    map<int,LowcFE>  fec_map;
#endif

    SampleArrayShort* get_channel(unsigned int channel_id);

    void mix_add(int* dest,int* src1,short* src2,unsigned int size);
    void mix_sub(int* dest,int* src1,short* src2,unsigned int size);
    void scale(short* buffer,int* tmp_buf,unsigned int size);

    void log_scale(short* out_buf,short* in_buf,
		   unsigned int size,float scale_factor);

public:
    AmMultiPartyMixer();
    ~AmMultiPartyMixer();
    
    unsigned int addChannel();
    void removeChannel(int channel_id);
    void PutChannelPacket(unsigned int   channel_id,
			  unsigned int   ts, 
			  unsigned char* buffer, 
			  unsigned int   size,
			  bool           begin_talk);

    void GetChannelPacket(unsigned int   channel,
			  unsigned int   ts,
			  unsigned char* buffer, 
 			  unsigned int   size);
};

#endif
// Local Variables:
// mode:C++
// End:

