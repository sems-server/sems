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

int AmConferenceChannel::put(unsigned long long system_ts, unsigned char* buffer,
			     int input_sample_rate, unsigned int size)
{
  memcpy((unsigned char*)samples,buffer,size);
  AmMultiPartyMixer* mixer = status->getMixer();
  mixer->lock();
  size = resampleInput(samples,size,input_sample_rate,
		       mixer->GetCurrentSampleRate());
  mixer->PutChannelPacket(channel_id,system_ts,
			  (unsigned char*)samples,size);
  mixer->unlock();
  return size;
}

int AmConferenceChannel::get(unsigned long long system_ts, unsigned char* buffer,
			     int output_sample_rate, unsigned int nb_samples)
{
  if (!nb_samples || !output_sample_rate)
    return 0;

  AmMultiPartyMixer* mixer = status->getMixer();
  mixer->lock();
  unsigned int size = output_sample_rate ?
    PCM16_S2B(nb_samples * mixer->GetCurrentSampleRate() / output_sample_rate) : 0;
  unsigned int mixer_sample_rate = 0;
  mixer->GetChannelPacket(channel_id,system_ts,buffer,size,mixer_sample_rate);
  size = resampleOutput(buffer,size,mixer_sample_rate,output_sample_rate);
  mixer->unlock();
  return size;
}
