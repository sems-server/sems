#include "AmConferenceChannel.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include <assert.h>
#include <fstream>
#include <string.h>

AmConferenceChannel::AmConferenceChannel(AmConferenceStatus* status, int channel_id, string channel_tag, bool own_channel)
  : own_channel(own_channel), channel_id(channel_id), channel_tag(channel_tag), status(status),
    have_in_sr(false), have_out_sr(false), in_file(NULL), out_file(NULL)
{
  assert(status);
  conf_id = status->getConfID();

  if (AmConfig::DumpConferenceStreams) {
    in_file_name = AmConfig::DumpConferencePath + "/"+conf_id+"_"+int2str(channel_id)+"_"+long2str((long)this)+"_in.s16";
    out_file_name = AmConfig::DumpConferencePath + "/"+conf_id+"_"+int2str(channel_id)+"_"+long2str((long)this)+"_out.s16";

    DBG("conf channel opening in_file '%s'\n", in_file_name.c_str());
    in_file = new ChannelWritingFile(in_file_name.c_str());
    DBG("conf channel opening out_file '%s'\n", out_file_name.c_str());
    out_file = new ChannelWritingFile(out_file_name.c_str());
  }
}

AmConferenceChannel::~AmConferenceChannel()
{
  if(own_channel)
    AmConferenceStatus::releaseChannel(conf_id,channel_id);

  if (in_file)
    in_file->close();
  if (out_file)
    out_file->close();

}

int AmConferenceChannel::put(unsigned long long system_ts, unsigned char* buffer,
			     int input_sample_rate, unsigned int size)
{
  memcpy((unsigned char*)samples,buffer,size);
  AmMultiPartyMixer* mixer = status->getMixer();

  if (AmConfig::DumpConferenceStreams) {
    if (!have_in_sr) {
      DBG("writing sample rate of %u to %s\n", input_sample_rate, (in_file_name+".samplerate").c_str());
      std::ofstream ofs((in_file_name+".samplerate").c_str());
      if (ofs.good()) {
	ofs << int2str(input_sample_rate);
	ofs.close();
      }
      have_in_sr = true;
    }
    if (in_file) {
      in_file->write(buffer, size);
    }
  }

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


  if (AmConfig::DumpConferenceStreams) {
    if (!have_out_sr) {
      DBG("writing mixer sample rate of %u to %s\n", mixer_sample_rate, (out_file_name+".samplerate").c_str());
      std::ofstream ofs((out_file_name+".samplerate").c_str());
      if (ofs.good()) {
	ofs << int2str(mixer_sample_rate);
	ofs.close();
      }
      have_out_sr = true;
    }
    if (out_file) {
      out_file->write(buffer, size);
    }
  }

  size = resampleOutput(buffer,size,mixer_sample_rate,output_sample_rate);
  mixer->unlock();
  return size;
}

ChannelWritingFile::ChannelWritingFile(const char* path) 
  : async_file(256*1024) // 256k buffer
{
  fp = fopen(path, "w");
  if (!fp) {
    ERROR("opening file '%s' for writing: '%s'\n", path, strerror(errno));
  }
}

ChannelWritingFile::~ChannelWritingFile() {
  if (fp)
    fclose(fp);
}

int ChannelWritingFile::write_to_file(const void* buf, unsigned int len) {
  if (!fp)
    return len;
  size_t res = fwrite(buf, 1, len, fp);
    return !ferror(fp) ? res : -1;
}

void ChannelWritingFile::on_flushed() {
  DBG("file on_flushed, deleting self\n");
  delete this; // uh!
}
