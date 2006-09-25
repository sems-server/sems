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
