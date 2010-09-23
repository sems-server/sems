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

AmRtpAudio::AmRtpAudio(AmSession* _s)
  : AmRtpStream(_s), AmAudio(0), 
    /*last_ts_i(false),*/ use_default_plc(true),
    send_only(false), playout_buffer(new AmPlayoutBuffer(this)),
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

bool AmRtpAudio::checkInterval(unsigned int ts, unsigned int frame_size)
{
  if(!last_check_i){
    send_int     = true;
    last_check_i = true;
    last_check   = ts;
  }
  else {
    if((ts - last_check) >= frame_size){
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

  while(true) {
    int payload;
    size = AmRtpStream::receive((unsigned char*)samples,
				(unsigned int)AUDIO_BUFFER_SIZE, rtp_ts,
				payload);
    if(size <= 0)
      break;

    if (// don't process if we don't need to 
	send_only || 
	// ignore CN
	COMFORT_NOISE_PAYLOAD_TYPE == payload  || 
	// ignore packet if payload not found
	setCurrentPayload(payload)
	){
      playout_buffer->clearLastTs();
      continue;
    }

    size = decode(size);
    if(size <= 0){
      ERROR("decode() returned %i\n",size);
      return -1;
    }

    playout_buffer->write(wallclock_ts, rtp_ts, (ShortSample*)((unsigned char *)samples), 
			  PCM16_B2S(size), begin_talk);
  }
  return size;
}

int AmRtpAudio::get(unsigned int ref_ts, unsigned char* buffer, unsigned int nb_samples)
{
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

void AmRtpAudio::init(const vector<SdpPayload*>& sdp_payloads)
{
  DBG("AmRtpAudio::init(...)\n");
  AmRtpStream::init(sdp_payloads);
  fmt.reset(session->getNewRtpFormat());
}

int AmRtpAudio::setCurrentPayload(int payload)
{
  int res = 
    ((AmAudioRtpFormat *) fmt.get())->setCurrentPayload(payload);
  if (!res) {
    amci_codec_t* codec = fmt->getCodec();
    use_default_plc = !(codec && codec->plc);
  }
  return res;
}

int AmRtpAudio::getCurrentPayload() {
  return ((AmAudioRtpFormat *) fmt.get())->getCurrentPayload();
}

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
				     unsigned int   rate)
{
  short* buf_offset = (short*)out_buf;

#ifdef USE_SPANDSP_PLC
  plc_fillin(plc_state, buf_offset, PCM16_B2S(size));
#else
  for(unsigned int i=0; i<(PCM16_B2S(size)/FRAMESZ); i++){

    fec.dofe(buf_offset);
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

  for(unsigned int i=0; i<(PCM16_B2S(size)/FRAMESZ); i++){
    fec.addtohistory(buf_offset);
    buf_offset += FRAMESZ;
  }
#endif // USE_SPANDSP_PLC
}

void AmRtpAudio::setPlayoutType(PlayoutType type)
{
  PlayoutType curr_type = SIMPLE_PLAYOUT;
  if (dynamic_cast<AmAdaptivePlayout *>(playout_buffer.get()))
    curr_type = ADAPTIVE_PLAYOUT;
  else if (dynamic_cast<AmJbPlayout *>(playout_buffer.get()))
    curr_type = JB_PLAYOUT;

  if (curr_type != type)
    {
      if (type == ADAPTIVE_PLAYOUT) {
	session->lockAudio();
	playout_buffer.reset(new AmAdaptivePlayout(this));
	session->unlockAudio();
	DBG("Adaptive playout buffer activated\n");
      }
      else if (type == JB_PLAYOUT) {
	session->lockAudio();
	playout_buffer.reset(new AmJbPlayout(this));
	session->unlockAudio();
	DBG("Adaptive jitter buffer activated\n");
      }
      else {
	session->lockAudio();
	playout_buffer.reset(new AmPlayoutBuffer(this));
	session->unlockAudio();
	DBG("Simple playout buffer activated\n");
      }
    }
}
