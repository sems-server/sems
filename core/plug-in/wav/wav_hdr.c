/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "wav_hdr.h"
#include "../../log.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || (defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN))
#define bswap_16(A)  ((((u_int16_t)(A) & 0xff00) >> 8) | \
                   (((u_int16_t)(A) & 0x00ff) << 8))
#define bswap_32(A)  ((((u_int32_t)(A) & 0xff000000) >> 24) | \
                   (((u_int32_t)(A) & 0x00ff0000) >> 8)  | \
                   (((u_int32_t)(A) & 0x0000ff00) << 8)  | \
                   (((u_int32_t)(A) & 0x000000ff) << 24))
#define cpu_to_le16(x) bswap_16(x)
#define le_to_cpu16(x) bswap_16(x)
#define cpu_to_le32(x) bswap_32(x)
#define le_to_cpu32(x) bswap_32(x)
#elif (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || defined(_LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN) || (defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN))
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define le_to_cpu16(x) (x)
#define le_to_cpu32(x) (x)
#else
#error unknown endianness!
#endif


#define SAFE_READ(buf,s,fp,sr) \
    sr = fread(buf,s,1,fp);\
    if((sr != 1) || ferror(fp)) { \
      ERROR("fread: %s (sr=%d)\n", strerror(errno), sr);	\
    return -1;					\
    }
\

/** \brief The file header of RIFF-WAVE files (*.wav). 
 * Files are always in
 * little-endian byte-order.  
 */

struct wav_header
{
  char      magic[4];
  u_int32_t length;
  char	    chunk_type[4];
  char      chunk_format[4];
  u_int32_t chunk_length;
  u_int16_t format;
  u_int16_t channels;
  u_int32_t sample_rate;
  u_int32_t bytes_per_second;
  u_int16_t sample_size;
  u_int16_t precision;
  char      chunk_data[4];
  u_int32_t data_length;
};

//If wav_dummyread() had a problem, return code -1
int wav_dummyread(FILE *fp, unsigned int size)
{
  unsigned int s;
  char *dummybuf;
  
  DBG("Skip chunk by reading dummy bytes from stream\n");
  dummybuf = (char*) malloc (size);
  if(dummybuf==NULL) {
      ERROR("Can't alloc memory for dummyread!\n");
      return -1;
  }

  SAFE_READ(dummybuf,size,fp,s);
  free(dummybuf);
  return 0;
} 

static int wav_read_header(FILE* fp, struct amci_file_desc_t* fmt_desc)
{
  unsigned int s;

  char tag[4]={'\0'};
  unsigned int file_size=0;
  unsigned int chunk_size=0;
  unsigned short fmt=0;
  unsigned short channels=0;
  unsigned int rate=0;
  unsigned short bits_per_sample=0;
  unsigned short sample_size=0;
  char dummy[6]={'\0'};
  int is_seekable = 1;

  if(!fp)
    return -1;
  rewind(fp);

  DBG("trying to read WAV file\n");

  SAFE_READ(tag,4,fp,s);
  DBG("tag = <%.4s>\n",tag);
  if(strncmp(tag,"RIFF",4)){
    DBG("wrong format !\n");
    return -1;
  }

  SAFE_READ(&file_size,4,fp,s);
  file_size=le_to_cpu32(file_size);
  DBG("file size = <%u>\n",file_size);

  SAFE_READ(tag,4,fp,s);
  DBG("tag = <%.4s>\n",tag);
  if(strncmp(tag,"WAVE",4)){
    DBG("wrong format !\n");
    return -1;
  }

  SAFE_READ(tag,4,fp,s);
  DBG("tag = <%.4s>\n",tag);
  if(strncmp(tag,"fmt ",4)){
    DBG("wrong format !\n");
    return -1;
  }
    
  SAFE_READ(&chunk_size,4,fp,s);
  chunk_size=le_to_cpu32(chunk_size);
  DBG("chunk_size = <%u>\n",chunk_size);
    
  SAFE_READ(&fmt,2,fp,s);
  fmt=le_to_cpu16(fmt);
  DBG("fmt = <%.2x>\n",fmt);

  SAFE_READ(&channels,2,fp,s);
  channels=le_to_cpu16(channels);
  DBG("channels = <%i>\n",channels);

  SAFE_READ(&rate,4,fp,s);
  rate=le_to_cpu32(rate);
  DBG("rate = <%i>\n",rate);

  /* do not read bytes/sec and block align */
  SAFE_READ(&dummy,6,fp,s); // skip by reading into dummy buffer

  SAFE_READ(&bits_per_sample,2,fp,s);
  bits_per_sample=le_to_cpu16(bits_per_sample);
  DBG("bits/sample = <%i>\n",bits_per_sample);

  fmt_desc->subtype = fmt;
  sample_size = bits_per_sample>>3;
  fmt_desc->rate = rate;
  fmt_desc->channels = channels;

  if( (fmt == 0x01) && (sample_size == 1)){
    ERROR("Sorry, we don't support PCM 8 bit\n");
    return -1;
  }

  if ((fseek(fp,chunk_size-16,SEEK_CUR) < 0) 
      && errno == EBADF) {
    is_seekable = 0;
    wav_dummyread(fp,chunk_size-16);
  }

  for(;;) {

    SAFE_READ(tag,4,fp,s);
    DBG("tag = <%.4s>\n",tag);
	
    SAFE_READ(&chunk_size,4,fp,s);
    chunk_size=le_to_cpu32(chunk_size);
    DBG("chunk size = <%i>\n",chunk_size);
	
    if(!strncmp(tag,"data",4))
      break;

    if (is_seekable)
      fseek(fp,chunk_size,SEEK_CUR);
    else 
      wav_dummyread(fp,chunk_size);
  }
  fmt_desc->data_size = chunk_size;

  return 0;
}

int wav_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{
  if(options == AMCI_RDONLY){
    return wav_read_header(fp,fmt_desc);
  }
  else {
    /*  Reserve some space for the header */
    /*  We need this, as information for headers  */
    /*  like 'size' is not known yet */
    fseek(fp,44L,SEEK_CUR); 
    return (ferror(fp) ? -1 : 0);
  }
}

int wav_write_header(FILE* fp, struct amci_file_desc_t* fmt_desc, long h_codec, struct amci_codec_t *codec)
{
  struct wav_header hdr;
  int sample_size;

  if (codec && codec->samples2bytes)
    sample_size = codec->samples2bytes(h_codec, 1);
  else {
    ERROR("Cannot determine sample size\n");
    sample_size = 2;
  }
  memcpy(hdr.magic, "RIFF",4);
  hdr.length = cpu_to_le32(fmt_desc->data_size + 36);
  memcpy(hdr.chunk_type, "WAVE",4);
  memcpy(hdr.chunk_format, "fmt ",4);
  hdr.chunk_length = cpu_to_le32(16);
  hdr.format = cpu_to_le16(fmt_desc->subtype);
  hdr.channels = cpu_to_le16((unsigned short)fmt_desc->channels);
  hdr.sample_rate = cpu_to_le32((unsigned int)fmt_desc->rate);
  hdr.sample_size = cpu_to_le16(hdr.channels * sample_size);
  hdr.bytes_per_second = cpu_to_le32(hdr.sample_rate * (unsigned int)hdr.sample_size);
  hdr.precision = cpu_to_le16((unsigned short)(sample_size * 8));
  memcpy(hdr.chunk_data,"data",4);
  hdr.data_length=cpu_to_le32(fmt_desc->data_size);

  fwrite(&hdr,sizeof(hdr),1,fp);
  if(ferror(fp)) return -1;

  DBG("fmt = <%i>\n",le_to_cpu16(hdr.format));
  DBG("channels = <%i>\n",le_to_cpu16(hdr.channels));
  DBG("rate = <%i>\n",le_to_cpu32(hdr.sample_rate));
  DBG("data_size = <%i>\n",le_to_cpu32(hdr.data_length));

  return 0;
}


int wav_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec, struct amci_codec_t *codec)
{
  if(options == AMCI_WRONLY){
    rewind(fp);
    return wav_write_header(fp, fmt_desc, h_codec, codec);
  }
  return 0;
}

#define SAFE_MEM_READ(buf,s,mptr,pos,size) \
    if (*pos+s>size) return -1; \
    memcpy(buf,mptr+*pos,s); \
    *pos+=s; 

int wav_mem_open(unsigned char* mptr, unsigned long size, unsigned long* pos, 
		 struct amci_file_desc_t* fmt_desc, int options, long h_codec) {
  if(options == AMCI_RDONLY){
		
    char tag[4]={'\0'};
    unsigned int file_size=0;
    unsigned int chunk_size=0;
    unsigned short fmt=0;
    unsigned short channels=0;
    unsigned int rate=0;
    unsigned short bits_per_sample=0;
    unsigned short sample_size=0;

    if(!mptr)
      return -1;
    *pos=0;

    DBG("trying to read WAV file from memory\n");

    SAFE_MEM_READ(tag,4,mptr,pos,size);
    DBG("tag = <%.4s>\n",tag);
    if(strncmp(tag,"RIFF",4)){
      DBG("wrong format !");
      return -1;
    }

    SAFE_MEM_READ(&file_size,4,mptr,pos,size);
    DBG("file size = <%u>\n",file_size);

    SAFE_MEM_READ(tag,4,mptr,pos,size);
    DBG("tag = <%.4s>\n",tag);
    if(strncmp(tag,"WAVE",4)){
      DBG("wrong format !");
      return -1;
    }

    SAFE_MEM_READ(tag,4,mptr,pos,size);
    DBG("tag = <%.4s>\n",tag);
    if(strncmp(tag,"fmt ",4)){
      DBG("wrong format !");
      return -1;
    }
    
    SAFE_MEM_READ(&chunk_size,4,mptr,pos,size);
    DBG("chunk_size = <%u>\n",chunk_size);
    
    SAFE_MEM_READ(&fmt,2,mptr,pos,size);
    DBG("fmt = <%.2x>\n",fmt);

    SAFE_MEM_READ(&channels,2,mptr,pos,size);
    DBG("channels = <%i>\n",channels);

    SAFE_MEM_READ(&rate,4,mptr,pos,size);
    DBG("rate = <%i>\n",rate);

    /* do not read bytes/sec and block align */
    *pos +=6;

    SAFE_MEM_READ(&bits_per_sample,2,mptr,pos,size);
    DBG("bits/sample = <%i>\n",bits_per_sample);

    fmt_desc->subtype = fmt;
    sample_size = bits_per_sample>>3;
    fmt_desc->rate = rate;
    fmt_desc->channels = channels;

    if( (fmt == 0x01) && (sample_size == 1)){
      ERROR("Sorry, we don't support PCM 8 bit\n");
      return -1;
    }

    *pos+=chunk_size-16;

    for(;;) {

      SAFE_MEM_READ(tag,4,mptr,pos,size);
      DBG("tag = <%.4s>\n",tag);
	
      SAFE_MEM_READ(&chunk_size,4,mptr,pos,size);
      DBG("chunk size = <%i>\n",chunk_size);
	
      if(!strncmp(tag,"data",4))
	break;

      *pos += chunk_size;
    }

    return 0;

  } else {
    ERROR("write support for in-memory file not implemented!\n");
    return -1;
  }
}

int wav_mem_close(unsigned char* mptr, unsigned long* pos,
		  struct amci_file_desc_t* fmt_desc, int options, long h_codec, struct amci_codec_t *codec) {
  return 0;
}







