#include "AmConferenceChannel.h"
#include <assert.h>

AmConferenceChannel::AmConferenceChannel(AmConferenceStatus* status, int channel_id, bool own_channel)
    : status(status), channel_id(channel_id), own_channel(own_channel)
{
    assert(status);
    conf_id = status->getConfID();
}

AmConferenceChannel::~AmConferenceChannel()
{
    if(own_channel)
	AmConferenceStatus::releaseChannel(conf_id,channel_id);
}

int AmConferenceChannel::put(unsigned int user_ts, unsigned char* buffer, unsigned int size)
{
    status->getMixer()->PutChannelPacket(channel_id,user_ts,buffer,size);
    return size;
}

int AmConferenceChannel::get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples)
{
    unsigned int size = PCM16_S2B(nb_samples);
    status->getMixer()->GetChannelPacket(channel_id,user_ts,buffer,size);
    return size;
}
