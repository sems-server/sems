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
/** @file AmAdvancedAudio.h */

#ifndef _AmAdvancedAudio_h_
#define _AmAdvancedAudio_h_

#include "AmAudio.h"
#include "AmPlaylist.h"

#include "AmThread.h"
#include "amci/codecs.h"

#include <list>
#include "SampleArray.h"

/**
 * \brief Entry in an AudioQueue
 */
struct AudioQueueEntry {
  AmAudio* audio;
  bool put;
  bool get;
  AudioQueueEntry(AmAudio* _audio, bool _put, bool _get) 
    : audio(_audio), put(_put), get(_get) { }
};

/**
 * \brief Holds AmAudios and reads/writes through all
 * 
 * AmAudioQueue can hold AmAudios in input and output queue.
 * Audio will be read through the whole output queue,
 * and written through the whole input queue.
 */

class AmAudioQueue : public AmAudio 
{

  AmMutex inputQueue_mut;
  std::list<AudioQueueEntry> inputQueue;
  AmMutex outputQueue_mut;
  std::list<AudioQueueEntry> outputQueue;

  bool owning;

public:
  AmAudioQueue();
  ~AmAudioQueue();

  enum QueueType { OutputQueue, InputQueue };
  enum Pos { Front, Back };

  /** add an audio to a queue */
  void pushAudio(AmAudio* audio, QueueType type, Pos pos, bool write, bool read); 
  /** pop an audio from queue and delete it @return 0 on success, -1 on failure */
  int popAudio(QueueType type, Pos pos); 
  /** pop an audio from queue @return pointer to the audio */
  AmAudio* popAndGetAudio(QueueType type, Pos pos); 
  /** this removes the audio if it is in on of the queues and does not
      delete them */
  int removeAudio(AmAudio* audio);
  void setOwning(bool _owning);

  /** AmAudio interface */
  int get(unsigned long long system_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int nb_samples);
  int put(unsigned long long system_ts, unsigned char* buffer, 
	  int input_sample_rate, unsigned int size);

protected:
  /** Fake implementation to satifsy AmAudio */
  int write(unsigned int user_ts, unsigned int size) { return 0; }
  int read(unsigned int user_ts, unsigned int size) { return 0; }
};

/**
 * \brief AmAudio device with a playlist and a background AmAudio
 *
 * AmAudioFrontlist is an AmAudio device, that has a playlist
 * in front of a AmAudio entry, the 'back' device. The back device
 * is only used if the playlist is empty. - This can be useful when 
 * for example announcements should be played to the participant 
 * while in a conference.
 *
 */
class AmAudioFrontlist : public AmPlaylist 
{
  AmMutex ba_mut;
  AmAudio* back_audio;

public:
  AmAudioFrontlist(AmEventQueue* q);
  AmAudioFrontlist(AmEventQueue* q, AmAudio* back_audio);
  ~AmAudioFrontlist();

  void setBackAudio(AmAudio* new_ba);

  int put(unsigned long long system_ts, unsigned char* buffer, 
	  int input_sample_rate, unsigned int size);

  int get(unsigned long long user_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int size);
};


/**
 * \brief \ref AmAudio that directly connects input and output
 *
 *  AmAudioBridge simply connects input and output
 *  This is useful e.g. at the end of a AudioQueue
 */
class AmAudioBridge : public AmAudio 
{
  SampleArrayShort sarr;

public:
  AmAudioBridge(unsigned int sample_rate = SYSTEM_SAMPLECLOCK_RATE);
  ~AmAudioBridge();

  int write(unsigned int user_ts, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
};

/**
 * \brief \ref AmAudio that delays output from input
 * delays delay_sec seconds (up to ~2)
 */
class AmAudioDelay : public AmAudio 
{
  SampleArrayShort sarr;
  float delay;

public:
  AmAudioDelay(float delay_sec = 0.0,
	       unsigned int sample_rate = SYSTEM_SAMPLECLOCK_RATE);
  ~AmAudioDelay();

  void setDelay(float delay) { this->delay = delay; }

  int write(unsigned int user_ts, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
};

/** 
 * AmNullAudio plays silence, and recording goes to void.
 * it can be parametrized with a maximum length (in milliseconds),
 * after which it is ended.
 * Read and write length can also be set after creation (and possibly even
 * when in use).
 */
class AmNullAudio : public AmAudio 
{
  int read_msec;
  int write_msec;

  bool read_end_ts_i;
  unsigned long long read_end_ts;

  bool write_end_ts_i;
  unsigned long long write_end_ts;

public:
  AmNullAudio(int read_msec = -1, int write_msec = -1)
    : read_msec(read_msec), write_msec(write_msec),
    read_end_ts_i(false), write_end_ts_i(false) { }
  ~AmNullAudio() { }

  /** (re) set maximum read length*/
  void setReadLength(int n_msec);
  /** (re) set maximum write length*/
  void setWriteLength(int n_msec);

  /** AmAudio interface */
  int get(unsigned long long system_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int nb_samples);
  int put(unsigned long long system_ts, unsigned char* buffer, 
	  int input_sample_rate, unsigned int size);

protected:
  /** Fake implementation to satifsy AmAudio */
  int write(unsigned int user_ts, unsigned int size) { return 0; }
  int read(unsigned int user_ts, unsigned int size) { return 0; }
};

#endif // _AmAdvancedAudio_h_

