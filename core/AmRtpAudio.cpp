/*
 * $Id$
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
#include <assert.h>
#include "AmSession.h"

AmRtpAudio::AmRtpAudio(AmSession* _s)
    : AmRtpStream(_s), AmAudio(0), 
      last_ts_i(false), use_default_plc(true),
      send_only(false), playout_buffer(new AmPlayoutBuffer()),
      last_check(0),last_check_i(false),send_int(false)
{
}

bool AmRtpAudio::checkInterval(unsigned int ts)
{
    if(!last_check_i){
	send_int     = true;
	last_check_i = true;
	last_check   = ts;
    }
    else {
	if((ts - last_check) >= getFrameSize()){
	    send_int = true;
	    last_check = ts;
	}
	else {
	    send_int = false;
	}
    }

    return send_int;
}

bool AmRtpAudio::sendIntReached()
{
    return send_int;
}

unsigned int AmRtpAudio::bytes2samples(unsigned int bytes) const
{
    return AmAudio::bytes2samples(bytes);
}
/* 
   @param audio_buffer_ts [in]    the current ts in the audio buffer 
 */
int AmRtpAudio::receive(unsigned int audio_buffer_ts) 
{
#ifndef USE_ADAPTIVE_JB  
// using fifo or adaptive playout buffer
    int size;
    unsigned int ts;

    while(true) {
	size = AmRtpStream::receive((unsigned char*)samples, 
				    (unsigned int)AUDIO_BUFFER_SIZE,ts,
				    audio_buffer_ts);
	if(size <= 0)
	    break;
	
	if(send_only){
	    last_ts_i = false;
	    continue;
	}

	if(!last_ts_i){
	    last_ts = ts;
	    last_ts_i = true;
	}

	if(ts_less()(last_ts,ts) && !begin_talk
	   && (ts-last_ts <= PLC_MAX_SAMPLES)) {
	
	    int l_size = conceal_loss(ts - last_ts);
 	    if(l_size>0){

  		playout_buffer->direct_write(last_ts,
					     (ShortSample*)samples.back_buffer(),
					     PCM16_B2S(l_size));
 	    }
	}

	size = decode(size);
	if(size <= 0){
	    ERROR("decode() returned %i\n",size);
	    return -1;
	}

	if(use_default_plc)
	    add_to_history(size);

	playout_buffer->write(audio_buffer_ts, ts,
			      (ShortSample*)((unsigned char*)samples),
			      PCM16_B2S(size));
	// update last_ts to end of received packet 
	// if not out-of-sequence
	if (ts_less()(last_ts,ts) || last_ts == ts)
		last_ts = ts + PCM16_B2S(size);
    }
    
    return size;
#else // using adaptive JB
    int rtp_size;
    int audio_size;
    unsigned int ts;

    if (send_only) {
	last_ts_i = false;
	return 0;
    }
    /**
     * Receive all RTP packets that correspond to the required interval,
     * decode them and put into playout buffer.
     */
    while (true)
    {
	rtp_size = AmRtpStream::receive((unsigned char*)samples, 
				    (unsigned int)AUDIO_BUFFER_SIZE, &ts,
				    audio_buffer_ts, getFrameSize());
	if (rtp_size < 0) {
	    return rtp_size;
	}
	if (rtp_size == 0) { // No more RTP packets for this interval
	    break;
	}
	audio_size = decode(rtp_size);
	if (audio_size <= 0) {
	    ERROR("decode() returned %i\n", audio_size);
	    return -1;
	}
	playout_buffer->direct_write(ts,
				     (ShortSample*)((unsigned char*)samples),
				     PCM16_B2S(audio_size));
	/* Conceal the gap between previous and current RTP packets */
	if (last_ts_i && ts_less()(m_last_rtp_endts, ts))
       	{
	    int concealed_size = conceal_loss(ts - m_last_rtp_endts);
	    if (concealed_size > 0)
		playout_buffer->direct_write(m_last_rtp_endts,
		     (ShortSample*)((unsigned char*)samples.back_buffer()),
		     PCM16_B2S(concealed_size));
	}
	m_last_rtp_endts = ts + bytes2samples(rtp_size);
	last_ts_i = true;
	if(use_default_plc) {
	    add_to_history(audio_size);
	}
    }
    if (!last_ts_i) {
	return 0;
    }
    if (ts_less()(m_last_rtp_endts, audio_buffer_ts + getFrameSize()))
    {
	/* Last packets have been lost. Conceal them */
	int concealed_size = conceal_loss(audio_buffer_ts + getFrameSize() - m_last_rtp_endts);
	if (concealed_size > 0)
	    playout_buffer->direct_write(m_last_rtp_endts,
		    (ShortSample*)((unsigned char*)samples.back_buffer()),
		    PCM16_B2S(concealed_size));
	m_last_rtp_endts = audio_buffer_ts + getFrameSize();
    }
    
    return PCM16_S2B(getFrameSize());
#endif
}

int AmRtpAudio::get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples)
{
    int size = read(user_ts,PCM16_S2B(nb_samples));
    memcpy(buffer,(unsigned char*)samples,size);
    return size;
}

int AmRtpAudio::read(unsigned int user_ts, unsigned int size)
{
    u_int32_t rlen = 
	playout_buffer
	->read(user_ts,
	       (ShortSample*)((unsigned char*)samples),
	       PCM16_B2S(size));

    return PCM16_S2B(rlen);
}

int AmRtpAudio::write(unsigned int user_ts, unsigned int size)
{
    return send(user_ts,(unsigned char*)samples,size);
}

void AmRtpAudio::init(const SdpPayload* sdp_payload)
{
    DBG("AmRtpAudio::init(...)\n");
    AmRtpStream::init(sdp_payload);
    fmt.reset(new AmAudioRtpFormat(int_payload, format_parameters));

    amci_codec_t* codec = fmt->getCodec();
    use_default_plc = !(codec && codec->plc);
}

unsigned int AmRtpAudio::conceal_loss(unsigned int ts_diff)
{
    int s=0;
    if(!use_default_plc){

	amci_codec_t* codec = fmt->getCodec();
	long h_codec = fmt->getHCodec();

	assert(codec && codec->plc);
 	s = (*codec->plc)(samples.back_buffer(),PCM16_S2B(ts_diff),
 			  fmt->channels,fmt->rate,h_codec);

	DBG("codec specific PLC (ts_diff = %i; s = %i)\n",ts_diff,s);
    }
    else {
	s = default_plc(samples.back_buffer(),PCM16_S2B(ts_diff),
 			fmt->channels,fmt->rate);

	DBG("default PLC (ts_diff = %i; s = %i)\n",ts_diff,s);
    }
    
    return s;
}

unsigned int AmRtpAudio::default_plc(unsigned char* out_buf,
				     unsigned int   size,
				     unsigned int   channels,
				     unsigned int   rate)
{
    short* buf_offset = (short*)out_buf;

    for(unsigned int i=0; i<(PCM16_B2S(size)/FRAMESZ); i++){

	fec.dofe(buf_offset);
	buf_offset += FRAMESZ;
    }

    return PCM16_S2B(buf_offset - (short*)out_buf);
}

void AmRtpAudio::add_to_history(unsigned int size)
{
    short* buf_offset = (short*)((unsigned char*)samples);

    for(unsigned int i=0; i<(PCM16_B2S(size)/FRAMESZ); i++){

	fec.addtohistory(buf_offset);
	buf_offset += FRAMESZ;
    }
}

void AmRtpAudio::setAdaptivePlayout(bool on)
{
    bool is_adaptive = 
	dynamic_cast<AmAdaptivePlayout*>
	(playout_buffer.get()) != 0;

    if(on == !is_adaptive){

	if(!is_adaptive){
 	    session->lockAudio();
	    playout_buffer.reset(new AmAdaptivePlayout());
	    session->unlockAudio();
	    DBG("Adaptive playout buffer activated\n");
	}
	else{
 	    session->lockAudio();
	    playout_buffer.reset(new AmPlayoutBuffer());
	    session->unlockAudio();
	    DBG("Adaptive playout buffer deactivated\n");
	}
    }
}
