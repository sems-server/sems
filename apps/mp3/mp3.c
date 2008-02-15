/*
 * $Id$
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

#include "amci.h"
#include "codecs.h" 
#include "log.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lame.h"

#ifdef WITH_MPG123DECODER
#include "mpg123.h"
#endif

/**
 * @file apps/mp3/mp3.c
 * lame-MP3 support 
 * This plug-in writes MP3 files using lame encoder. 
 *
 * See http://lame.sourceforge.net/ .
 * 
 * If WITH_MPG123DECODER=yes (default), mpg123 mp3 decoding is supported as well.
 * 
 */

#define MP3_phone  1 

static int MP3_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, 
			unsigned int size, unsigned int channels, unsigned int rate, 
			long h_codec );

static int MP3_ModuleLoad(void);
static void MP3_ModuleDestroy(void);

static int Pcm16_2_MP3( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );
static long MP3_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
static void MP3_destroy(long h_inst);
static int MP3_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec);
static int MP3_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec,
					 struct amci_codec_t *codec);
static unsigned int mp3_bytes2samples(long h_codec, unsigned int num_bytes);
static unsigned int mp3_samples2bytes(long h_codec, unsigned int num_samples);

BEGIN_EXPORTS( "mp3", MP3_ModuleLoad, MP3_ModuleDestroy )
    BEGIN_CODECS
CODEC( CODEC_MP3, Pcm16_2_MP3, MP3_2_Pcm16, (amci_plc_t)0, (amci_codec_init_t)MP3_create, (amci_codec_destroy_t)MP3_destroy, mp3_bytes2samples, mp3_samples2bytes)
    END_CODECS
    
    BEGIN_PAYLOADS
    END_PAYLOADS

    BEGIN_FILE_FORMATS
      BEGIN_FILE_FORMAT( "MP3", "mp3", "audio/x-mp3", MP3_open, MP3_close, 0, 0)
        BEGIN_SUBTYPES
          SUBTYPE( MP3_phone,  "MP3",  8000, 1, CODEC_MP3 )
        END_SUBTYPES
      END_FILE_FORMAT
    END_FILE_FORMATS

END_EXPORTS

int MP3_ModuleLoad(void) {
#ifdef WITH_MPG123DECODER
  int res;
  if ((res = mpg123_init()) != MPG123_OK) {
    ERROR("initializing mpg123 failed: %d\n", res);
    return -1;
  }
#endif

  DBG("MP3 module loaded.\n");
  return 0;
}

void MP3_ModuleDestroy(void) {
#ifdef WITH_MPG123DECODER
  mpg123_exit();
#endif
  DBG("MP3 module destroyed.\n");
}

#define MP3_FRAMESAMPLES   1152
/* for 128kbit 44.1 khz  FrameSize = 144 * BitRate / (SampleRate + Padding) */
#define MP3_MAXFRAMEBYTES   417 
#define DECODED_BUF_SIZE MP3_FRAMESAMPLES * 2 * 2 // space for two decoded frames
#define CODED_BUF_SIZE  MP3_MAXFRAMEBYTES * 2     // space for two encoded frames

typedef struct {
  lame_global_flags* gfp;
#ifdef WITH_MPG123DECODER
  mpg123_handle* mpg123_h;
  long rate;
  int channels, enc;

/*   unsigned char decoded_buf[DECODED_BUF_SIZE]; */
/*   size_t decoded_begin; */
/*   size_t decoded_end; */
/*   unsigned char coded_buf[CODED_BUF_SIZE]; */
/*   size_t coded_pos; */
#endif  
} mp3_coder_state;

void no_output(const char *format, va_list ap)
{
    return;
//    (void) vfprintf(stdout, format, ap);
}

long MP3_create(const char* format_parameters, amci_codec_fmt_info_t* format_description) {
  mp3_coder_state* coder_state;
  int ret_code;
  
  coder_state = malloc(sizeof(mp3_coder_state));
  if (!coder_state) {
    ERROR("no memory for allocating mp3 coder state\n");
    return -1;
  }
  
  DBG("MP3: creating lame %s\n", get_lame_version());
  format_description[0].id = 0; 
  coder_state->gfp = lame_init(); 
    
  if (!coder_state->gfp) {
    ERROR("initialiting lame\n");
    free(coder_state);
    return -1;
  }

  lame_set_errorf(coder_state->gfp, &no_output);
  lame_set_debugf(coder_state->gfp, &no_output);
  lame_set_msgf(coder_state->gfp, &no_output);
  
  lame_set_num_channels(coder_state->gfp,1);
  lame_set_in_samplerate(coder_state->gfp,8000);
  lame_set_brate(coder_state->gfp,16);
  lame_set_mode(coder_state->gfp,3); // mono
  lame_set_quality(coder_state->gfp,2);   /* 2=high  5 = medium  7=low */ 
  
  id3tag_init(coder_state->gfp);
  id3tag_set_title(coder_state->gfp, "mp3 voicemail by iptel.org");
  ret_code = lame_init_params(coder_state->gfp);
  
  if (ret_code < 0) {
    ERROR("lame encoder init failed: return code is %d\n", ret_code);
    free(coder_state);
    return -1;
  }
  

#ifdef WITH_MPG123DECODER
  coder_state->mpg123_h = mpg123_new(NULL, NULL);
  if (!coder_state->mpg123_h) {
    ERROR("initializing mpg123 decoder instance\n");
    return -1;
  }

  /* suppress output */
  mpg123_param(coder_state->mpg123_h, MPG123_FLAGS, MPG123_QUIET /* | MPG123_FORCE_MONO */,0);

/*   mpg123_param(coder_state->mpg123_h, MPG123_VERBOSE, 0, 0); */

#endif

  return (long)coder_state;
}

void MP3_destroy(long h_inst) {
  if (h_inst){
    if (((mp3_coder_state*)h_inst)->gfp)
      lame_close(((mp3_coder_state*)h_inst)->gfp);
#ifdef WITH_MPG123DECODER
    mpg123_delete(((mp3_coder_state*)h_inst)->mpg123_h);
#endif

    free((mp3_coder_state*)h_inst);
  }
}

static int Pcm16_2_MP3( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{ 

  if (!h_codec){
    ERROR("MP3 codec not initialized.\n");
    return 0;
  }

  if ((channels!=1)||(rate!=8000)) {
    ERROR("Unsupported input format for MP3 encoder.\n");
    return 0;
  }

/*   DBG("h_codec=0x%lx; in_buf=0x%lx; size=%i\n", */
/*       (unsigned long)h_codec,(unsigned long)in_buf,size);  */

  int ret =  lame_encode_buffer(((mp3_coder_state*)h_codec)->gfp, 
				(short*) in_buf,        // left channel
				/* (short*) in_buf */0, // right channel
				size / 2 ,              // no of samples (size is in bytes!)
				out_buf,
  				AUDIO_BUFFER_SIZE); 

  switch(ret){
    // 0 is valid: if not enough samples for an mp3 
    //frame lame will not return anything
  case 0:  /*DBG("lame_encode_buffer returned 0\n");*/ break; 
  case -1: ERROR("mp3buf was too small\n"); break;
  case -2: ERROR("malloc() problem\n"); break;
  case -3: ERROR("lame_init_params() not called\n"); break;
  case -4: ERROR("psycho acoustic problems. uh!\n"); break;
  }

  return ret;
}


static unsigned int mp3_samples2bytes(long h_codec, unsigned int num_samples)
{
#ifndef WITH_MPG123DECODER
  return num_samples;
#else

  /* 
   * 150 bytes is about one frame as produced by the mp3 writer.
   * for higher bitrate files this will only introduce the small performance 
   * penalty of multiple read() calls per frame-decode.
   */
  return 150;

/*   or a full frame? */
/*    we don't know bitrate - so use 128000 as max bitrate */
/*     144 * BitRate / (SampleRate + Padding) */  
/*   unsigned int res =  144 * 128000 / (((mp3_coder_state*)h_codec)->rate + 1); */
/*   if (res > AUDIO_BUFFER_SIZE) */
/*     res = AUDIO_BUFFER_SIZE; */
/*   return res; */

#endif
}

static int MP3_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			unsigned int channels, unsigned int rate, 
			long h_codec )
{

#ifndef WITH_MPG123DECODER
    ERROR("MP3 decoding support not compiled in.\n");
    return -1;
#else
    int res;
    size_t decoded_size;
    mp3_coder_state* coder_state;

    if (!h_codec) {
      ERROR("mp3 decoder not initialized!\n");
      return -1;
    }
        
    coder_state = (mp3_coder_state*)h_codec;
    res = mpg123_decode(coder_state->mpg123_h, in_buf, size, 
			out_buf, AUDIO_BUFFER_SIZE, &decoded_size);
    
    if (res == MPG123_NEW_FORMAT) {
      WARN("intermediate mp3 file format change!\n");
    }
    if (res == MPG123_ERR) {
      ERROR("decoding mp3: '%s'\n", 
	    mpg123_strerror(coder_state->mpg123_h));
      return -1;
    }
/*     DBG("mp3: decoded %d\n", decoded_size); */
    return decoded_size;
#endif
}

#define SAFE_READ(buf,s,fp,sr) \
    sr = fread(buf,s,1,fp);\
    if((sr != 1) || ferror(fp)) return -1;

#define SAFE_WRITE(buf,s,fp) \
    fwrite(buf,s,1,fp);\
    if(ferror(fp)) return -1;

static int MP3_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{

#ifdef WITH_MPG123DECODER
  unsigned char mp_rd_buf[20]; 
  size_t sr, decoded_size;
  mp3_coder_state* coder_state;
  int res;
  DBG("mp3_open.\n");
#endif

   if(options == AMCI_RDONLY){
#ifndef WITH_MPG123DECODER
    ERROR("MP3 decoding support not compiled in.\n");
    return -1;
#else
     if (!h_codec) {
       ERROR("mp3 decoder not initialized!\n");
       return -1;
     }
     coder_state = (mp3_coder_state*)h_codec;
     
     DBG("Initializing mpg123 codec state.\n");
     res = mpg123_open_feed(coder_state->mpg123_h);
     if (MPG123_ERR == res) {
       ERROR("mpg123_open_feed returned mpg123 error '%s'\n", 
	     mpg123_strerror(coder_state->mpg123_h));
       return -1;
     }

     /* read until we know the format */
     while (res!= MPG123_NEW_FORMAT) {
       SAFE_READ(mp_rd_buf, 20, fp, sr);

       res = mpg123_decode(coder_state->mpg123_h, mp_rd_buf, 20, 
			   mp_rd_buf, 0, &decoded_size);
       if (res == MPG123_ERR) {
	 ERROR("trying to determine MP3 file format: '%s'\n", 
	       mpg123_strerror(coder_state->mpg123_h));
	 return -1;
       }
     }
          
     mpg123_getformat(coder_state->mpg123_h, 
		      &coder_state->rate, &coder_state->channels, &coder_state->enc);

     DBG("mpg123: New format: %li Hz, %i channels, encoding value %i\n", 
	 coder_state->rate, coder_state->channels, coder_state->enc);

     fmt_desc->subtype   = 1; // ?
     fmt_desc->rate      = coder_state->rate;
     fmt_desc->channels  = coder_state->channels;
     fmt_desc->data_size = -1;

     /* set buffering parameters */
     fmt_desc->buffer_size   = MP3_FRAMESAMPLES * 8;
     fmt_desc->buffer_thresh = MP3_FRAMESAMPLES;
     fmt_desc->buffer_full_thresh = MP3_FRAMESAMPLES * 3; 

     return 0;
#endif
   }  
   return 0;
}

static int MP3_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec,
				    struct amci_codec_t *codec)
{
    int final_samples;

    unsigned char  mp3buffer[7200];    
    DBG("MP3: close. \n");
    if(options == AMCI_WRONLY) {
      if (!h_codec || !((mp3_coder_state*)h_codec)->gfp) {
	ERROR("no valid codec handle!\n");
	return -1;
      }

      if ((final_samples = lame_encode_flush(((mp3_coder_state*)h_codec)->gfp,mp3buffer, 7200))) {
	    fwrite(mp3buffer, 1, final_samples, fp);
	    DBG("MP3: flushing %d bytes from MP3 encoder.\n", final_samples);
	}
	lame_mp3_tags_fid(((mp3_coder_state*)h_codec)->gfp,fp);
    }
    return 0;
}


static unsigned int mp3_bytes2samples(long h_codec, unsigned int num_bytes)
{
    // even though we are using CBR (16kbps) and encoding 
    // 128kbps to 16kbps would give num_samples/8 (or num_bytes*8)
    // as MP3 is not used as payload calculating this would make no 
    // sense. The whole frame is encoded anyway.

	//	WARN("size calculation not possible with MP3 codec.\n");
    return num_bytes;
}



