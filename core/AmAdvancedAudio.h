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

class AmAudioQueue : public AmAudio {
  SampleArrayShort sarr;

  AmMutex inputQueue_mut;
  std::list<AudioQueueEntry> inputQueue;
  AmMutex outputQueue_mut;
  std::list<AudioQueueEntry> outputQueue;
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

 protected:
  int write(unsigned int user_ts, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
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
class AmAudioFrontlist : public AmPlaylist {
  AmMutex ba_mut;
  AmAudio* back_audio;
 public:

  AmAudioFrontlist(AmEventQueue* q);
  AmAudioFrontlist(AmEventQueue* q, AmAudio* back_audio);
  ~AmAudioFrontlist();

  void setBackAudio(AmAudio* new_ba);

 protected:
  int put(unsigned int user_ts, unsigned char* buffer, unsigned int size);
  int get(unsigned int user_ts, unsigned char* buffer, unsigned int size);
};


/**
 * \brief \ref AmAudio that directly connects input and output
 *
 *  AmAudioBridge simply connects input and output
 *  This is useful e.g. at the end of a AudioQueue
 */
class AmAudioBridge : public AmAudio {
  SampleArrayShort sarr;
 public:
  AmAudioBridge();
  ~AmAudioBridge();
 protected:
  int write(unsigned int user_ts, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
};

/**
 * \brief \ref AmAudio that delays output from input
 * delays delay_sec seconds (up to ~2)
 */
class AmAudioDelay : public AmAudio {
  SampleArrayShort sarr;
  float delay;
 public:
  AmAudioDelay(float delay_sec);
  ~AmAudioDelay();
 protected:
  int write(unsigned int user_ts, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
};

#endif // _AmAdvancedAudio_h_

