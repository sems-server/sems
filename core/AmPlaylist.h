#ifndef AmPlaylist_h
#define AmPlaylist_h

#include "AmAudio.h"
#include "AmThread.h"
#include "AmEventQueue.h"

#include <deque>
using std::deque;

struct AmPlaylistItem
{

    AmAudio* play;
    AmAudio* record;

    AmPlaylistItem(AmAudio* play,
		   AmAudio* record)
	: play(play), record(record) {}
};


class AmPlaylist: public AmAudio
{
    
    AmMutex                items_mut;
    deque<AmPlaylistItem*> items;

    AmMutex                cur_mut;
    AmPlaylistItem*        cur_item;

    AmEventQueue*          ev_q;

    void updateCurrentItem();
    void gotoNextItem();
    
 protected:
    // Fake implement AmAudio's pure virtual methods
    int read(unsigned int user_ts, unsigned int size){ return -1; }
    int write(unsigned int user_ts, unsigned int size){ return -1; }
    
    // override AmAudio
    int get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples);
    int put(unsigned int user_ts, unsigned char* buffer, unsigned int size);
	
 public:
    AmPlaylist(AmEventQueue* q);
    ~AmPlaylist();
    
    void addToPlaylist(AmPlaylistItem* item);
    void addToPlayListFront(AmPlaylistItem* item);
    void close();
};



#endif
