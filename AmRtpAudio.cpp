/*
 * $Id: AmRtpAudio.cpp,v 1.1.2.7 2005/06/01 12:00:24 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
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

#include "AmRtpAudio.h"
#include <sys/time.h>

AmRtpAudio::AmRtpAudio(AmSession* _s)
 : AmRtpStream(_s), AmAudio(0)
{
}

/* 
   @param audio_buffer_ts [in]    the current ts in the audio buffer 
 */
int AmRtpAudio::receive(unsigned int audio_buffer_ts) 
{
    int ret = 1;

    while(ret > 0){
	unsigned int ts;
	ret = AmRtpStream::receive((unsigned char*)samples, 
				   (unsigned int)AUDIO_BUFFER_SIZE,ts,
				   audio_buffer_ts);
	if(ret <= 0)
	    break;
	
	int size = decode(ret);
	if(size <= 0){
	    ERROR("decode() returned %i\n",size);
	    return -1;
	}
	timed_buffer.put(ts,(ShortSample*)((unsigned char*)samples),size >> 1);
    }
    
    return ret;
}

int AmRtpAudio::get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples)
{
    int size = read(user_ts,nb_samples<<1);
    memcpy(buffer,(unsigned char*)samples,size);
    return size;
}

int AmRtpAudio::read(unsigned int user_ts, unsigned int size)
{
    timed_buffer.get(user_ts,(ShortSample*)((unsigned char*)samples),size>>1);
    return size;
}

int AmRtpAudio::write(unsigned int user_ts, unsigned int size)
{
    send(user_ts,(unsigned char*)samples,size);
    return 0;
}

void AmRtpAudio::init(const SdpPayload* sdp_payload)
{
    AmRtpStream::init(sdp_payload);
    fmt.reset(new AmAudioRtpFormat(int_payload, format_parameters));
}

