/*
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "AmCachedAudioFile.h"
#include "AmUtils.h"
#include "log.h"
#include "AmPlugIn.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>



using std::string;

AmFileCache::AmFileCache() 
  : data(NULL), 
    data_size(0)
{ }

AmFileCache::~AmFileCache() {
  if ((data != NULL) && 
      munmap(data, data_size)) {
    ERROR("while unmapping file.\n");
  }
}

int AmFileCache::load(const std::string& filename) {
  int fd; 
  struct stat sbuf;

  name = filename;

  if ((fd = open(name.c_str(), O_RDONLY)) == -1) {
    ERROR("while opening file '%s' for caching.\n", 
	  filename.c_str());
    return -1;
  }

  if (fstat(fd,  &sbuf) == -1) {
    ERROR("cannot stat file '%s'.\n", 
	  name.c_str());
    close(fd);
    return -2;
  }
	
  if ((data = mmap((caddr_t)0, sbuf.st_size, PROT_READ, MAP_PRIVATE, 
		   fd, 0)) == (caddr_t)(-1)) {
    ERROR("cannot mmap file '%s'.\n", 
	  name.c_str());
    close(fd);
    return -3;
  }

  data_size = sbuf.st_size;
  close(fd);

  return 0;
}

int AmFileCache::read(void* buf, 
		      size_t* pos, 
		      size_t size) {

  if (*pos >= data_size)
    return -1; // eof

  size_t r_size = size;
  if (*pos+size > data_size)
    r_size = data_size-*pos;

  if (r_size>0) {
    memcpy(buf, (unsigned char*)data + *pos, r_size);
    *pos+=r_size;
  }
  return r_size;
}

inline size_t AmFileCache::getSize() {
  return data_size;
}

inline const string& AmFileCache::getFilename() {
  return name;
}


AmCachedAudioFile::AmCachedAudioFile(AmFileCache* cache) 
  : cache(cache), fpos(0), begin(0), good(false), loop(false)
{
  if (!cache) {
    ERROR("Need open file cache.\n");
    return;
  }

  AmAudioFileFormat* f_fmt = fileName2Fmt(cache->getFilename());
  if(!f_fmt){
    ERROR("while trying to determine the format of '%s'\n",
	  cache->getFilename().c_str());
    return;
  }
  fmt.reset(f_fmt);
	
  amci_file_desc_t fd;
  int ret = -1;

  fd.subtype = f_fmt->getSubtypeId();
  fd.channels = f_fmt->channels;
  fd.rate = f_fmt->getRate();

  long unsigned int ofpos = fpos;

  if( iofmt->mem_open && 
      !(ret = (*iofmt->mem_open)((unsigned char*)cache->getData(),cache->getSize(),&ofpos,
				 &fd,AmAudioFile::Read,f_fmt->getHCodecNoInit())) ) {
    f_fmt->setSubtypeId(fd.subtype);
    f_fmt->channels = fd.channels;
    f_fmt->setRate(fd.rate);

    begin = fpos = ofpos;
  }
  else {
    if(!iofmt->mem_open)
      ERROR("no mem_open function\n");
    else
      ERROR("mem_open returned %d\n",ret);
    close();
    return;
  }

  good = true;

  return;
}

AmCachedAudioFile::~AmCachedAudioFile() {
}

AmAudioFileFormat* AmCachedAudioFile::fileName2Fmt(const string& name)
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

void AmCachedAudioFile::rewind() {
  fpos = begin;
}

/** Closes the file. */
void AmCachedAudioFile::close() {
  fpos = 0;
}

/** Executes the handler's on_close. */
void on_close() {
}

int AmCachedAudioFile::read(unsigned int user_ts, unsigned int size) {

  if(!good){
    ERROR("AmAudioFile::read: file is not opened\n");
    return -1;
  }

  int ret = cache->read((void*)((unsigned char*)samples),&fpos,size);
	
  //DBG("s = %i; ret = %i\n",s,ret);
  if(loop.get() && (ret <= 0) && fpos==cache->getSize()){
    DBG("rewinding audio file...\n");
    rewind();
    ret = cache->read((void*)((unsigned char*)samples),&fpos, size);
  }

  if(ret > 0 && (unsigned int)ret < size){
    DBG("0-stuffing packet: adding %i bytes (packet size=%i)\n",size-ret,size);
    memset((unsigned char*)samples + ret,0,size-ret);
    return size;
  }

  return (fpos==cache->getSize() && !loop.get() ? -2 : ret);
}

int AmCachedAudioFile::write(unsigned int user_ts, unsigned int size) {
  ERROR("AmCachedAudioFile writing not supported!\n");
  return -1;
}

