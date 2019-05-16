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

#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmUtils.h"
#include "AmSdp.h"
#include "AmRtpStream.h"
#include "AmConfig.h"
#include "amci/codecs.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <typeinfo>

/** \brief structure to hold loaded codec instances */
struct CodecContainer
{
  amci_codec_t *codec;
  int frame_size;
  int frame_length;
  int frame_encoded_size;
  long h_codec;
};

AmAudioFormat::AmAudioFormat(int codec_id, unsigned int rate)
  : channels(1),
    sdp_format_parameters_out(NULL),
    codec_id(codec_id),
    rate(rate),
    codec(NULL)
{
  codec = getCodec();
}

AmAudioFormat::~AmAudioFormat()
{
  destroyCodec();
}

void AmAudioFormat::setRate(unsigned int sample_rate)
{
  rate = sample_rate;
}

unsigned int AmAudioFormat::calcBytesToRead(unsigned int needed_samples) const
{
  if (codec && codec->samples2bytes)
    return codec->samples2bytes(h_codec, needed_samples) * channels; // FIXME: channels

  WARN("Cannot convert samples to bytes\n");
  return needed_samples * channels;
}

unsigned int AmAudioFormat::bytes2samples(unsigned int bytes) const
{
  if (codec && codec->bytes2samples)
    return codec->bytes2samples(h_codec, bytes) / channels;
  WARN("Cannot convert bytes to samples\n");
  return bytes / channels;
}

bool AmAudioFormat::operator == (const AmAudioFormat& r) const
{
  return ( codec && r.codec
	   && (r.codec->id == codec->id) 
	   && (r.bytes2samples(1024) == bytes2samples(1024))
	   && (r.channels == channels)
	   && (r.rate == rate));
}

bool AmAudioFormat::operator != (const AmAudioFormat& r) const
{
  return !(this->operator == (r));
}

void AmAudioFormat::initCodec()
{
  amci_codec_fmt_info_t* fmt_i = NULL;
  sdp_format_parameters_out    = NULL; // reset

  if( codec && codec->init ) {
    if ((h_codec = (*codec->init)(sdp_format_parameters.c_str(), 
				  &sdp_format_parameters_out, &fmt_i)) == -1) {
      ERROR("could not initialize codec %i\n",codec->id);
    } else {
      if (NULL != sdp_format_parameters_out) {
	DBG("negotiated fmt parameters '%s'\n", sdp_format_parameters_out);
      }
    }
  } 
}

void AmAudioFormat::destroyCodec()
{
  if( codec && codec->destroy ){
    (*codec->destroy)(h_codec);
    h_codec = 0;
  }
  codec = NULL;
}

void AmAudioFormat::resetCodec() {
  codec = NULL;
  getCodec();
}

amci_codec_t* AmAudioFormat::getCodec()
{
  if(!codec){
    codec = AmPlugIn::instance()->codec(codec_id);
    initCodec();
  }
    
  return codec;
}

long AmAudioFormat::getHCodec()
{
  if(!codec)
    getCodec();
  return h_codec;
}

#ifdef USE_LIBSAMPLERATE
AmLibSamplerateResamplingState::AmLibSamplerateResamplingState()
  : resample_state(NULL), resample_buf_samples(0), resample_out_buf_samples(0)
{
}

AmLibSamplerateResamplingState::~AmLibSamplerateResamplingState()
{
  if (NULL != resample_state) {
    src_delete(resample_state);
    resample_state=NULL;
  }
}

unsigned int AmLibSamplerateResamplingState::resample(unsigned char* samples, unsigned int s, double ratio)
{
  DBG("resampling packet of size %d with ratio %f", s, ratio);
  if (!resample_state) {
    int src_error;
    // for better quality but more CPU usage, use SRC_SINC_ converters
    resample_state = src_new(SRC_LINEAR, 1, &src_error);
    if (!resample_state) {
      ERROR("samplerate initialization error: ");
    }
  }

  if (resample_state) {
    if (resample_buf_samples + PCM16_B2S(s) > PCM16_B2S(AUDIO_BUFFER_SIZE) * 2) {
      WARN("resample input buffer overflow! (%lu)\n", resample_buf_samples + PCM16_B2S(s));
    } else if (resample_out_buf_samples + (PCM16_B2S(s) * ratio) + 20 > PCM16_B2S(AUDIO_BUFFER_SIZE)) {
      WARN("resample: possible output buffer overflow! (%lu)\n", (resample_out_buf_samples + (size_t) ((PCM16_B2S(s) * ratio)) + 20));
    } else {
      signed short* samples_s = (signed short*)samples;
      src_short_to_float_array(samples_s, &resample_in[resample_buf_samples], PCM16_B2S(s));
      resample_buf_samples += PCM16_B2S(s);
    }

    SRC_DATA src_data;
    src_data.data_in = resample_in;
    src_data.input_frames = resample_buf_samples;
    src_data.data_out = &resample_out[resample_out_buf_samples];
    src_data.output_frames = PCM16_B2S(AUDIO_BUFFER_SIZE);
    src_data.src_ratio = ratio;
    src_data.end_of_input = 0;

    int src_err = src_process(resample_state, &src_data);
    if (src_err) {
      DBG("resample error: '%s'\n", src_strerror(src_err));
    }else {
      signed short* samples_s = (signed short*)(unsigned char*)samples;
      resample_out_buf_samples += src_data.output_frames_gen;
      s *= ratio;
      src_float_to_short_array(resample_out, samples_s, PCM16_B2S(s));
      DBG("resample: output_frames_gen = %ld", src_data.output_frames_gen);

      if (resample_buf_samples !=  (unsigned int)src_data.input_frames_used) {
	memmove(resample_in, &resample_in[src_data.input_frames_used],
		(resample_buf_samples - src_data.input_frames_used) * sizeof(float));
      }
      resample_buf_samples = resample_buf_samples - src_data.input_frames_used;

      if (resample_out_buf_samples != s) {
	memmove(resample_out, &resample_out[PCM16_B2S(s)], (resample_out_buf_samples - PCM16_B2S(s)) * sizeof(float));
      }
      resample_out_buf_samples -= PCM16_B2S(s);
    }
  }

  DBG("resample: output size is %d", s);
  return s;
}
#endif

#ifdef USE_INTERNAL_RESAMPLER
AmInternalResamplerState::AmInternalResamplerState()
  : rstate(NULL)
{
  rstate = ResampleFactory::createResampleObj(true, 4.0, ResampleFactory::INTERPOL_SINC, ResampleFactory::SAMPLE_MONO);
}

AmInternalResamplerState::~AmInternalResamplerState()
{
  if (rstate != NULL)
    ResampleFactory::destroyResampleObj(rstate);
}

unsigned int AmInternalResamplerState::resample(unsigned char *samples, unsigned int s, double ratio)
{
  if (rstate == NULL) {
    ERROR("Uninitialized resampling state");
    return s;
  }

  //DBG("Resampling with ration %f", ratio);
  //DBG("Putting %d samples in the buffer", PCM16_B2S(s));
  rstate->put_samples((signed short *)samples, PCM16_B2S(s));
  s = rstate->resample((signed short *)samples, ratio, PCM16_B2S(s) * ratio);
  //DBG("Returning %d samples", s);
  return PCM16_S2B(s);
}
#endif

AmAudio::AmAudio()
  : rec_time(0),
    max_rec_time(-1),
    fmt(new AmAudioFormat(CODEC_PCM16)),
    input_resampling_state(nullptr),
    output_resampling_state(nullptr)
{
}

AmAudio::AmAudio(AmAudioFormat *_fmt)
  : rec_time(0),
    max_rec_time(-1),
    fmt(_fmt),
    input_resampling_state(nullptr),
    output_resampling_state(nullptr)
{
}

AmAudio::~AmAudio()
{
  close();
}

void AmAudio::setFormat(AmAudioFormat* new_fmt) {
  fmt.reset(new_fmt);
  fmt->resetCodec();
}

void AmAudio::close()
{
}


// returns bytes read, else -1 if error (0 is OK)
int AmAudio::get(unsigned long long system_ts, unsigned char* buffer, 
		 int output_sample_rate, unsigned int nb_samples)
{
  int size = calcBytesToRead((int)((float)nb_samples * (float)getSampleRate()
				   / (float)output_sample_rate));

  unsigned int rd_ts = scaleSystemTS(system_ts);
  //DBG("\tread(rd_ts = %10.u; size = %u)\n",rd_ts,size);
  size = read(rd_ts,size);
  if(size <= 0){
    return size;
  }

  size = decode(size);
  if(size < 0) {
    DBG("decode returned %i\n",size);
    return -1; 
  }
  size = downMix(size);

  size = resampleOutput((unsigned char*)samples, size, 
			getSampleRate(), output_sample_rate);
  
  if(size>0)
    memcpy(buffer,(unsigned char*)samples,size);

  return size;
}

// returns bytes written, else -1 if error (0 is OK)
int AmAudio::put(unsigned long long system_ts, unsigned char* buffer, 
		 int input_sample_rate, unsigned int size)
{
  if(!size){
    return 0;
  }

  if(max_rec_time > -1 && rec_time >= max_rec_time)
    return -1;

  memcpy((unsigned char*)samples,buffer,size);
  size = resampleInput((unsigned char*)samples, size, 
		       input_sample_rate, getSampleRate());

  int s = encode(size);
  if(s>0){

    incRecordTime(bytes2samples(size));

    unsigned int wr_ts = scaleSystemTS(system_ts);
    //DBG("write(wr_ts = %10.u; s = %u)\n",wr_ts,s);
    return write(wr_ts,(unsigned int)s);
  }
  else{
    return s;
  }
}

void AmAudio::stereo2mono(unsigned char* out_buf,unsigned char* in_buf,unsigned int& size)
{
  short* in  = (short*)in_buf;
  short* end = (short*)(in_buf + size);
  short* out = (short*)out_buf;
    
  while(in != end){
    *(out++) = (*in + *(in+1)) / 2;
    in += 2;
  }

  size /= 2;
}

int AmAudio::decode(unsigned int size)
{
  int s = size;

  if(!fmt.get()){
    DBG("no fmt !\n");
    return s;
  }

  amci_codec_t* codec = fmt->getCodec();
  long h_codec = fmt->getHCodec();

  if(!codec){
    ERROR("audio format set, but no codec has been loaded\n");
    return -1;
  }

  if(codec->decode){
    s = (*codec->decode)(samples.back_buffer(),samples,s,
			 fmt->channels,getSampleRate(),h_codec);
    if(s<0) return s;
    samples.swap();
  }
    
  return s;
}

int AmAudio::encode(unsigned int size)
{
  int s = size;

  amci_codec_t* codec = fmt->getCodec();
  long h_codec = fmt->getHCodec();

  assert(codec);
  if(codec->encode){
    s = (*codec->encode)(samples.back_buffer(),samples,(unsigned int) size,
			 fmt->channels,getSampleRate(),h_codec);
    if(s<0) return s;
    samples.swap();
  }
    
  return s;
}

unsigned int AmAudio::downMix(unsigned int size)
{
  unsigned int s = size;
  if(fmt->channels == 2){
    stereo2mono(samples.back_buffer(),(unsigned char*)samples,s);
    samples.swap();
  } 

  return s;
}

unsigned int AmAudio::resampleInput(unsigned char* buffer, unsigned int s, int input_sample_rate, int output_sample_rate)
{
  if ((input_sample_rate == output_sample_rate) && !input_resampling_state.get()) {
    return s;
  }

  if (!input_resampling_state.get()) {
#ifdef USE_INTERNAL_RESAMPLER
    if (AmConfig::ResamplingImplementationType == AmAudio::INTERNAL_RESAMPLER) {
      DBG("using internal resampler for input");
      input_resampling_state.reset(new AmInternalResamplerState());
    } else
#endif
#ifdef USE_LIBSAMPLERATE
      if (AmConfig::ResamplingImplementationType == AmAudio::LIBSAMPLERATE) {
	input_resampling_state.reset(new AmLibSamplerateResamplingState());
      } else
#endif
	{
	  return s;
	}
  }

  return resample(*input_resampling_state, buffer, s, input_sample_rate, output_sample_rate);
}

unsigned int AmAudio::resampleOutput(unsigned char* buffer, unsigned int s, int input_sample_rate, int output_sample_rate)
{
  if ((input_sample_rate == output_sample_rate) 
      && !output_resampling_state.get()) {
    return s;
  }

  if (!output_resampling_state.get()) {
#ifdef USE_INTERNAL_RESAMPLER
    if (AmConfig::ResamplingImplementationType == AmAudio::INTERNAL_RESAMPLER) {
      DBG("using internal resampler for output");
      output_resampling_state.reset(new AmInternalResamplerState());
    } else
#endif
#ifdef USE_LIBSAMPLERATE
      if (AmConfig::ResamplingImplementationType == AmAudio::LIBSAMPLERATE) {
	output_resampling_state.reset(new AmLibSamplerateResamplingState());
      } else
#endif
	{
	  return s;
	}
  }

  return resample(*output_resampling_state, buffer, s, input_sample_rate, output_sample_rate);
}

unsigned int AmAudio::resample(AmResamplingState& rstate, unsigned char* buffer, unsigned int s, int input_sample_rate, int output_sample_rate)
{
  return rstate.resample((unsigned char*) buffer, s, ((double) output_sample_rate) / ((double) input_sample_rate));
}

int AmAudio::getSampleRate()
{
  if (!fmt.get())
    return 0;

  return fmt->getRate();
}

unsigned int AmAudio::scaleSystemTS(unsigned long long system_ts)
{
  // pre-division by 100 is important
  // so that the first multiplication
  // does not overflow the 64bit int
  unsigned long long user_ts =
    system_ts * ((unsigned long long)getSampleRate() / 100)
    / (WALLCLOCK_RATE / 100);
		 
  return (unsigned int)user_ts;
}

unsigned int AmAudio::calcBytesToRead(unsigned int nb_samples) const
{
  return fmt->calcBytesToRead(nb_samples);
}

unsigned int AmAudio::bytes2samples(unsigned int bytes) const
{
  return fmt->bytes2samples(bytes);
}

void AmAudio::setRecordTime(unsigned int ms)
{
  max_rec_time = (ms * (getSampleRate() / 100)) / 10;
}

int AmAudio::incRecordTime(unsigned int samples)
{
  return rec_time += samples;
}


DblBuffer::DblBuffer()
  : active_buf(0)
{ 
  memset(samples, 0, AUDIO_BUFFER_SIZE * 2);
}

DblBuffer::operator unsigned char*()
{
  return samples + (active_buf ? AUDIO_BUFFER_SIZE : 0);
}

unsigned char* DblBuffer::back_buffer()
{
  return samples + (active_buf ? 0 : AUDIO_BUFFER_SIZE);
}

void DblBuffer::swap()
{
  active_buf = !active_buf;
}
