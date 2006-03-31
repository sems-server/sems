/*
 * $Id: AmAudio.cpp,v 1.23.2.8 2005/08/31 13:54:29 rco Exp $
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

#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmUtils.h"
#include "amci/codecs.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <typeinfo>

AmAudioRtpFormat::AmAudioRtpFormat(int payload, string format_parameters)
  : AmAudioFormat(), payload(payload), amci_pl(0)
{
  sdp_format_parameters = format_parameters;
  codec = getCodec();
 
  amci_payload_t* pl = getPayloadP();
  if(pl && codec){
    sample = codec->sample_size;
    channels = pl->channels;
    rate = pl->sample_rate;
  } else {
    ERROR("Could not find payload <%i>\n",payload);
  } 
}

AmAudioFormat::AmAudioFormat()
  : sample(-1), channels(-1), rate(-1), codec(0),
    frame_length(20), frame_size(160), frame_encoded_size(320)
{

}

AmAudioSimpleFormat::AmAudioSimpleFormat(int codec_id)
    : AmAudioFormat(), codec_id(codec_id)
{
    codec = getCodec();
    sample = codec->sample_size;
    rate = 8000;
    channels = 1;
}

AmAudioFileFormat::AmAudioFileFormat(const string& name, int subtype)
    : name(name), subtype(subtype), p_subtype(0)
{
    getSubtype();
    codec = getCodec();
    
    if(p_subtype && codec){
	sample = codec->sample_size;
	rate = p_subtype->sample_rate;
	channels = p_subtype->channels;
	subtype = p_subtype->type;
    } 
}

AmAudioFormat::~AmAudioFormat()
{
    destroyCodec();
}

bool AmAudioFormat::operator == (const AmAudioFormat& r) const
{
    return ( codec && r.codec
	     && (r.codec->id == codec->id) 
	     && (r.sample == sample)
	     && (r.channels == channels)
	     && (r.rate == rate));
}

bool AmAudioFormat::operator != (const AmAudioFormat& r) const
{
    return !(this->operator == (r));
}

void AmAudioFormat::initCodec()
{
  amci_codec_fmt_info_t fmt_i[4];

  fmt_i[0].id=0;

  if( codec && codec->init ) {
    if ((h_codec = (*codec->init)(sdp_format_parameters.c_str(), fmt_i)) == -1) {
      ERROR("could not initialize codec %i\n",codec->id);
    } else {
      string s; 
      int i=0;
	while (fmt_i[i].id) {
	  switch (fmt_i[i].id) {
	  case AMCI_FMT_FRAME_LENGTH : {
	    frame_length=fmt_i[i].value; 
	  } break;
	  case AMCI_FMT_FRAME_SIZE: {
	    frame_size=fmt_i[i].value; 
	  } break;
	  case AMCI_FMT_ENCODED_FRAME_SIZE: {
	    frame_encoded_size=fmt_i[i].value; 
	  } break;
	  default: {
	    DBG("Unknown codec format descriptor: %d\n", fmt_i[i].id);
	  } break;
	  }
	  i++;
      }
    }  
  } 
}

void AmAudioFormat::destroyCodec()
{
    if( codec && codec->destroy ){
	(*codec->destroy)(h_codec);
	h_codec = 0;
    }
}

amci_subtype_t*  AmAudioFileFormat::getSubtype()
{
    if(!p_subtype && !name.empty()){

	amci_inoutfmt_t* iofmt = AmPlugIn::instance()->fileFormat(name.c_str());
	if(!iofmt){
	    ERROR("AmAudioFileFormat::getSubtype: file format '%s' does not exist\n",
		  name.c_str());
	    throw string("AmAudioFileFormat::getSubtype: file format '%s' does not exist\n");
	}
	else {
	    p_subtype = AmPlugIn::instance()->subtype(iofmt,subtype);
	    if(!p_subtype)
		ERROR("AmAudioFileFormat::getSubtype: subtype %i in format '%s' does not exist\n",
		      subtype,iofmt->name);
	    subtype = p_subtype->type;
	}
    }
    return p_subtype;
}


amci_codec_t* AmAudioFormat::getCodec()
{

    if(!codec){
	int codec_id = getCodecId();
	codec = AmPlugIn::instance()->codec(codec_id);

	initCodec();
    }
    
    return codec;
}

long AmAudioFormat::getHCodec()
{
    if(!codec)
	getCodec();
    return h_codec;
}

AmAudio::AmAudio()
    : fmt(new AmAudioSimpleFormat(CODEC_PCM16)),
      max_rec_time(-1),
      rec_time(0)
{
}

AmAudio::AmAudio(AmAudioFormat *_fmt)
    : fmt(_fmt),
      max_rec_time(-1),
      rec_time(0)
{
}

AmAudio::~AmAudio()
{
}

void AmAudio::close()
{
}

// returns bytes read, else -1 if error (0 is OK)
int AmAudio::get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples)
{
    int size = nb_samples * fmt->sample * fmt->channels;

    size = read(user_ts,size);
    //DBG("size = %d\n",size);
    if(size <= 0){
	return size;
    }

    size = decode(size);
    if(size < 0) {
	DBG("decode returned %i\n",size);
	return -1; 
    }
    size = downMix(size);
    
    if(size>0)
	memcpy(buffer,(unsigned char*)samples,size);

    return size;
}

// returns bytes written, else -1 if error (0 is OK)
int AmAudio::put(unsigned int user_ts, unsigned char* buffer, unsigned int size)
{
    if(!size){
	return 0;
    }

    if(max_rec_time > -1 && rec_time >= max_rec_time)
	return -1;


    memcpy((unsigned char*)samples,buffer,size);

    unsigned int s = encode(size);
    if(s>0){
	//DBG("%s\n",typeid(this).name());
	incRecordTime(bytes2samples(size));
	return write(user_ts,(unsigned int)s);
    }
    else{
	return s;
    }
}

void AmAudio::stereo2mono(unsigned char* out_buf,unsigned char* in_buf,unsigned int& size)
{
    short* in  = (short*)in_buf;
    short* end = (short*)(in_buf + size);
    short* out = (short*)out_buf;
    
    while(in != end){
	*(out++) = (*in + *(in+1)) / 2;
	in += 2;
    }

    size /= 2;
}

int AmAudio::decode(unsigned int size)
{
    int s = size;

    if(!fmt.get()){
 	DBG("no fmt !\n");
 	return s;
    }

    amci_codec_t* codec = fmt->getCodec();
    long h_codec = fmt->getHCodec();

//     if(!codec){
// 	ERROR("audio format set, but no codec has been loaded\n");
// 	abort();
// 	return -1;
//     }

    if(codec->decode){
	s = (*codec->decode)(samples.back_buffer(),samples,s,
			     fmt->channels,fmt->rate,h_codec);
	if(s<0) return s;
	samples.swap();
    }
    
    return s;
}

int AmAudio::encode(unsigned int size)
{
    int s = size;

//     if(!fmt.get()){
// 	DBG("no encode fmt\n");
// 	return 0;
//     }

    amci_codec_t* codec = fmt->getCodec();
    long h_codec = fmt->getHCodec();

    if(codec->encode){
	s = (*codec->encode)(samples.back_buffer(),samples,(unsigned int) size,
				      fmt->channels,fmt->rate,h_codec);
	if(s<0) return s;
	samples.swap();
    }
    
    return s;
}

unsigned int AmAudio::downMix(unsigned int size)
{
    unsigned int s = size;
    if(fmt->channels == 2){
	stereo2mono(samples.back_buffer(),(unsigned char*)samples,s);
	samples.swap();
    }

    return s;
}

unsigned int AmAudio::getFrameSize()
{
    assert(fmt.get());
    return fmt->frame_size;
}

unsigned int AmAudio::samples2bytes(unsigned int nb_samples)
{
    return nb_samples * fmt->sample * fmt->channels;
}

unsigned int AmAudio::bytes2samples(unsigned int bytes)
{
    return bytes / (fmt->sample * fmt->channels);
}

void AmAudio::setRecordTime(unsigned int ms)
{
    max_rec_time = ms * (fmt->rate / 1000);
}

int AmAudio::incRecordTime(unsigned int samples)
{
    return rec_time += samples;
}


DblBuffer::DblBuffer()
    : active_buf(0)
{ 
}

DblBuffer::operator unsigned char*()
{
    return samples + (active_buf ? AUDIO_BUFFER_SIZE : 0);
}

unsigned char* DblBuffer::back_buffer()
{
    return samples + (active_buf ? 0 : AUDIO_BUFFER_SIZE);
}

void DblBuffer::swap()
{
    active_buf = !active_buf;
}

// returns 0 if everything's OK
// return -1 if error
int  AmAudioFile::open(const string& filename, OpenMode mode, bool is_tmp)
{
    close();

    AmAudioFileFormat* f_fmt = fileName2Fmt(filename);
    if(!f_fmt){
	ERROR("while trying to the format of '%s'\n",filename.c_str());
	return -1;
    }
    fmt.reset(f_fmt);

    open_mode = mode;
    this->close_on_exit = close_on_exit;

    if(!is_tmp){
	fp = fopen(filename.c_str(),mode == AmAudioFile::Read ? "r" : "w+");
	if(!fp){
	    if(mode == AmAudioFile::Read)
		ERROR("file not found: %s\n",filename.c_str());
	    else
		ERROR("could not create/overwrite file: %s\n",filename.c_str());
	    return -1;
	}
    } else {
	
	fp = tmpfile();
	if(!fp){
	    ERROR("could not create temporary file: %s\n",strerror(errno));
	}
    }

    amci_file_desc_t fd;
    int ret = -1;

    if(open_mode == AmAudioFile::Write){

 	if (f_fmt->channels<0 || f_fmt->rate<0) {
	    if (f_fmt->channels<0)
		ERROR("channel count must be set for output file.\n");
	    if (f_fmt->rate<0)
		ERROR("sampling rate must be set for output file.\n");
	    close();
	    return -1;
 	}
    }

    fd.subtype = f_fmt->getSubtypeId();
    fd.sample = f_fmt->sample;
    fd.channels = f_fmt->channels;
    fd.rate = f_fmt->rate;

    if( iofmt->open && !(ret = (*iofmt->open)(fp,&fd,mode, f_fmt->getHCodecNoInit())) ) {
	if (mode == AmAudioFile::Read) {
	    f_fmt->setSubtypeId(fd.subtype);
	    f_fmt->sample = fd.sample;
	    f_fmt->channels = fd.channels;
	    f_fmt->rate = fd.rate;
	}
	begin = ftell(fp);
    }
    else {
	if(!iofmt->open)
	    ERROR("no open function\n");
	else
	    ERROR("open returned %d\n",ret);
	close();
	return ret;
    }

//     if(open_mode == AmAudioFile::Write){

// 	DBG("After open:\n");
// 	DBG("fmt::subtype = %i\n",f_fmt->getSubtypeId());
// 	DBG("fmt::sample = %i\n",f_fmt->sample);
// 	DBG("fmt::channels = %i\n",f_fmt->channels);
// 	DBG("fmt::rate = %i\n",f_fmt->rate);
//     }

    return ret;
}

int AmAudioFile::fpopen(const string& filename, OpenMode mode, FILE* n_fp)
{
    close();

    AmAudioFileFormat* f_fmt = fileName2Fmt(filename);
    if(!f_fmt){
	ERROR("while trying to the format of '%s'\n",filename.c_str());
	return -1;
    }
    fmt.reset(f_fmt);

    open_mode = mode;
    fp = n_fp;
    fseek(fp,0L,SEEK_SET);

    amci_file_desc_t fd;
    int ret = -1;

    if(open_mode == AmAudioFile::Write){

 	if (f_fmt->channels<0 || f_fmt->rate<0) {
	    if (f_fmt->channels<0)
		ERROR("channel count must be set for output file.\n");
	    if (f_fmt->rate<0)
		ERROR("sampling rate must be set for output file.\n");
	    close();
	    return -1;
 	}
    }

    fd.subtype = f_fmt->getSubtypeId();
    fd.sample = f_fmt->sample;
    fd.channels = f_fmt->channels;
    fd.rate = f_fmt->rate;

    if( iofmt->open && !(ret = (*iofmt->open)(fp,&fd,mode, f_fmt->getHCodecNoInit())) ) {
	if (mode == AmAudioFile::Read) {
	    f_fmt->setSubtypeId(fd.subtype);
	    f_fmt->sample = fd.sample;
	    f_fmt->channels = fd.channels;
	    f_fmt->rate = fd.rate;
	}
	begin = ftell(fp);
    }
    else {
	if(!iofmt->open)
	    ERROR("no open function\n");
	else
	    ERROR("open returned %d\n",ret);
	close();
	return ret;
    }

//     if(open_mode == AmAudioFile::Write){

// 	DBG("After open:\n");
// 	DBG("fmt::subtype = %i\n",f_fmt->getSubtypeId());
// 	DBG("fmt::sample = %i\n",f_fmt->sample);
// 	DBG("fmt::channels = %i\n",f_fmt->channels);
// 	DBG("fmt::rate = %i\n",f_fmt->rate);
//     }

    return ret;
}


AmAudioFile::AmAudioFile()
    : AmAudio(), data_size(0), 
      fp(0), begin(0), loop(false),
      on_close_done(false),
      close_on_exit(true)
{
}

AmAudioFile::~AmAudioFile()
{
    close();
}

void AmAudioFile::rewind()
{
    fseek(fp,begin,SEEK_SET);
}

void AmAudioFile::on_close()
{
    if(fp && !on_close_done){

	AmAudioFileFormat* f_fmt = 
	    dynamic_cast<AmAudioFileFormat*>(fmt.get());

	if(f_fmt){
	    amci_file_desc_t fmt_desc = { f_fmt->getSubtypeId(), 
					  f_fmt->sample, 
					  f_fmt->rate, 
					  f_fmt->channels, 
					  data_size };
	    
	    if(!iofmt){
		ERROR("file format pointer not initialized: on_close will not be called\n");
	    }
	    else if(iofmt->on_close)
		(*iofmt->on_close)(fp,&fmt_desc,open_mode, fmt->getHCodecNoInit());
	}

	if(open_mode == AmAudioFile::Write){

	    DBG("After close:\n");
	    DBG("fmt::subtype = %i\n",f_fmt->getSubtypeId());
	    DBG("fmt::sample = %i\n",f_fmt->sample);
	    DBG("fmt::channels = %i\n",f_fmt->channels);
	    DBG("fmt::rate = %i\n",f_fmt->rate);
	}

	on_close_done = true;
    }
}


void AmAudioFile::close()
{
    if(fp){
	on_close();

	if(close_on_exit)
	    fclose(fp);
	fp = 0;
    }
}

string AmAudioFile::getMimeType()
{
    if(!iofmt)
	return "";
    
    return iofmt->email_content_type;
}


int AmAudioFile::read(unsigned int user_ts, unsigned int size)
{
    if(!fp){
	ERROR("AmAudioFile::read: file is not opened\n");
	return -1;
    }

    int s = fread((void*)((unsigned char*)samples),1,size,fp);
    int ret = (!ferror(fp) ? s : -1);

    //DBG("s = %i; ret = %i\n",s,ret);
    if(loop.get() && (ret <= 0) && feof(fp)){

	DBG("rewinding audio file...\n");
	rewind();
	s = fread((void*)((unsigned char*)samples),1,size,fp);
	ret = (!ferror(fp) ? s : -1);
    }

    if(ret > 0 && s > 0 && (unsigned int)s < size){
	DBG("0-stuffing packet: adding %i bytes (packet size=%i)\n",size-s,size);
	memset((unsigned char*)samples + s,0,size-s);
	return size;
    }

    return (feof(fp) && !loop.get() ? -2 : ret);
}

int AmAudioFile::write(unsigned int user_ts, unsigned int size)
{
    if(!fp){
	ERROR("AmAudioFile::write: file is not opened\n");
	return -1;
    }

    int s = fwrite((void*)((unsigned char*)samples),1,size,fp);
    if(s>0)
	data_size += s;
    return (!ferror(fp) ? s : -1);
}

AmAudioFileFormat* AmAudioFile::fileName2Fmt(const string& name)
{
    string ext = file_extension(name);
    if(ext == ""){
	ERROR("fileName2Fmt: file name has no extension (%s)",name.c_str());
	return NULL;
    }

    iofmt = AmPlugIn::instance()->fileFormat("",ext);
    if(!iofmt){
	ERROR("fileName2Fmt: could not find a format with that extension: '%s'",ext.c_str());
	return NULL;
    }

    return new AmAudioFileFormat(iofmt->name);
}


int AmAudioFileFormat::getCodecId()
{
  if(!name.empty()){
      getSubtype();
	if(p_subtype)
	  return p_subtype->codec_id;
  }
    
  return -1;
}


amci_payload_t* AmAudioRtpFormat::getPayloadP()
{
    if(!amci_pl)
	amci_pl = AmPlugIn::instance()->payload(payload);

    return amci_pl;
}

int AmAudioRtpFormat::getCodecId()
{
    amci_payload_t* pl = getPayloadP();
    if(!pl){
	ERROR("AmAudioRtpFormat::getCodecId: could not find payload %i\n",payload);
	return -1;
    }
    else 
	return pl->codec_id;
}

