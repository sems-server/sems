#ifndef AmConferenceChannel_h
#define AmConferenceChannel_h

#include "AmAudio.h"
#include "AmConferenceStatus.h"

class AmConferenceChannel: public AmAudio
{
    bool                own_channel;
    int                 channel_id;
    string              conf_id;
    AmConferenceStatus* status;

protected:
    // Fake implement AmAudio's pure virtual methods
    // this avoids to copy the samples locally by implementing only get/put
    int read(unsigned int user_ts, unsigned int size){ return -1; }
    int write(unsigned int user_ts, unsigned int size){ return -1; }

    // override AmAudio
    int get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples);
    int put(unsigned int user_ts, unsigned char* buffer, unsigned int size);

public:
    AmConferenceChannel(AmConferenceStatus* status, 
			int channel_id, bool own_channel);

    ~AmConferenceChannel();

};



#endif
