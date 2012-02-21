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

#include "amci.h"
#include "codecs.h" 
#include "../../log.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "iLBC_define.h"
#include "iLBC_encode.h"
#include "iLBC_decode.h"
#include "constants.h"
/**
 * @file plug-in/ilbc/ilbc.c
 * iLBC support 
 * This plug-in imports the Internet Low Bitrate Codec. 
 *
 * See http://www.ilbcfreeware.org/ . 
 * Features: <ul>
 *           <li>iLBC30 codec/payload/subtype
 *           <li>iLBC20 codec/payload/subtype
 *           <li>ilbc file format including iLBC30/iLBC20 
 *           </ul>
 *
 */

/** @def ILBC30 subtype declaration. */
#define ILBC30  30 
#define ILBC20  20

extern const iLBC_ULP_Inst_t ULP_20msTbl;
extern const iLBC_ULP_Inst_t ULP_30msTbl;

static int iLBC_2_Pcm16_Ext( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			     unsigned int channels, unsigned int rate, long h_codec, int mode );

static int iLBC_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static int iLBC_PLC( unsigned char* out_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_iLBC( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );
static long iLBC_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
static void iLBC_destroy(long h_inst);
static int iLBC_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec);
static int iLBC_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec, struct amci_codec_t *codec);

static unsigned int ilbc_bytes2samples(long, unsigned int);
static unsigned int ilbc_samples2bytes(long, unsigned int);

BEGIN_EXPORTS( "ilbc" , AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

  BEGIN_CODECS
    CODEC( CODEC_ILBC, Pcm16_2_iLBC, iLBC_2_Pcm16, iLBC_PLC,
           iLBC_create, 
           iLBC_destroy,
           ilbc_bytes2samples, ilbc_samples2bytes )
  END_CODECS
    
  BEGIN_PAYLOADS
    PAYLOAD( -1, "iLBC", 8000, 8000, 1, CODEC_ILBC, AMCI_PT_AUDIO_FRAME )
  END_PAYLOADS

  BEGIN_FILE_FORMATS
    BEGIN_FILE_FORMAT( "iLBC", "ilbc", "audio/iLBC", iLBC_open, iLBC_close, 0, 0)
      BEGIN_SUBTYPES
        SUBTYPE( ILBC30,  "iLBC30",  8000, 1, CODEC_ILBC )
        SUBTYPE( ILBC20,  "iLBC20",  8000, 1, CODEC_ILBC )
      END_SUBTYPES
    END_FILE_FORMAT
  END_FILE_FORMATS

END_EXPORTS

typedef struct {
  iLBC_Enc_Inst_t iLBC_Enc_Inst;
  iLBC_Dec_Inst_t iLBC_Dec_Inst;
  int mode;
} iLBC_Codec_Inst_t;

static unsigned int ilbc_bytes2samples(long h_codec, unsigned int num_bytes)
{
  iLBC_Codec_Inst_t* codec_inst = (iLBC_Codec_Inst_t*) h_codec;
  if (codec_inst->mode == 30)
    return 240 * (num_bytes / 50);
  else
    return 160 * (num_bytes / 38);
}

static unsigned int ilbc_samples2bytes(long h_codec, unsigned int num_samples)
{
  iLBC_Codec_Inst_t* codec_inst = (iLBC_Codec_Inst_t*) h_codec;
  if (codec_inst->mode == 30)
    return (num_samples / 240) * 50;
  else
    return (num_samples / 160) * 38;
}

long iLBC_create(const char* format_parameters, amci_codec_fmt_info_t* format_description) {

  iLBC_Codec_Inst_t* codec_inst;
  int mode;
  char* mbegin;
  char* msep;
  char modeb[8];
  
  if ((!format_parameters)||(!*format_parameters)||(!(mbegin=strstr(format_parameters, "mode")))) {
    mode = 30; // default to 30 ms mode if no parameter given.
  } else {
    msep=mbegin;
    while (*msep!='=' && *msep!='\0') msep++;
    msep++; mbegin=msep;
    while (*msep!='=' && *msep!='\0') msep++;
    if ((msep-mbegin)>8) {
      DBG("Error in fmtp line >>'%s<<.\n", format_parameters); 
      mode=30;
    } else {
      memcpy(modeb, mbegin, msep-mbegin);
      modeb[msep-mbegin]='\0';
      if ((!(mode=atoi(modeb))) || (mode != 30 && mode!= 20)) {
	DBG("Error in fmtp line >>'%s<<.\n", format_parameters);
	mode=30;
      }
    }
  }
  format_description[0].id = AMCI_FMT_FRAME_LENGTH ;
  format_description[0].value = mode;
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = mode==30 ? 240 : 160;
  format_description[2].id = AMCI_FMT_ENCODED_FRAME_SIZE;
  format_description[2].value = mode==30 ? 50 : 38;
  format_description[3].id = 0;
    
  if (format_parameters) {
    DBG("ilbc with format parameters : '%s', mode=%d.\n", format_parameters, mode);
  }

  codec_inst = (iLBC_Codec_Inst_t*)malloc(sizeof(iLBC_Codec_Inst_t));
  codec_inst->mode = mode;

  if (!codec_inst) 
    return -1;

  initEncode(&codec_inst->iLBC_Enc_Inst, mode);
  initDecode(&codec_inst->iLBC_Dec_Inst, mode, 0 /* 1=use_enhancer */);
  
  return (long)codec_inst;
}

void iLBC_destroy(long h_inst) {
  if (h_inst)
    free((char *) h_inst);
}

int Pcm16_2_iLBC( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{ 
  short* in_b = (short*)in_buf;

  float block[BLOCKL_MAX];
  int i,k;
  iLBC_Codec_Inst_t* codec_inst;
  int out_buf_offset=0;

  if (!h_codec){
    ERROR("iLBC codec not initialized.\n");
    return 0;
  }
  if ((channels!=1)||(rate!=8000)) {
    ERROR("Unsupported input format for iLBC encoder.\n");
    return 0;
  }
  
  codec_inst = (iLBC_Codec_Inst_t*)h_codec;

  for (i=0;i< size / (2*codec_inst->iLBC_Enc_Inst.blockl);i++) {  
    /* convert signal to float */
    for (k=0; k<codec_inst->iLBC_Enc_Inst.blockl; k++)
      block[k] = in_b[i*codec_inst->iLBC_Enc_Inst.blockl + k];
    /* do the actual encoding */
    iLBC_encode((unsigned char *)(out_buf+out_buf_offset), block, &codec_inst->iLBC_Enc_Inst);
    out_buf_offset+=codec_inst->iLBC_Enc_Inst.no_of_bytes;
  }
 
  
  return out_buf_offset;
}

static int iLBC_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec )
{
  return iLBC_2_Pcm16_Ext(out_buf,in_buf,size,channels,rate,h_codec,1/*Normal*/);
}

static int iLBC_PLC( unsigned char* out_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec )
{
  iLBC_Codec_Inst_t* codec_inst;
  unsigned int in_size;

  codec_inst = (iLBC_Codec_Inst_t*)h_codec;

  in_size = (codec_inst->iLBC_Dec_Inst.no_of_bytes * size)
    / (2*codec_inst->iLBC_Dec_Inst.blockl);

  return iLBC_2_Pcm16_Ext(out_buf,NULL,in_size,channels,rate,h_codec,0/*PL*/);
}

static int iLBC_2_Pcm16_Ext( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			     unsigned int channels, unsigned int rate, long h_codec, int mode )
{
  short* out_b = (short*)out_buf;
  int i,k,noframes;
  float decblock[BLOCKL_MAX];
  float dtmp;
  iLBC_Codec_Inst_t* codec_inst;
  
  short out_buf_offset=0;

  if (!h_codec){
    ERROR("iLBC codec not initialized.\n");
    return 0;
  }
  if ((channels!=1)||(rate!=8000)) {
    ERROR("Unsupported input format for iLBC encoder.\n");
    return 0;
  }

  codec_inst = (iLBC_Codec_Inst_t*)h_codec;
  
  noframes = size / codec_inst->iLBC_Dec_Inst.no_of_bytes;
  if (noframes*codec_inst->iLBC_Dec_Inst.no_of_bytes != size) {
    WARN("Dropping extra %d bytes from iLBC packet.\n",
	 size - noframes*codec_inst->iLBC_Dec_Inst.no_of_bytes);
  }

  for (i=0;i<noframes;i++) {
    /* do actual decoding of block */
      
    iLBC_decode(decblock,in_buf+i*codec_inst->iLBC_Dec_Inst.no_of_bytes,
		&codec_inst->iLBC_Dec_Inst, mode/* mode 0=PL, 1=Normal */);

    for (k=0; k<codec_inst->iLBC_Dec_Inst.blockl; k++){
      dtmp=decblock[k];
      if (dtmp<MIN_SAMPLE)
	dtmp=MIN_SAMPLE;
      else if (dtmp>MAX_SAMPLE)
	dtmp=MAX_SAMPLE;
      out_b[out_buf_offset] = (short) dtmp;
      out_buf_offset++;
    }
  }

  return out_buf_offset*2; // sample size: 2 bytes (short)
}

#define SAFE_READ(buf,s,fp,sr) \
    sr = fread(buf,s,1,fp);\
    if((sr != 1) || ferror(fp)) return -1;

#define SAFE_WRITE(buf,s,fp) \
    fwrite(buf,s,1,fp);\
    if(ferror(fp)) return -1;

static int ilbc_read_header(FILE* fp, struct amci_file_desc_t* fmt_desc)
{
  unsigned int s;

  char tag[9]={'\0'};

  if(!fp)
    return -1;
  rewind(fp);

  DBG("trying to read iLBC file\n");

  SAFE_READ(tag,9,fp,s);
  DBG("tag = <%.9s>\n",tag);

  if(!strncmp(tag,"#iLBC30\n",9) ){
    fmt_desc->subtype=ILBC30;
  } else if (!strncmp(tag,"#iLBC20\n",9)) {
    fmt_desc->subtype=ILBC20;
  } else {
    DBG("wrong format !");
    return -1;
  }
  fmt_desc->rate = 8000;
  fmt_desc->channels = 1;

  fseek(fp, 0, SEEK_END);
  fmt_desc->data_size = ftell(fp) - 9; // file size - header size
  fseek(fp, 9, SEEK_SET);   // get at start of samples
    
  return 0;
}

int iLBC_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{
  DBG("ilbc_open.\n");
  if(options == AMCI_RDONLY){
    return ilbc_read_header(fp,fmt_desc);
  }  else {
    if (fmt_desc->subtype == ILBC30) {
      DBG("writing: #iLBC30\n");
      SAFE_WRITE("#iLBC30\n",9,fp);
    } else if (fmt_desc->subtype == ILBC20) {
      DBG("writing: #iLBC20\n");
      SAFE_WRITE("#iLBC20\n",9,fp);
    } else {
      ERROR("Unsupported ilbc sub type %d\n", fmt_desc->subtype);
      return -1;
    }
    return 0;
  }
}

int iLBC_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec, struct amci_codec_t *codec)
{
  DBG("iLBC_close.\n");
  return 0;
}




