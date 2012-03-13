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
/** @file AmAudio.h */
#ifndef _AmAudio_h_
#define _AmAudio_h_

#include "AmThread.h"
#include "amci/amci.h"
#include "amci/codecs.h"
#include "AmEventQueue.h"

#include <stdio.h>

#include <memory>
using std::auto_ptr;
#include <string>
using std::string;
#include <map>

#ifdef USE_LIBSAMPLERATE
#include <samplerate.h>
#endif

#ifdef USE_INTERNAL_RESAMPLER
#include "resample/resample.h"
#endif

#define PCM16_B2S(b) ((b) >> 1)
#define PCM16_S2B(s) ((s) << 1)

//#define SYSTEM_SAMPLERATE 8000 // fixme: sr per session
#ifndef SYSTEM_SAMPLECLOCK_RATE
#define SYSTEM_SAMPLECLOCK_RATE 32000
#endif

// Wallclock definitions:
//
// The wallclock is defined such that:
//  - it is the highest clock rate is the system
//  - any supported sample rate must be smaller
//  - the difference between scaled down timers
//    is always consistent with respect to overflows.
//  - supported sample rates are multiples of 100
//    (44100 is supported, 44110 is not)
#define WALLCLOCK_RATE 102400LL
//
// Wallclock overflow mask
#define WALLCLOCK_MASK 0xFFFFFFFFFFFFLL // 48 bit mask
//
// Wallclock increments
#define WC_INC_MS 10LL /* 10 ms */
#define WC_INC ((WALLCLOCK_RATE*WC_INC_MS)/1000LL)

struct SdpPayload;
struct CodecContainer;
struct Payload;

/** \brief Audio Event */
class AmAudioEvent: public AmEvent
{
public:
  enum EventType {
	
    noAudio, // Audio class has nothing to play and/or record anymore

    // Audio input & output have been cleared: 
    // !!! sent only from AmSession !!!
    cleared  
  };

  AmAudioEvent(int id):AmEvent(id){}
  virtual ~AmAudioEvent() { }
};


/**
 * \brief double buffer with back and front
 * Implements double buffering.
 */

class DblBuffer
{
  /** Buffer. */
  unsigned char samples[AUDIO_BUFFER_SIZE * 2];
  /** 0 for first buffer, 1 for the second. */
  int active_buf;

public:
  /** Constructs a double buffer. */
  DblBuffer();
  /** Returns a pointer to the current front buffer. */
  operator unsigned char*();
  /** Returns a pointer to the current back buffer. */
  unsigned char* back_buffer();
  /** swaps front and back buffer. */
  void swap();
};

class AmAudio;

/**
 * \brief Audio format structure.
 * Holds a description of the format.
 * @todo Create two child class:
 * <ul>
 *   <li>file based format
 *   <li>payload based format
 * </ul>
 */

class AmAudioFormat
{
public:
  /** Number of channels. */
  int channels;

  string sdp_format_parameters;
    
  AmAudioFormat(int codec_id = CODEC_PCM16,
		unsigned int rate = SYSTEM_SAMPLECLOCK_RATE);

  virtual ~AmAudioFormat();

  /** @return The format's codec pointer. */
  virtual amci_codec_t* getCodec();
  void resetCodec();

  /** return the sampling rate */
  unsigned int getRate() { return rate; }

  /** set the sampling rate */
  void setRate(unsigned int sample_rate);

  /** @return Handler returned by the codec's init function.*/
  long             getHCodec();
  long             getHCodecNoInit() { return h_codec; } // do not initialize

  unsigned int calcBytesToRead(unsigned int needed_samples) const;
  unsigned int bytes2samples(unsigned int) const;

  /** @return true if same format. */
  bool operator == (const AmAudioFormat& r) const;
  /** @return false if same format. */
  bool operator != (const AmAudioFormat& r) const;

protected:
  /** Codec id: @see amci/codecs.h */
  int codec_id;

  /** Sampling rate. */
  unsigned int rate;

  /** ==0 if not yet initialized. */
  amci_codec_t*   codec;
  /** ==0 if not yet initialized. */
  long            h_codec;

  /** Calls amci_codec_t::destroy() */
  void destroyCodec();
  /** Calls amci_codec_t::init() */
  virtual void initCodec();

private:
  void operator = (const AmAudioFormat& r);
};

/**
 * \brief keeps the resampling state for one direction (input or output)
 */
class AmResamplingState
{
public:
  virtual unsigned int resample(unsigned char* samples, unsigned int size, double ratio) = 0;
  virtual ~AmResamplingState() {}
};

#ifdef USE_LIBSAMPLERATE
class AmLibSamplerateResamplingState: public AmResamplingState
{
private:
  SRC_STATE* resample_state;
  float resample_in[PCM16_B2S(AUDIO_BUFFER_SIZE)*2];
  float resample_out[PCM16_B2S(AUDIO_BUFFER_SIZE)];
  size_t resample_buf_samples;
  size_t resample_out_buf_samples;
public:
  AmLibSamplerateResamplingState();
  virtual ~AmLibSamplerateResamplingState();

  virtual unsigned int resample(unsigned char* samples, unsigned int size, double ratio);
};
#endif

#ifdef USE_INTERNAL_RESAMPLER
class AmInternalResamplerState: public AmResamplingState
{
private:
  Resample *rstate;

public:
  AmInternalResamplerState();
  virtual ~AmInternalResamplerState();

  virtual unsigned int resample(unsigned char* samples, unsigned int size, double ratio);
};
#endif

/**
 * \brief base for classes that input or output audio.
 *
 * AmAudio binds a format and converts the samples if needed.
 * <br>Internal Format: PCM signed 16 bit (mono | stereo).
 */

class AmAudio
  : public AmObject
{
private:
  int rec_time; // in samples
  int max_rec_time;

public:
  enum ResamplingImplementationType {
	LIBSAMPLERATE,
	INTERNAL_RESAMPLER,
	UNAVAILABLE
  };

protected:
  /** Sample buffer. */
  DblBuffer samples;
  
  /** Audio format. @see AmAudioFormat */
  auto_ptr<AmAudioFormat> fmt;
  
  /** Resampling states. @see AmResamplingState */
  auto_ptr<AmResamplingState> input_resampling_state;
  auto_ptr<AmResamplingState> output_resampling_state;

  AmAudio();
  AmAudio(AmAudioFormat *);

  /** Gets 'size' bytes directly from stream (Read,Pull). */
  virtual int read(unsigned int user_ts, unsigned int size) = 0;
  /** Puts 'size' bytes directly from stream (Write,Push). */
  virtual int write(unsigned int user_ts, unsigned int size) = 0;

  /** 
   * Converts a buffer from stereo to mono. 
   * @param size [in,out] size in bytes
   * <ul><li>Before call is size = input size</li><li>After the call is size = output size</li></ul>
   */
  void stereo2mono(unsigned char* out_buf,unsigned char* in_buf,unsigned int& size);

  /**
   * Converts from the input format to the internal format.
   * <ul><li>input = front buffer</li><li>output = back buffer</li></ul>
   * @param size [in] size in bytes
   * @return new size in bytes
   */
  int decode(unsigned int size);
  /**
   * Converts from the internal format to the output format.
   * <ul><li>input = front buffer</li><li>output = back buffer</li></ul>
   * @param size [in] size in bytes
   * @return new size in bytes
   */
  int encode(unsigned int size);

  /**
   * Converts to mono depending on the format.
   * @return new size in bytes
   */
  unsigned int downMix(unsigned int size);

  /**
   * Resamples from the given input sample rate to the given output sample rate
   * using the input resampling state. The input resampling state is created if
   * it does not exist.
   *
   */
  unsigned int resampleInput(unsigned char *buffer, unsigned int size, int input_sample_rate, int output_sample_rate);

  /**
   * Resamples from the given input sample rate to the given output sample rate
   * using the output resampling state. The output resampling state is created if
   * it does not exist.
   *
   */
  unsigned int resampleOutput(unsigned char *buffer, unsigned int size, int input_sample_rate, int output_sample_rate);

  /**
   * Resamples from the given input sample rate to the given output sample rate using
   * the given resampling state.
   * <ul><li>input = front buffer</li><li>output = back buffer</li></ul>
   * @param rstate resampling state to be used
   * @param size [in] size in bytes
   * @param output_sample_rate desired output sample rate
   * @return new size in bytes
   */
  unsigned int resample(AmResamplingState& rstate, unsigned char *buffer, unsigned int size, int input_sample_rate, int output_sample_rate);
   
  /**
   * Get the number of bytes to read from encoded, depending on the format.
   */
  unsigned int calcBytesToRead(unsigned int needed_samples) const;

  /**
   * Convert the size from bytes to samples, depending on the format.
   */
  unsigned int bytes2samples(unsigned int bytes) const;

  /**
   * Scale a system timestamp down dependent on the sample rate.
   */
  unsigned int scaleSystemTS(unsigned long long system_ts);

public:
  /** Destructor */
  virtual ~AmAudio();

  /** Closes the audio pipe. */
  virtual void close();

  /** 
   * Get some samples from input stream.
   * @warning For packet based payloads / file formats, use:
   * <pre>           nb_sample = input buffer size / sample size of the reference format
   * </pre>           whereby the format with/from which the codec works is the reference one.
   * @return # bytes read, else -1 if error (0 is OK) 
   */
  virtual int get(unsigned long long system_ts, unsigned char* buffer, 
		  int output_sample_rate, unsigned int nb_samples);

  /** 
   * Put some samples to the output stream.
   * @warning For packet based payloads / file formats, use:
   * <pre>           nb_sample = input buffer size / sample size of the reference format
   * </pre>           whereby the format with/from which the codec works is the reference one.
   * @return # bytes written, else -1 if error (0 is OK) 
   */
  virtual int put(unsigned long long system_ts, unsigned char* buffer, 
		  int input_sample_rate, unsigned int size);
  
  int  getSampleRate();

  void setRecordTime(unsigned int ms);
  int  incRecordTime(unsigned int samples);

  void setBufferedOutput(unsigned int buffer_size);

  void setFormat(AmAudioFormat* new_fmt);
};


#endif

// Local Variables:
// mode:C++
// End:



