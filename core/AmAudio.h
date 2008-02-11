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
/** @file AmAudio.h */
#ifndef _AmAudio_h_
#define _AmAudio_h_

#include "AmThread.h"
#include "amci/amci.h"
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

#define PCM16_B2S(b) ((b) >> 1)
#define PCM16_S2B(s) ((s) << 1)

class SdpPayload;
class CodecContainer;

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
  /** Sampling rate. */
  int rate;
  /* frame length in samples (frame based codecs) */
  int frame_length;
  /* frame size in bytes */
  int frame_size;
  /* encoded frame size in bytes */
  int frame_encoded_size;

  string sdp_format_parameters;
    
  AmAudioFormat();
  virtual ~AmAudioFormat();

  /** @return The format's codec pointer. */
  amci_codec_t*    getCodec();
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
  virtual int getCodecId()=0;

  /** ==0 if not yet initialized. */
  amci_codec_t*   codec;
  /** ==0 if not yet initialized. */
  long            h_codec;

  /** Calls amci_codec_t::destroy() */
  void destroyCodec();
  /** Calls amci_codec_t::init() */
  void initCodec();

private:
  void operator = (const AmAudioFormat& r);
};

/** \brief simple \ref AmAudioFormat audio format */
class AmAudioSimpleFormat: public AmAudioFormat
{
  int codec_id;

protected:
  int getCodecId() { return codec_id; }

public:
  AmAudioSimpleFormat(int codec_id);
};


/** \brief RTP audio format */
class AmAudioRtpFormat: public AmAudioFormat
{
  vector<SdpPayload *> m_payloads;
  int m_currentPayload;
  amci_payload_t *m_currentPayloadP;
  std::map<int, SdpPayload *> m_sdpPayloadByPayload;
  std::map<int, amci_payload_t *> m_payloadPByPayload;
  std::map<int, CodecContainer *> m_codecContainerByPayload;

protected:
  int getCodecId();

public:
  /**
   * Constructor for payload based formats.
   * All the information are taken from the 
   * payload description in the originating plug-in.
   */
  AmAudioRtpFormat(const vector<SdpPayload *>& payloads);
  ~AmAudioRtpFormat();

  /**
   * changes payload. returns != 0 on error.
   */
  int setCurrentPayload(int payload);
};

/**
 * \brief base for classes that input or output audio.
 *
 * AmAudio binds a format and converts the samples if needed.
 * <br>Internal Format: PCM signed 16 bit (mono | stereo).
 */

class AmAudio
{
private:
  AmMutex fmt_mut;
  int rec_time; // in samples
  int max_rec_time;

#ifdef USE_LIBSAMPLERATE 
  SRC_STATE* resample_state;
  float resample_in[PCM16_B2S(AUDIO_BUFFER_SIZE)*2];
  float resample_out[PCM16_B2S(AUDIO_BUFFER_SIZE)];
  size_t resample_buf_samples;
#endif

protected:
  /** Sample buffer. */
  DblBuffer samples;
  
  /** Audio format. @see AmAudioFormat */
  auto_ptr<AmAudioFormat> fmt;

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
   * Get the number of bytes to read from encoded, depending on the format.
   */
  unsigned int calcBytesToRead(unsigned int needed_samples) const;

  /**
   * Convert the size from bytes to samples, depending on the format.
   */
  unsigned int bytes2samples(unsigned int bytes) const;

public:
  //bool begin_talk;

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
  virtual int get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples);

  /** 
   * Put some samples to the output stream.
   * @warning For packet based payloads / file formats, use:
   * <pre>           nb_sample = input buffer size / sample size of the reference format
   * </pre>           whereby the format with/from which the codec works is the reference one.
   * @return # bytes written, else -1 if error (0 is OK) 
   */
  virtual int put(unsigned int user_ts, unsigned char* buffer, unsigned int size);
  
  unsigned int getFrameSize();

  void setRecordTime(unsigned int ms);
  int  incRecordTime(unsigned int samples);

  void setBufferedOutput(unsigned int buffer_size);
};


#endif

// Local Variables:
// mode:C++
// End:



