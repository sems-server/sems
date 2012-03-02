/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmRtpAudio.h"
#include <sys/time.h>
#include <assert.h>
#include "AmSession.h"
#include "AmPlayoutBuffer.h"

AmRtpAudio::AmRtpAudio(AmSession* _s, int _if)
  : AmRtpStream(_s,_if), AmAudio(0), 
    /*last_ts_i(false),*/ use_default_plc(true),
    playout_buffer(NULL),
    last_check(0),last_check_i(false),send_int(false)
{
#ifdef USE_SPANDSP_PLC
  plc_state = plc_init(NULL);
#endif // USE_SPANDSP_PLC
}

AmRtpAudio::~AmRtpAudio() {
#ifdef USE_SPANDSP_PLC
  plc_release(plc_state);
#endif // USE_SPANDSP_PLC
}

bool AmRtpAudio::checkInterval(unsigned int ts)
{
  if(!last_check_i){
    send_int     = true;
    last_check_i = true;
    last_check   = ts;
  }
  else {
    if(((ts - last_check) / getSampleRateDivisor()) >= getFrameSize()){
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
   @param wallclock_ts [in]    the current ts in the audio buffer 
*/
int AmRtpAudio::receive(unsigned int wallclock_ts) 
{
  int size;
  unsigned int rtp_ts;
  int new_payload = -1;
  wallclock_ts /= getSampleRateDivisor();

  while(true) {
    size = AmRtpStream::receive((unsigned char*)samples,
				(unsigned int)AUDIO_BUFFER_SIZE, rtp_ts,
				new_payload);
    if(size <= 0)
      break;

    if (// don't process if we don't need to
	// ignore CN
	COMFORT_NOISE_PAYLOAD_TYPE == new_payload  ||
	// ignore packet if payload not found
	setCurrentPayload(new_payload)
	){
      playout_buffer->clearLastTs();
      continue;
    }

    size = decode(size);
    if(size <= 0){
      ERROR("decode() returned %i\n",size);
      return -1;
    }

    unsigned int adjusted_rtp_ts = rtp_ts * ((double)fmt->rate / (double)fmt->advertized_rate);
    playout_buffer->write(wallclock_ts, adjusted_rtp_ts, (ShortSample*)((unsigned char *)samples),
			  PCM16_B2S(size), begin_talk);
  }
  return size;
}

int AmRtpAudio::get(unsigned int ref_ts, unsigned char* buffer, int output_sample_rate, unsigned int nb_samples)
{
  assert(getSampleRate()==output_sample_rate); // resampling should not be done here

  ref_ts /= getSampleRateDivisor();
  
  int size = read(ref_ts,PCM16_S2B(nb_samples));
  memcpy(buffer,(unsigned char*)samples,size);
  return size;
}

int AmRtpAudio::read(unsigned int ref_ts, unsigned int size)
{
  u_int32_t rlen = 
    playout_buffer
    ->read(ref_ts,
	   (ShortSample*)((unsigned char*)samples),
	   PCM16_B2S(size));

  return PCM16_S2B(rlen);
}

int AmRtpAudio::write(unsigned int user_ts, unsigned int size)
{
  return send(user_ts,(unsigned char*)samples,size);
}

void AmRtpAudio::getSdpOffer(unsigned int index, SdpMedia& offer)
{
  offer.type = MT_AUDIO;
  AmRtpStream::getSdpOffer(index,offer);
}

void AmRtpAudio::getSdpAnswer(unsigned int index, 
			      const SdpMedia& offer, 
			      SdpMedia& answer)
{
  answer.type = MT_AUDIO;
  AmRtpStream::getSdpAnswer(index,offer,answer);
}

int AmRtpAudio::init(const AmSdp& local,
		     const AmSdp& remote)
{
  DBG("AmRtpAudio::init(...)\n");
  if(AmRtpStream::init(local,remote)){
    return -1;
  }

  AmAudioRtpFormat* fmt_p = new AmAudioRtpFormat();

  PayloadMappingTable::iterator pl_it = pl_map.find(payload);
  if ((pl_it == pl_map.end()) || (pl_it->second.remote_pt < 0)) {
    ERROR("no default payload has been set\n");
    return -1;
  }
  fmt_p->setCurrentPayload(payloads[pl_it->second.index]);
  fmt.reset(fmt_p);

  fec.reset(new LowcFE(fmt->rate));


  if (m_playout_type == SIMPLE_PLAYOUT) {
    playout_buffer.reset(new AmPlayoutBuffer(this,fmt->rate));
  } else if (m_playout_type == ADAPTIVE_PLAYOUT) {
    playout_buffer.reset(new AmAdaptivePlayout(this,fmt->rate));
  } else {
    playout_buffer.reset(new AmJbPlayout(this,fmt->rate));
  }
  return 0;
}

int AmRtpAudio::setCurrentPayload(int payload)
{
  if(payload != this->payload){
    PayloadMappingTable::iterator pmt_it = pl_map.find(payload);
    if(pmt_it == pl_map.end()){
      ERROR("Could not set current payload: payload %i unknown to this stream\n",payload);
      return -1;
    }
    
    unsigned char index = pmt_it->second.index;
    if(index >= payloads.size()){
      ERROR("Could not set current payload: payload %i maps to invalid index %i\n",
	    payload, index);
      return -1;
    }
    
    this->payload = payload;
    return ((AmAudioRtpFormat*)fmt.get())->setCurrentPayload(payloads[index]);
  }
  else {
    return 0;
  }
}

//int AmRtpAudio::getCurrentPayload() {
  //return ((AmAudioRtpFormat *) fmt.get())->getCurrentPayload();
//}

unsigned int AmRtpAudio::conceal_loss(unsigned int ts_diff, unsigned char *buffer)
{
  int s=0;
  if(!use_default_plc){

    amci_codec_t* codec = fmt->getCodec();
    long h_codec = fmt->getHCodec();

    assert(codec && codec->plc);
    s = (*codec->plc)(buffer, PCM16_S2B(ts_diff),
		      fmt->channels,fmt->rate,h_codec);

    DBG("codec specific PLC (ts_diff = %i; s = %i)\n",ts_diff,s);
  }
  else {
    s = default_plc(buffer, PCM16_S2B(ts_diff),
		    fmt->channels,fmt->rate);

    DBG("default PLC (ts_diff = %i; s = %i)\n",ts_diff,s);
  }
    
  return s;
}

unsigned int AmRtpAudio::default_plc(unsigned char* out_buf,
				     unsigned int   size,
				     unsigned int   channels,
				     unsigned int   sample_rate)
{
  short* buf_offset = (short*)out_buf;

#ifdef USE_SPANDSP_PLC
  plc_fillin(plc_state, buf_offset, PCM16_B2S(size));
#else
  for(unsigned int i=0; i<(PCM16_B2S(size)/FRAMESZ); i++){

    fec->dofe(buf_offset);
    buf_offset += FRAMESZ;
  }
#endif // USE_SPANDSP_PLC

  return PCM16_S2B(buf_offset - (short*)out_buf);
}

void AmRtpAudio::add_to_history(int16_t *buffer, unsigned int size)
{
  if (!use_default_plc)
    return;

#ifdef USE_SPANDSP_PLC
  plc_rx(plc_state, buffer, PCM16_B2S(size));
#else // USE_SPANDSP_PLC
  int16_t* buf_offset = buffer;

  unsigned int sample_rate = fmt->rate;

  for(unsigned int i=0; i<(PCM16_B2S(size)/FRAMESZ); i++){
    fec->addtohistory(buf_offset);
    buf_offset += FRAMESZ;
  }
#endif // USE_SPANDSP_PLC
}

void AmRtpAudio::setPlayoutType(PlayoutType type)
{
  if (m_playout_type != type)
    {
      if (type == ADAPTIVE_PLAYOUT) {
	session->lockAudio();
	m_playout_type = type;
	if (fmt.get())
	  playout_buffer.reset(new AmAdaptivePlayout(this,fmt->rate));
	session->unlockAudio();
	DBG("Adaptive playout buffer activated\n");
      }
      else if (type == JB_PLAYOUT) {
	session->lockAudio();
	m_playout_type = type;
	if (fmt.get())
	  playout_buffer.reset(new AmJbPlayout(this,fmt->rate));
	session->unlockAudio();
	DBG("Adaptive jitter buffer activated\n");
      }
      else {
	session->lockAudio();
	m_playout_type = type;
	if (fmt.get())
	  playout_buffer.reset(new AmPlayoutBuffer(this,fmt->rate));
	session->unlockAudio();
	DBG("Simple playout buffer activated\n");
      }
    }
}
