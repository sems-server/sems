/*
 * $Id: AmAudio.cpp 633 2008-01-28 18:17:36Z sayer $
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

#include "AmAudioFile.h"
#include "AmPlugIn.h"
#include "AmUtils.h"
#include "compat/solaris.h"

#include <string.h>

AmAudioFileFormat::AmAudioFileFormat(const string& name, int subtype)
  : name(name), subtype(subtype), p_subtype(0)
{
  getSubtype();
  codec = getCodec();
    
  if(p_subtype && codec){
    rate = p_subtype->sample_rate;
    channels = p_subtype->channels;
    subtype = p_subtype->type;
  } 
}

void AmAudioFileFormat::setSubtypeId(int subtype_id)  { 
  if (subtype != subtype_id) {
    DBG("changing file subtype to ID %d\n", subtype_id);
    destroyCodec();
    subtype = subtype_id; 
    p_subtype = 0;
    codec = getCodec();
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


AmAudioFileFormat* AmAudioFile::fileName2Fmt(const string& name)
{
  string ext = file_extension(name);
  if(ext == ""){
    ERROR("fileName2Fmt: file name has no extension (%s)\n",name.c_str());
    return NULL;
  }

  iofmt = AmPlugIn::instance()->fileFormat("",ext);
  if(!iofmt){
    ERROR("fileName2Fmt: could not find a format with that extension: '%s'\n",ext.c_str());
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

// returns 0 if everything's OK
// return -1 if error
int  AmAudioFile::open(const string& filename, OpenMode mode, bool is_tmp)
{
  close();

  this->close_on_exit = true;

  FILE* n_fp = NULL;

  if(!is_tmp){
    n_fp = fopen(filename.c_str(),mode == AmAudioFile::Read ? "r" : "w+");
    if(!n_fp){
      if(mode == AmAudioFile::Read)
	ERROR("file not found: %s\n",filename.c_str());
      else
	ERROR("could not create/overwrite file: %s\n",filename.c_str());
      return -1;
    }
  } else {	
    n_fp = tmpfile();
    if(!n_fp){
      ERROR("could not create temporary file: %s\n",strerror(errno));
      return -1;
    }
  }

  return fpopen_int(filename, mode, n_fp);
}

int AmAudioFile::fpopen(const string& filename, OpenMode mode, FILE* n_fp)
{
  close();
  return fpopen_int(filename, mode, n_fp);
}

int AmAudioFile::fpopen_int(const string& filename, OpenMode mode, FILE* n_fp)
{

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
  memset(&fd, 0, sizeof(amci_file_desc_t));

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
  fd.channels = f_fmt->channels;
  fd.rate = f_fmt->rate;

  if( iofmt->open && 
      !(ret = (*iofmt->open)(fp, &fd, mode, f_fmt->getHCodecNoInit())) ) {

    if (mode == AmAudioFile::Read) {
      f_fmt->setSubtypeId(fd.subtype);
      f_fmt->channels = fd.channels;
      f_fmt->rate = fd.rate;
      data_size = fd.data_size;

      setBufferSize(fd.buffer_size, fd.buffer_thresh, fd.buffer_full_thresh);
    }
    begin = ftell(fp);
  } else {
    if(!iofmt->open)
      ERROR("no open function\n");
    else
      ERROR("open returned %d: %s\n", ret, strerror(errno));
    close();
    return ret;
  }

  //     if(open_mode == AmAudioFile::Write){

  // 	DBG("After open:\n");
  // 	DBG("fmt::subtype = %i\n",f_fmt->getSubtypeId());
  // 	DBG("fmt::channels = %i\n",f_fmt->channels);
  // 	DBG("fmt::rate = %i\n",f_fmt->rate);
  //     }

  return ret;
}


AmAudioFile::AmAudioFile()
  : AmBufferedAudio(0, 0, 0), data_size(0), 
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
  clearBufferEOF();
}

void AmAudioFile::on_close()
{
  if(fp && !on_close_done){

    AmAudioFileFormat* f_fmt = 
      dynamic_cast<AmAudioFileFormat*>(fmt.get());

    if(f_fmt){
      amci_file_desc_t fmt_desc = { f_fmt->getSubtypeId(), 
				    f_fmt->rate, 
				    f_fmt->channels, 
				    data_size };
	    
      if(!iofmt){
	ERROR("file format pointer not initialized: on_close will not be called\n");
      }
      else if(iofmt->on_close)
	(*iofmt->on_close)(fp,&fmt_desc,open_mode, fmt->getHCodecNoInit(), fmt->getCodec());
    }

    if(open_mode == AmAudioFile::Write){

      DBG("After close:\n");
      DBG("fmt::subtype = %i\n",f_fmt->getSubtypeId());
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

  int ret;
  int s = size;

 read_block:
  long fpos  = ftell(fp);
  if(data_size < 0 || fpos - begin < data_size){
    
    if((data_size > 0) && (fpos - begin + (int)size > data_size)){
      s = data_size - fpos + begin;
    }
    
    s = fread((void*)((unsigned char*)samples),1,s,fp);
    
    ret = (!ferror(fp) ? s : -1);
    
#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
#define bswap_16(A)  ((((u_int16_t)(A) & 0xff00) >> 8) | \
		      (((u_int16_t)(A) & 0x00ff) << 8))
    
    unsigned int i;
    for(i=0;i<=size/2;i++) {
      ((u_int16_t *)((unsigned char*)samples))[i]=bswap_16(((u_int16_t *)((unsigned char*)samples))[i]);
    }
    
#endif
  }
  else {
    if(loop.get() && data_size>0){
      DBG("rewinding audio file...\n");
      rewind();
      goto read_block;
      }
    
    ret = -2; // eof
  }

  if(ret > 0 && s > 0 && (unsigned int)s < size){
    DBG("0-stuffing packet: adding %i bytes (packet size=%i)\n",size-s,size);
    memset((unsigned char*)samples + s,0,size-s);
    return size;
  }

  return ret;
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

int AmAudioFile::getLength() 
{ 
  if (!data_size || !fmt.get())
    return 0;

  return 
    fmt->bytes2samples(data_size) /
    (fmt->rate/1000); 
}
