/*
 * Copyright (C) 2006 iptelorg GmbH
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
/** @file AmPlaylist.h */
#ifndef AmPlaylist_h
#define AmPlaylist_h

#include "AmAudio.h"
#include "AmThread.h"
#include "AmEventQueue.h"

#include <deque>
using std::deque;
/** \brief entry in an \ref AmPlaylist */
struct AmPlaylistItem
{

  AmAudio* play;
  AmAudio* record;

  AmPlaylistItem(AmAudio* play,
		 AmAudio* record)
    : play(play), record(record) {}

  virtual ~AmPlaylistItem() { }
};

/**
 * \brief AmAudio component that plays entries serially 
 * 
 * This class can be plugged to the Input or Output of a 
 * session. Entries can be added or removed from the playlist,
 * and the current entry is played until it is finished,
 * then the next entry is played.
 */
class AmPlaylist: public AmAudio
{
    
  AmMutex                items_mut;
  deque<AmPlaylistItem*> items;

  AmMutex                cur_mut;
  AmPlaylistItem*        cur_item;

  AmEventQueue*          ev_q;

  void updateCurrentItem();
  void gotoNextItem(bool notify);
    
 protected:
  /** Fake implement AmAudio's pure virtual methods */
  int read(unsigned int user_ts, unsigned int size){ return -1; }
  int write(unsigned int user_ts, unsigned int size){ return -1; }

  /** override AmAudio */
  int get(unsigned long long system_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int nb_samples);

  int put(unsigned long long system_ts, unsigned char* buffer, 
	  int input_sample_rate, unsigned int size);
	
  /** from AmAudio */
  void close();
    
 public:
  AmPlaylist(AmEventQueue* q = NULL);
  ~AmPlaylist();

  bool isEmpty();

  void addToPlaylist(AmPlaylistItem* item);
  void addToPlayListFront(AmPlaylistItem* item);

  void flush();
};

/**
 * \brief event fired by the AmPlaylistSeparator
 */
class AmPlaylistSeparatorEvent : 
  public AmEvent {
public:
  AmPlaylistSeparatorEvent(int id)
    : AmEvent(id) { }
};

/**
 * \brief playlistelement to notify session about the playlist progress
 *
 * null-playlist element which notifies session that this position in 
 * playlist is reached. It fies a AmPlaylistSeparatorEvent if it is 
 * read or written.
 */
class AmPlaylistSeparator 
  : public AmAudio {
  bool notified;
  AmEventQueue* ev_q;
  int id;
public:  
  AmPlaylistSeparator(AmEventQueue* q, int id)
    : notified(false), ev_q(q), id(id) { }
  ~AmPlaylistSeparator() { }

  int read(unsigned int user_ts, unsigned int size){
    if (!notified) 
      ev_q->postEvent(new AmPlaylistSeparatorEvent(id)); 
    notified = true; 
    return 0; 
  }
  int write(unsigned int user_ts, unsigned int size){
    return read(user_ts, size);
  }
};




#endif
