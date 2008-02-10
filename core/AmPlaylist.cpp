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

#include "AmPlaylist.h"
#include "amci/codecs.h"
#include "log.h"

void AmPlaylist::updateCurrentItem()
{
  if(!cur_item){
    items_mut.lock();
    if(!items.empty()){
      cur_item = items.front();
      items.pop_front();
    }
    items_mut.unlock();
  }
}

void AmPlaylist::gotoNextItem(bool notify)
{
  bool had_item = false;
  if(cur_item){

    // 	if(cur_item->play)
    // 	    cur_item->play->close();

    // 	if(cur_item->record)
    // 	    cur_item->record->close();

    delete cur_item;
    cur_item = 0;
    had_item = true;
  }

  updateCurrentItem();
  if(notify && had_item && !cur_item){
    DBG("posting AmAudioEvent::noAudio event!\n");
    ev_q->postEvent(new AmAudioEvent(AmAudioEvent::noAudio));
  }
}

int AmPlaylist::get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples)
{
  int ret = -1;

  cur_mut.lock();
  updateCurrentItem();

  while(cur_item && 
	cur_item->play && 
	(ret = cur_item->play->get(user_ts,buffer,nb_samples)) <= 0){

    DBG("get: gotoNextItem\n");
    gotoNextItem();
  }

  if(!cur_item || !cur_item->play) {
    ret = calcBytesToRead(nb_samples);
    memset(buffer,0,ret);
  }

  cur_mut.unlock();
  return ret;
}

int AmPlaylist::put(unsigned int user_ts, unsigned char* buffer, unsigned int size)
{
  int ret = -1;

  cur_mut.lock();
  updateCurrentItem();
  while(cur_item && 
	cur_item->record &&
	(ret = cur_item->record->put(user_ts,buffer,size)) < 0){

    DBG("put: gotoNextItem\n");
    gotoNextItem();
  }

  if(!cur_item || !cur_item->record)
    ret = size;
    
  cur_mut.unlock();
  return ret;
}

AmPlaylist::AmPlaylist(AmEventQueue* q)
  : AmAudio(new AmAudioSimpleFormat(CODEC_PCM16)),
    ev_q(q), cur_item(0)
{
  
}

AmPlaylist::~AmPlaylist()
{
}

void AmPlaylist::addToPlaylist(AmPlaylistItem* item)
{
  items_mut.lock();
  items.push_back(item);
  items_mut.unlock();
}

void AmPlaylist::addToPlayListFront(AmPlaylistItem* item)
{
  cur_mut.lock();
  items_mut.lock();
  if(cur_item){
    items.push_front(cur_item);
    cur_item = item;
  }
  else {
    items.push_front(item);
  }    
  items_mut.unlock();
  cur_mut.unlock();
}

void AmPlaylist::close(bool notify)
{
  cur_mut.lock();
  while(cur_item)
    gotoNextItem(notify);
  cur_mut.unlock();
}

bool AmPlaylist::isEmpty()
{
  bool res(true);

  cur_mut.lock();
  items_mut.lock();

  res = (!cur_item) && items.empty();
    
  items_mut.unlock();
  cur_mut.unlock();

  return res;
}
