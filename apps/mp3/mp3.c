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

/**
 * @file plug-in/mp3/mp3.c
 * lame-MP3 support 
 * This plug-in writes MP3 files using lame encoder. 
 *
 * Set LAME_DIR in Makefile first!
 *
 * See http://lame.sourceforge.net/ .
 *
 */

#define MP3_phone  1 

static int MP3_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_MP3( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );
static long MP3_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
static void MP3_destroy(long h_inst);
static int MP3_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec);
static int MP3_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec,
					 struct amci_codec_t *codec);
static unsigned int mp3_bytes2samples(long h_codec, unsigned int num_bytes);
static unsigned int mp3_samples2bytes(long h_codec, unsigned int num_samples);

BEGIN_EXPORTS( "mp3" )
    BEGIN_CODECS
      CODEC( CODEC_MP3, Pcm16_2_MP3, MP3_2_Pcm16, (amci_plc_t)0, (amci_codec_init_t)MP3_create, (amci_codec_destroy_t)MP3_destroy, mp3_bytes2samples, mp3_samples2bytes)
    END_CODECS
    
    BEGIN_PAYLOADS
    END_PAYLOADS

    BEGIN_FILE_FORMATS
      BEGIN_FILE_FORMAT( "MP3", "mp3", "audio/x-mp3", MP3_open, MP3_close)
        BEGIN_SUBTYPES
          SUBTYPE( MP3_phone,  "MP3",  8000, 1, CODEC_MP3 )
        END_SUBTYPES
      END_FILE_FORMAT
    END_FILE_FORMATS

END_EXPORTS


void no_output(const char *format, va_list ap)
{
    return;
//    (void) vfprintf(stdout, format, ap);
}

long MP3_create(const char* format_parameters, amci_codec_fmt_info_t* format_description) {
    lame_global_flags* gfp;
    int ret_code;

    DBG("MP3: creating lame %s\n", get_lame_version());
    format_description[0].id = 0; 
    gfp = lame_init(); 
    
    if (!gfp) 
	return -1;

    lame_set_errorf(gfp, &no_output);
    lame_set_debugf(gfp, &no_output);
    lame_set_msgf(gfp, &no_output);
    
    lame_set_num_channels(gfp,1);
    lame_set_in_samplerate(gfp,8000);
    lame_set_brate(gfp,16);
    lame_set_mode(gfp,3); // mono
    lame_set_quality(gfp,2);   /* 2=high  5 = medium  7=low */ 

    id3tag_init(gfp);
    id3tag_set_title(gfp, "mp3 voicemail by iptel.org");
    ret_code = lame_init_params(gfp);
    
    if (ret_code < 0) {
	DBG("lame encoder init failed: return code is %d\n", ret_code);
	return -1;
    }

    return (long)gfp;
}

void MP3_destroy(long h_inst) {
  if (h_inst)
    free((lame_global_flags*) h_inst);
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

  int ret =  lame_encode_buffer((lame_global_flags*)h_codec, 
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
  case -4: ERROR("psycho acoustic problems\n"); break;
  }

  return ret;
}

static int MP3_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec )
{
    ERROR("MP3 decoding not supported yet.\n");
    return -1;

/*     mp3data_struct mp3data; */
/*     int dec_length = 0; */
/*     dec_length = lame_decode( */
/*         in_buf, */
/*         size, */
/*         (short*) out_buf, // left channel */
/*         (short*) out_buf); //right channel -- we assume mono. */
        
/*     return dec_length*2; // sample size = 2 */
}

#define SAFE_READ(buf,s,fp,sr) \
    sr = fread(buf,s,1,fp);\
    if((sr != 1) || ferror(fp)) return -1;

#define SAFE_WRITE(buf,s,fp) \
    fwrite(buf,s,1,fp);\
    if(ferror(fp)) return -1;

static int MP3_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{
//    mp3data_struct mp3data;
//    unsigned char* mp_hdr;
//    short* dummy_pcm;
    
    DBG("mp3_open.\n");

    if(options == AMCI_RDONLY){
	ERROR("Sorry, MP3 file reading is not supported yet.\n");
	return -1;

/* 	if (!(mp_hdr = malloc(0x80))) { */
/* 	    ERROR("MP3: could not allocate memory for mp3 header decode.\n"); */
/* 	    return -1; */
/* 	} */
/* 	if (!(dummy_pcm = malloc(320*2))) { */
/* 	    ERROR("MP3: could not allocate memory for mp3 header decode.\n"); */
/* 	    return -1; */
/* 	} */

/* 	SAFE_READ(mp_hdr, 1, 0x80, fp); */
/* 	if (lame_decode_headers(mp_hdr, 0x80, dummy_pcm, dummy_pcm, &mp3data)==-1) { */
/* 	    ERROR("MP3: cannot decode MP3 header."); */
/* 	    return -1; */
/* 	} */

/* 	if (mp3data.header_parsed) { */
/* 	    fmt_desc->subtype = 1; */
/* 	    fmt_desc->sample = 2; */
/* 	    fmt_desc->rate = mp3data.bitrate; */
/* 	    fmt_desc->channels = mp3data.stereo; */
/* 	} else { */
/* 	    ERROR("MP3: header could not be parsed.\n"); */
/* 	} */

/* 	free(mp_hdr); */
/* 	free(dummy_pcm); */

/* 	return 0; */
    }  else {
      return 0;
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
	if ((final_samples = lame_encode_flush((lame_global_flags *)h_codec,mp3buffer, 7200))) {
	    fwrite(mp3buffer, 1, final_samples, fp);
	    DBG("MP3: flushing %d bytes from MP3 encoder.\n", final_samples);
	}
	lame_mp3_tags_fid((lame_global_flags *)h_codec,fp);
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

static unsigned int mp3_samples2bytes(long h_codec, unsigned int num_samples)
{
	// we don't support MP3 file reading so this is not needed 
	// (would be maybe num_samples*8)

	//	WARN("size calculation not possible with MP3 codec.\n");
    return num_samples;
}


