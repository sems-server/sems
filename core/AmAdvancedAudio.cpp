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

#include "AmAdvancedAudio.h"

#include <set>
using std::set;

#include "math.h"

/* AudioQueue */
AmAudioQueue::AmAudioQueue() 
  : AmAudio(new AmAudioSimpleFormat(CODEC_PCM16)) // we get and put in this (internal) fmt
{
  sarr.clear_all();
}

AmAudioQueue::~AmAudioQueue() { 
  set<AmAudio*> deleted_audios; // don't delete them twice
  for (std::list<AudioQueueEntry>::iterator it = inputQueue.begin();it != inputQueue.end(); it++) {
    if (deleted_audios.find(it->audio) == deleted_audios.end()) {
      deleted_audios.insert(it->audio);
      delete it->audio;
    }
  }
	
  for (std::list<AudioQueueEntry>::iterator it = outputQueue.begin();it != outputQueue.end(); it++) {
    if (deleted_audios.find(it->audio) == deleted_audios.end()) {
      deleted_audios.insert(it->audio);
      delete it->audio;
    }
  }
}

int AmAudioQueue::write(unsigned int user_ts, unsigned int size) {
  inputQueue_mut.lock();
  unsigned int size_trav = size;
  for (std::list<AudioQueueEntry>::iterator it = inputQueue.begin(); it != inputQueue.end(); it++) {
    if (it->put) {
      if ((size_trav = it->audio->put(user_ts, samples, size_trav)) < 0)
	break;
    }
    if (it->get) {
      if ((size_trav = it->audio->get(user_ts, samples, size_trav >> 1)) < 0)
	break;
    }
  }
  inputQueue_mut.unlock();
  return size_trav;
}

int AmAudioQueue::read(unsigned int user_ts, unsigned int size) {
  outputQueue_mut.lock();
  unsigned int size_trav = size;
  for (std::list<AudioQueueEntry>::iterator it = outputQueue.begin(); it != outputQueue.end(); it++) {
    if (it->put) {
      if ((size_trav = it->audio->put(user_ts, samples, size_trav)) < 0)
	break;
    }
    if (it->get) {
      if ((size_trav = it->audio->get(user_ts, samples, size_trav >> 1)) < 0)
	break;
    }
  }
  outputQueue_mut.unlock();
  return size_trav;
}

void AmAudioQueue::pushAudio(AmAudio* audio, QueueType type, Pos pos, bool write, bool read) {
  AmMutex* q_mut; 
  std::list<AudioQueueEntry>* q; 
  switch (type) {
  case OutputQueue: 
    q_mut = &outputQueue_mut;
    q = &outputQueue;
    break;
  case InputQueue: 
  default:  q_mut = &inputQueue_mut;
    q = &inputQueue;
    break;
  };
  q_mut->lock();
  if (pos == Front)
    q->push_front(AudioQueueEntry(audio, write, read));
  else
    q->push_back(AudioQueueEntry(audio, write, read));
  q_mut->unlock();
}

int AmAudioQueue::popAudio(QueueType type, Pos pos) {
  AmAudio* audio = popAndGetAudio(type, pos);
  if (audio) {
    delete audio;
    return 0;
  }
  return -1; // error
}

AmAudio* AmAudioQueue::popAndGetAudio(QueueType type, Pos pos) {
  AmMutex* q_mut; 
  std::list<AudioQueueEntry>* q; 
  switch (type) {
  case OutputQueue: 
    q_mut = &outputQueue_mut;
    q = &outputQueue;
    break;
  case InputQueue: 
  default:  q_mut = &inputQueue_mut;
    q = &inputQueue;
    break;
  };
  q_mut->lock();
  if (q->empty()) {
    q_mut->unlock();
    return 0;
  }

  AmAudio* audio;
  if (pos == Front) {
    audio = q->front().audio;
    q->pop_front();
  }  else {
    audio = q->back().audio;
    q->pop_back();
  }
  q_mut->unlock();
  return audio;
}

int AmAudioQueue::removeAudio(AmAudio* audio) {
  bool found = false;
  outputQueue_mut.lock();
  for (std::list<AudioQueueEntry>::iterator it = outputQueue.begin(); 
       it != outputQueue.end(); it++) {
    if (it->audio == audio) {
      found = true;
      outputQueue.erase(it);
      break;
    }
	    
  }
  outputQueue_mut.unlock();
  if (found)
    return 0;
  inputQueue_mut.lock();
  for (std::list<AudioQueueEntry>::iterator it = inputQueue.begin(); 
       it != inputQueue.end(); it++) {
    if (it->audio == audio) {
      found = true;
      inputQueue.erase(it);
      break;
    }
	    
  }
  inputQueue_mut.unlock();
  if (found)
    return 0;
  else {
    ERROR("could not find audio in queue\n");
    return -1; // error
  }
}


/* AudioBridge */
AmAudioBridge::AmAudioBridge()
  : AmAudio(new AmAudioSimpleFormat(CODEC_PCM16))
{
  sarr.clear_all();
}

AmAudioBridge::~AmAudioBridge() { 
}

int AmAudioBridge::write(unsigned int user_ts, unsigned int size) {  
  sarr.write(user_ts, (short*) ((unsigned char*) samples), size >> 1); 
  return size; 
}

int AmAudioBridge::read(unsigned int user_ts, unsigned int size) { 
  sarr.read(user_ts, (short*) ((unsigned char*) samples), size >> 1); 
  return size;
}

/* AudioDelay */
AmAudioDelay::AmAudioDelay(float delay_sec)
  : AmAudio(new AmAudioSimpleFormat(CODEC_PCM16))
{
  sarr.clear_all();
  delay = delay_sec;
}

AmAudioDelay::~AmAudioDelay() { 
}

int AmAudioDelay::write(unsigned int user_ts, unsigned int size) {  
  sarr.write(user_ts,(short*) ((unsigned char*) samples), size >> 1); 
  return size; 
}

int AmAudioDelay::read(unsigned int user_ts, unsigned int size) { 
  sarr.read((unsigned int) (user_ts  - delay*8000.0), (short*)  ((unsigned char*) samples), size >> 1); 
  return size;
}

AmAudioFrontlist::AmAudioFrontlist(AmEventQueue* q) 
  : AmPlaylist(q), back_audio(NULL)
{
}

AmAudioFrontlist::AmAudioFrontlist(AmEventQueue* q, AmAudio* back_audio) 
  : AmPlaylist(q), back_audio(back_audio)
{
}

AmAudioFrontlist::~AmAudioFrontlist() {
//   ba_mut.lock();
//   if (back_audio)
//     back_audio->close();
//   ba_mut.unlock();
}

void AmAudioFrontlist::setBackAudio(AmAudio* new_ba) {
  ba_mut.lock();
  back_audio = new_ba;
  ba_mut.unlock();
}

int AmAudioFrontlist::put(unsigned int user_ts, unsigned char* buffer, unsigned int size) {
  // stay consistent with Playlist - if empty return size
  int res = size; 
  ba_mut.lock();

  if (isEmpty()) {
    if (back_audio) 
      res = back_audio->put(user_ts, buffer, size);
  } else {
    res = AmPlaylist::put(user_ts, buffer, size);
  }

  ba_mut.unlock();
  return res;
}

int AmAudioFrontlist::get(unsigned int user_ts, unsigned char* buffer, unsigned int size) {
  // stay consistent with Playlist - if empty return size
  int res = size; 

  ba_mut.lock();
  if (isEmpty()) {
    if (back_audio) 
      res = back_audio->get(user_ts, buffer, size);
  } else {
    res = AmPlaylist::get(user_ts, buffer, size);
  }
  ba_mut.unlock();
  return res;
}
