/*
 * Copyright (C) 2008 iptego GmbH
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

#include "AmPrecodedFile.h"
#include "AmUtils.h"
#include "log.h"
#include <fstream>

#include <libgen.h>

unsigned int precoded_bytes2samples(long h_codec, unsigned int num_bytes) {

  return ((precoded_payload_t*)h_codec)->frame_ms
    * ((precoded_payload_t*)h_codec)->sample_rate
    / 1000;
}

unsigned int precoded_samples2bytes(long h_codec, unsigned int num_samples) {
  return ((precoded_payload_t*)h_codec)->frame_bytes;
}

amci_codec_t _codec_precoded = { 
  PRECODED_CODEC_ID,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  precoded_bytes2samples,
  precoded_samples2bytes
};

void AmPrecodedFile::initPlugin() {
  AmPlugIn::instance()->addCodec(&_codec_precoded);
  for(std::map<int,precoded_payload_t>::iterator it = 
	payloads.begin(); it != payloads.end(); ++it)
    AmPlugIn::instance()->addPayload(&it->second);
}

AmPrecodedRtpFormat::AmPrecodedRtpFormat(precoded_payload_t& precoded_payload)
  : AmAudioRtpFormat(), precoded_payload(precoded_payload)
{
  channels = precoded_payload.channels;
  rate = precoded_payload.sample_rate;
  // frame_size is in samples, precoded_payload.frame_size in millisec
  //frame_size = precoded_payload.frame_ms * precoded_payload.sample_rate / 1000;
  //frame_length = precoded_payload.frame_ms;
  //frame_encoded_size = precoded_payload.frame_bytes;
  h_codec = (long)&(this->precoded_payload);
}

AmPrecodedRtpFormat::~AmPrecodedRtpFormat() {
}

AmPrecodedFileFormat::AmPrecodedFileFormat(precoded_payload_t& precoded_payload)
  : AmAudioFileFormat(""), precoded_payload(precoded_payload) 
{
  subtype.type = 0;
  subtype.name = precoded_payload.name;
  subtype.sample_rate = precoded_payload.sample_rate;
  subtype.channels = precoded_payload.channels;
  subtype.codec_id = PRECODED_CODEC_ID;

  channels = precoded_payload.channels;
  rate = precoded_payload.sample_rate;
  codec = getCodec();

  // used in precoded_bytes2samples()/precoded_samples2bytes()
  //frame_size = precoded_payload.frame_ms * precoded_payload.sample_rate / 1000;
  //frame_encoded_size = precoded_payload.frame_bytes;
  h_codec = (long)&(this->precoded_payload);
}

AmPrecodedFileFormat::~AmPrecodedFileFormat() { 
}

int precoded_file_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{
  fseek (fp, 0, SEEK_END);
  fmt_desc->data_size=ftell (fp);
  rewind(fp);
  DBG("file opened, size = %d\n", fmt_desc->data_size);
  return 0;
}

int precoded_file_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, 
			long h_codec, struct amci_codec_t *codec)
{
  DBG("file closed\n");
  return 0;
}

AmPrecodedFileInstance::AmPrecodedFileInstance(precoded_payload_t& precoded_payload) 
  : AmAudioFile(), precoded_payload(precoded_payload)
{
  memset(&m_iofmt, 0, sizeof(amci_inoutfmt_t));
  m_iofmt.open = &precoded_file_open;
  m_iofmt.on_close = &precoded_file_close;
  iofmt = &m_iofmt;
}

AmPrecodedFileInstance::~AmPrecodedFileInstance(){
}

AmAudioFileFormat* AmPrecodedFileInstance::fileName2Fmt(const string& name, const string& subtype) {
  return new AmPrecodedFileFormat(precoded_payload);
}

int AmPrecodedFileInstance::open() {
  return AmAudioFile::open(precoded_payload.filename, AmAudioFile::Read);
}

AmPrecodedRtpFormat* AmPrecodedFileInstance::getRtpFormat() {
  return new AmPrecodedRtpFormat(precoded_payload);
}

AmPrecodedFile::AmPrecodedFile() 
{
}

AmPrecodedFile::~AmPrecodedFile() {
}

int AmPrecodedFile::open(const std::string& filename) {
  std::ifstream ifs(filename.c_str());
  if (!ifs.good()) {
    return -1;
  }

  char *dir = strdup(filename.c_str());
  string str_dir(dirname(dir));
  str_dir += "/";

  while (ifs.good() && !ifs.eof()) {
    string codec_line;
    getline(ifs, codec_line);
    if (!codec_line.length() || codec_line[0]=='#')
      continue;

    vector<string> codec_def = explode(codec_line, ";");
    if (codec_def.size() != 8) {
      ERROR("unable to decipher codec line '%s'\n",
	    codec_line.c_str());
      continue;
    }
    
    precoded_payload_t pl;

#define get_uint_item(name, index, description)			\
    unsigned int name;						\
    if (str2i(codec_def[index], name)) {			\
      ERROR("decoding " description " in codec line '%s'\n",	\
	    codec_line.c_str());				\
      continue;							\
    }								\
    pl.name = name;						

    get_uint_item(payload_id, 0, "payload_id");
    pl.c_name = codec_def[1];
    pl.name = pl.c_name.c_str();
    get_uint_item(sample_rate, 2, "sample_rate");
    get_uint_item(channels, 3, "channels");
    pl.format_parameters = codec_def[4];
    get_uint_item(frame_ms, 5, "frame ms");
    get_uint_item(frame_bytes, 6, "frame bytes");
    pl.filename=str_dir + codec_def[7];
#undef get_uint_item

    DBG("inserting codec '%s' file '%s' and id %d\n",
	pl.name, pl.filename.c_str(), pl.payload_id);
    payloads[pl.payload_id]=pl;
  }
  free(dir);
  ifs.close();
  return 0; // OK
}

amci_payload_t* AmPrecodedFile::payload(int payload_id) const {
  std::map<int,precoded_payload_t>::const_iterator it = 
    payloads.find(payload_id);

  if(it != payloads.end())
    return (amci_payload_t*)&it->second;

  return NULL;
}

int AmPrecodedFile::getDynPayload(const string& name, int rate, int encoding_param) const {
  // find a dynamic payload by name/rate and encoding_param (channels, if > 0)
  for(std::map<int, precoded_payload_t>::const_iterator pl_it = payloads.begin();
      pl_it != payloads.end(); ++pl_it)
    if( (name == pl_it->second.name)
	&& (rate == pl_it->second.sample_rate) ) {
      if ((encoding_param > 0) && (pl_it->second.channels > 0) && 
	  (encoding_param != pl_it->second.channels))
	continue;
	  
      return pl_it->first;
    }

  // not found
  return -1;
}


void AmPrecodedFile::getPayloads(vector<SdpPayload>& pl_vec) const
{
  for(std::map<int,precoded_payload_t>::const_iterator pl_it = payloads.begin();
      pl_it != payloads.end(); ++pl_it) {
    pl_vec.push_back(SdpPayload(pl_it->first, pl_it->second.name, pl_it->second.sample_rate, 0));
  }
}

AmPrecodedFileInstance* AmPrecodedFile::getFileInstance(int payload_id) {
  std::map<int,precoded_payload_t>::iterator it=payloads.find(payload_id);
  if (it != payloads.end()) 
    return new AmPrecodedFileInstance(it->second);
  return NULL;
}
