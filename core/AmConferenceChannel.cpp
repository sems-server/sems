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

int AmConferenceChannel::put(unsigned int user_ts, unsigned char* buffer, int input_sample_rate, unsigned int size)
{
  memcpy((unsigned char*)samples,buffer,size);
  status->getMixer()->lock();
  size = resampleInput(samples,size,input_sample_rate,status->getMixer()->GetCurrentSampleRate());
  status->getMixer()->PutChannelPacket(channel_id,user_ts,(unsigned char*)samples,size);
  status->getMixer()->unlock();
  return size;
}

int AmConferenceChannel::get(unsigned int user_ts, unsigned char* buffer, int output_sample_rate, unsigned int nb_samples)
{
  status->getMixer()->lock();
  unsigned int size = PCM16_S2B(nb_samples * status->getMixer()->GetCurrentSampleRate() / output_sample_rate);
  unsigned int mixer_sample_rate = 0;
  status->getMixer()->GetChannelPacket(channel_id,user_ts,buffer,size,mixer_sample_rate);
  size = resampleOutput(buffer,size,mixer_sample_rate,output_sample_rate);
  status->getMixer()->unlock();
  return size;
}
