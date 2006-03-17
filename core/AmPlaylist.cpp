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

void AmPlaylist::gotoNextItem()
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
    if(had_item && !cur_item)
	ev_q->postEvent(new AmAudioEvent(AmAudioEvent::noAudio));
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
	ret = samples2bytes(nb_samples);
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
	  (ret = cur_item->record->put(user_ts,buffer,size)) <= 0){

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

void AmPlaylist::close()
{
    cur_mut.lock();
    while(cur_item)
	gotoNextItem();
    cur_mut.unlock();
}
