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

  unsigned int samples2bytes(unsigned int) const;
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

/** \brief \ref AmAudioFormat for file */
class AmAudioFileFormat: public AmAudioFormat
{
  /** == "" if not yet initialized. */
  string          name;
    
  /** == -1 if not yet initialized. */
  int             subtype;

  /** ==  0 if not yet initialized. */
  amci_subtype_t* p_subtype;

protected:
  int getCodecId();

public:
  /**
   * Constructor for file based formats.
   * All information are taken from the plug-in description.
   * @param name The file format name (ex. "Wav").
   * @param subtype Subtype for the file format (see amci.h).
   */
  AmAudioFileFormat(const string& name, int subtype = -1);
  /**
   * Constructor for file based formats.
   * All information are taken from the file descriptor.
   * @param name The file format name (ex. "Wav").
   * @param fd A file descriptor filled by the a plug-in's open function.
   */
  AmAudioFileFormat(const string& name, const amci_file_desc_t* fd);

  /** @return Format name. */
  string        getName() { return name; }
  /** @return Format subtype. */
  int           getSubtypeId() { return subtype; }
  /** @return Subtype pointer. */
  amci_subtype_t*  getSubtype();

  void setSubtypeId(int subtype_id) 
  { 
    destroyCodec();
    subtype = subtype_id; 
    p_subtype = 0;
    codec = getCodec();
  }
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
   * Convert the size from samples to bytes, depending on the format.
   */
  unsigned int samples2bytes(unsigned int nb_samples) const;

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
};

/**
 * \brief AmAudio implementation for file access
 */
class AmAudioFile: public AmAudio
{
public:
  /** Open mode. */
  enum OpenMode { Read=1, Write=2 };

protected:
  /** Pointer to the file opened as last. */
  FILE* fp;
  long begin;

  /** Format of that file. @see fp, open(). */
  amci_inoutfmt_t* iofmt;
  /** Open mode. */
  int open_mode;

  /** Size of datas having been read/written until now. */
  int data_size;

  bool on_close_done;
  bool close_on_exit;

  /** @see AmAudio::read */
  int read(unsigned int user_ts, unsigned int size);

  /** @see AmAudio::write */
  int write(unsigned int user_ts, unsigned int size);

  /** @return a file format from file name. (ex: '1234.wav') */
  AmAudioFileFormat* fileName2Fmt(const string& name);

public:
  AmSharedVar<bool> loop;

  AmAudioFile();
  ~AmAudioFile();

  /**
   * Opens a file.
   * <ul>
   * <li>In read mode: sets input format.
   * <li>In write mode: <ol>
   *                    <li>needs output format set. 
   *                    <li>If file name already exists, 
   *                        the file will be overwritten.
   *                    </ol>
   * </ul>
   * @param filename Name of the file.
   * @param mode Open mode.
   * @return 0 if everything's OK
   * @see OpenMode
   */
  int open(const string& filename, OpenMode mode, 
	   bool is_tmp=false);

  int fpopen(const string& filename, OpenMode mode, FILE* n_fp);

  /**
   * Rewind the file.
   */
  void rewind();

  /** Closes the file. */
  void close();

  /** Executes the handler's on_close. */
  void on_close();

  /** be carefull with this one ;-) */ 
  FILE* getfp() { return fp; }

  OpenMode getMode() { return (OpenMode)open_mode; }

  /** Gets data size in the current file */
  int getDataSize() { return data_size; }

  /** Gets length of the current file in ms */
  int getLength();

  /**
   * @return MIME type corresponding to the audio file.
   */
  string getMimeType();

  void setCloseOnDestroy(bool cod){
    close_on_exit = cod;
  }
};

#endif

// Local Variables:
// mode:C++
// End:



