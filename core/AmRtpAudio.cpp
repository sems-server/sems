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

AmAudioRtpFormat::AmAudioRtpFormat()
  : AmAudioFormat(-1),
    advertized_rate(-1)
{
}

AmAudioRtpFormat::~AmAudioRtpFormat()
{
}

int AmAudioRtpFormat::setCurrentPayload(Payload pl)
{
  if (this->codec_id != pl.codec_id) {
    this->codec_id = pl.codec_id;
    DBG("fmt.codec_id = %d", this->codec_id);
    this->channels = 1;
    this->rate = pl.clock_rate;
    DBG("fmt.rate = %d", this->rate);
    this->advertized_rate = pl.advertised_clock_rate;
    DBG("fmt.advertized_rate = %d", this->advertized_rate);
    this->frame_size = 20*this->rate/1000;
    if (this->codec != NULL) {
      destroyCodec();
    }
  }
  return 0;
}

void AmAudioRtpFormat::initCodec()
{
  amci_codec_fmt_info_t fmt_i[4];

  fmt_i[0].id=0;

  if( codec && codec->init ) {
    if ((h_codec = (*codec->init)(sdp_format_parameters.c_str(), fmt_i)) == -1) {
      ERROR("could not initialize codec %i\n",codec->id);
    } else {
      string s; 
      int i=0;
      while (fmt_i[i].id) {
	switch (fmt_i[i].id) {
	case AMCI_FMT_FRAME_LENGTH : {
	  //frame_length=fmt_i[i].value; 
	} break;
	case AMCI_FMT_FRAME_SIZE: {
	  frame_size=fmt_i[i].value; 
	} break;
	case AMCI_FMT_ENCODED_FRAME_SIZE: {
	//   frame_encoded_size=fmt_i[i].value; 
	} break;
	default: {
	  DBG("Unknown codec format descriptor: %d\n", fmt_i[i].id);
	} break;
	}
	i++;
      }
    }  
  } 
}


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

bool AmRtpAudio::checkInterval(unsigned long long ts)
{
  if(!last_check_i){
    send_int     = true;
    last_check_i = true;
    last_check   = ts;
  }
  else {
    if(scaleSystemTS(ts - last_check) >= getFrameSize()){
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

bool AmRtpAudio::sendIntReached(unsigned long long ts)
{
  if (!last_send_ts_i) return true;
  else return (scaleSystemTS(ts - last_send_ts) >= getFrameSize());
}

unsigned int AmRtpAudio::bytes2samples(unsigned int bytes) const
{
  return AmAudio::bytes2samples(bytes);
}
/* 
   @param wallclock_ts [in]    the current ts in the audio buffer 
*/
int AmRtpAudio::receive(unsigned long long system_ts) 
{
  int size;
  unsigned int rtp_ts;
  int new_payload = -1;

  if(!fmt.get()) {
    DBG("audio format not initialized\n");
    return RTP_ERROR;
  }

  unsigned int wallclock_ts = scaleSystemTS(system_ts);

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

    // This only works because the possible ratio (Rate/TSRate)
    // is 2. Rate and TSRate are only different in case of g722.
    // For g722, TSRate=8000 and Rate=16000
    //
    AmAudioRtpFormat* rtp_fmt = (AmAudioRtpFormat*)fmt.get();
    unsigned long long adjusted_rtp_ts = rtp_ts;

    if(rtp_fmt->getRate() != rtp_fmt->getTSRate()) {
      adjusted_rtp_ts =
	(unsigned long long)rtp_ts *
	(unsigned long long)rtp_fmt->getRate()
	/ (unsigned long long)rtp_fmt->getTSRate();
    }

    playout_buffer->write(wallclock_ts, adjusted_rtp_ts,
			  (ShortSample*)((unsigned char *)samples),
			  PCM16_B2S(size), begin_talk);
  }
  return size;
}

int AmRtpAudio::get(unsigned long long system_ts, unsigned char* buffer, 
		    int output_sample_rate, unsigned int nb_samples)
{
  if (!(receiving || getPassiveMode())) return 0; // like nothing received
  if (!active) return 0;
    
  int ret = receive(system_ts);
  if(ret < 0){
    switch(ret){

      case RTP_DTMF:
      case RTP_UNKNOWN_PL:
      case RTP_PARSE_ERROR:
        return 0; // like nothing received
        break;

      case RTP_TIMEOUT:
        //FIXME: postRequest(new SchedRequest(AmMediaProcessor::RemoveSession,s));
        // post to the session (FIXME: is session always set? seems to be...)
        session->postEvent(new AmRtpTimeoutEvent());
        return -1;
        break;

      case RTP_BUFFER_SIZE:
      default:
        ERROR("AmRtpAudio::receive() returned %i\n",ret);
        //FIXME: postRequest(new SchedRequest(AmMediaProcessor::ClearSession,s));
        //       or AmMediaProcessor::instance()->clearSession(session);
        return -1;
        break;
    }

    return 0; // like nothing received?
  }

  unsigned int user_ts = scaleSystemTS(system_ts);

  nb_samples = (unsigned int)((float)nb_samples * (float)getSampleRate()
			     / (float)output_sample_rate);

  u_int32_t size =
    PCM16_S2B(playout_buffer->read(user_ts,
				   (ShortSample*)((unsigned char*)samples),
				   nb_samples));
  if(output_sample_rate != getSampleRate()) {
    size = resampleOutput((unsigned char*)samples, size,
			  getSampleRate(), output_sample_rate);
  }
  
  memcpy(buffer,(unsigned char*)samples,size);

  return size;
}

int AmRtpAudio::put(unsigned long long system_ts, unsigned char* buffer, 
		    int input_sample_rate, unsigned int size)
{
  last_send_ts_i = true;
  last_send_ts = system_ts;

  if(!size){
    return 0;
  }

  if (mute) return 0;

  memcpy((unsigned char*)samples,buffer,size);
  size = resampleInput((unsigned char*)samples, size, 
		       input_sample_rate, getSampleRate());

  int s = encode(size);
  if(s<=0){
    return s;
  }

  AmAudioRtpFormat* rtp_fmt = (AmAudioRtpFormat*)fmt.get();

  // pre-division by 100 is important
  // so that the first multiplication
  // does not overflow the 64bit int
  unsigned long long user_ts =
    system_ts * ((unsigned long long)rtp_fmt->getTSRate() / 100)
    / (WALLCLOCK_RATE/100);

  return send((unsigned int)user_ts,(unsigned char*)samples,s);
}

void AmRtpAudio::getSdpOffer(unsigned int index, SdpMedia& offer)
{
  if (offer.type != MT_AUDIO) return;
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

  fec.reset(new LowcFE(getSampleRate()));

  if (m_playout_type == SIMPLE_PLAYOUT) {
    playout_buffer.reset(new AmPlayoutBuffer(this,getSampleRate()));
  } else if (m_playout_type == ADAPTIVE_PLAYOUT) {
    playout_buffer.reset(new AmAdaptivePlayout(this,getSampleRate()));
  } else {
    playout_buffer.reset(new AmJbPlayout(this,getSampleRate()));
  }
  return 0;
}

unsigned int AmRtpAudio::getFrameSize()
{
  if (!fmt.get())
    return 0;

  return ((AmAudioRtpFormat*)fmt.get())->getFrameSize();
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
		      fmt->channels,getSampleRate(),h_codec);

    DBG("codec specific PLC (ts_diff = %i; s = %i)\n",ts_diff,s);
  }
  else {
    s = default_plc(buffer, PCM16_S2B(ts_diff),
		    fmt->channels,getSampleRate());

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

  unsigned int sample_rate = getSampleRate();

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
	  playout_buffer.reset(new AmAdaptivePlayout(this,getSampleRate()));
	session->unlockAudio();
	DBG("Adaptive playout buffer activated\n");
      }
      else if (type == JB_PLAYOUT) {
	session->lockAudio();
	m_playout_type = type;
	if (fmt.get())
	  playout_buffer.reset(new AmJbPlayout(this,getSampleRate()));
	session->unlockAudio();
	DBG("Adaptive jitter buffer activated\n");
      }
      else {
	session->lockAudio();
	m_playout_type = type;
	if (fmt.get())
	  playout_buffer.reset(new AmPlayoutBuffer(this,getSampleRate()));
	session->unlockAudio();
	DBG("Simple playout buffer activated\n");
      }
    }
}
