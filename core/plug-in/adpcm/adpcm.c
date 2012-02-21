/*
 * Copyright (C) 2007 iptego GmbH
 * 
 * based on the SUN codec wrapper from twinklephone
 *  Copyright (C) 2005-2007  Michel de Boer <michel@twinklephone.com>
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
#include "g72x.h"
#include <stdlib.h>

#define PCM16_B2S(b) ((b) >> 1)
#define PCM16_S2B(s) ((s) << 1)

/* or ATM-AAL packing  -> RFC3551 has the names AAL2-G726-xy for the big-endian packing */
#define G726_PACK_RFC3551   1 

static long G726_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
static void G726_destroy(long h_inst);

static int Pcm16_2_G726_16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );
static int G726_16_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_G726_24( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );
static int G726_24_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_G726_32( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );
static int G726_32_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_G726_40( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );
static int G726_40_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec );


static unsigned int g726_16_bytes2samples(long, unsigned int);
static unsigned int g726_16_samples2bytes(long, unsigned int);

static unsigned int g726_24_bytes2samples(long, unsigned int);
static unsigned int g726_24_samples2bytes(long, unsigned int);

static unsigned int g726_32_bytes2samples(long, unsigned int);
static unsigned int g726_32_samples2bytes(long, unsigned int);

static unsigned int g726_40_bytes2samples(long, unsigned int);
static unsigned int g726_40_samples2bytes(long, unsigned int);


BEGIN_EXPORTS( "adpcm", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

BEGIN_CODECS
CODEC( CODEC_G726_16, Pcm16_2_G726_16, G726_16_2_Pcm16, AMCI_NO_CODEC_PLC, 
       G726_create, G726_destroy, g726_16_bytes2samples, g726_16_samples2bytes )
CODEC( CODEC_G726_24, Pcm16_2_G726_24, G726_24_2_Pcm16, AMCI_NO_CODEC_PLC, 
       G726_create, G726_destroy, g726_24_bytes2samples, g726_24_samples2bytes )
CODEC( CODEC_G726_32, Pcm16_2_G726_32, G726_32_2_Pcm16, AMCI_NO_CODEC_PLC, 
       G726_create, G726_destroy, g726_32_bytes2samples, g726_32_samples2bytes )
CODEC( CODEC_G726_40, Pcm16_2_G726_40, G726_40_2_Pcm16, AMCI_NO_CODEC_PLC, 
       G726_create, G726_destroy, g726_40_bytes2samples, g726_40_samples2bytes )
END_CODECS
    
BEGIN_PAYLOADS
PAYLOAD( -1, "G726-32", 8000, 8000, 1, CODEC_G726_32, AMCI_PT_AUDIO_LINEAR )
PAYLOAD(  2, "G721",    8000, 8000, 1, CODEC_G726_32, AMCI_PT_AUDIO_LINEAR )
PAYLOAD( -1, "G726-24", 8000, 8000, 1, CODEC_G726_24, AMCI_PT_AUDIO_LINEAR )
PAYLOAD( -1, "G726-40", 8000, 8000, 1, CODEC_G726_40, AMCI_PT_AUDIO_LINEAR )
PAYLOAD( -1, "G726-16", 8000, 8000, 1, CODEC_G726_16, AMCI_PT_AUDIO_LINEAR )
END_PAYLOADS

BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS

struct G726_twoway {
  struct g72x_state to_g726;  
  struct g72x_state from_g726;  
};

static long G726_create(const char* format_parameters, amci_codec_fmt_info_t* format_description) {
  struct G726_twoway* cinst = calloc(1, sizeof(struct G726_twoway));
  if (!cinst)
    return -1;

  g72x_init_state(&cinst->to_g726);
  g72x_init_state(&cinst->from_g726);

  format_description[0].id = 0;
  return (long) cinst;
}

static void G726_destroy(long h_inst) {
  if (h_inst)
    free((struct G726_twoway*) h_inst);
}

// 2 bits/sample
static unsigned int g726_16_bytes2samples(long h_codec, unsigned int num_bytes)
{  return num_bytes << 2; } 
static unsigned int g726_16_samples2bytes(long h_codec, unsigned int num_samples)
{  return num_samples >> 2; }

// 3 bits/sample
static unsigned int g726_24_bytes2samples(long h_codec, unsigned int num_bytes)
{   return (num_bytes * 8) / 3; }
static unsigned int g726_24_samples2bytes(long h_codec, unsigned int num_samples)
{  return (num_samples * 3) / 8; }

// 4 bits/sample
static unsigned int g726_32_bytes2samples(long h_codec, unsigned int num_bytes)
{  return num_bytes << 1; }
static unsigned int g726_32_samples2bytes(long h_codec, unsigned int num_samples)
{  return num_samples >> 1; }

// 5 bits/sample
static unsigned int g726_40_bytes2samples(long h_codec, unsigned int num_bytes)
{  return (num_bytes * 8) / 5; }
static unsigned int g726_40_samples2bytes(long h_codec, unsigned int num_samples)
{  return (num_samples * 5) / 8; }

static int Pcm16_2_G726_16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {
  int i, j;  
  if (!h_codec)
    return -1;

  short* sample_buf = (short*)in_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;

  for (i = 0; i < PCM16_B2S(size); i += 4) {
    out_buf[i >> 2] = 0;
    for (j = 0; j < 4; j++) {
      char v = g723_16_encoder(sample_buf[i+j],
			       AUDIO_ENCODING_LINEAR, 
			       &cs->to_g726);
#ifdef G726_PACK_RFC3551
      out_buf[i >> 2] |= v << (j * 2);
#else
      out_buf[i >> 2] |= v << ((3-j) * 2);
#endif
    }
  }
  
  return PCM16_B2S(size) >> 2;
}

static int G726_16_2_Pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			   unsigned int channels, unsigned int rate, long h_codec ) {
  int i, j;
  if (!h_codec)
    return -1;

  short* pcm_buf = (short*)out_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;

  for (i = 0; i < size; i++) {
    for (j = 0; j < 4; j++) {
      char w;
#ifdef G726_PACK_RFC3551
      w = (in_buf[i] >> (j*2)) & 0x3;
#else 
      w = (in_buf[i] >> ((3-j)*2)) & 0x3;
#endif
      pcm_buf[4*i+j] = g723_16_decoder(w, AUDIO_ENCODING_LINEAR, 
				       &cs->from_g726);
    }
  }

  return PCM16_S2B(size * 4);
}

static int Pcm16_2_G726_24( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {
  int i, j;  
  if (!h_codec)
    return -1;
  short* sample_buf = (short*)in_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;

  for (i = 0; i < PCM16_B2S(size); i += 8) {
    u_int32_t v = 0;
    for (j = 0; j < 8; j++) {
#ifdef G726_PACK_RFC3551
	v |= (g723_24_encoder(sample_buf[i+j],
				    AUDIO_ENCODING_LINEAR, &cs->to_g726)) << (j * 3);
#else
	v |= (g723_24_encoder(sample_buf[i+j],
				    AUDIO_ENCODING_LINEAR, &cs->to_g726)) << ((7-j) * 3);
#endif
    }
    out_buf[(i >> 3) * 3] = (v & 0xff);
    out_buf[(i >> 3) * 3 + 1] = ((v >> 8) & 0xff);
    out_buf[(i >> 3) * 3 + 2] = ((v >> 16) & 0xff);
  }
  
  return (PCM16_B2S(size) >> 3) * 3;
}

static int G726_24_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {
  int i, j;
  if (!h_codec)
    return -1;

  short* pcm_buf = (short*)out_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;

  for (i = 0; i < size; i += 3) {
    u_int32_t v = ((in_buf[i+2]) << 16) |
      ((in_buf[i+1]) << 8) |
      (in_buf[i]);
    
    for (j = 0; j < 8; j++) {
      char w;
#ifdef G726_PACK_RFC3551
	w = (v >> (j*3)) & 0x7;
#else
	w = (v >> ((7-j)*3)) & 0x7;
#endif
	pcm_buf[8*(i/3)+j] = 
	  g723_24_decoder(w, AUDIO_ENCODING_LINEAR, &cs->from_g726);
    }
  }
  
  return PCM16_S2B(size * 8 / 3);
}

static int Pcm16_2_G726_32( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {
  int i, j;  
  if (!h_codec)
    return -1;
  short* sample_buf = (short*)in_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;
  
  for (i = 0; i < PCM16_B2S(size); i += 2) {
    out_buf[i >> 1] = 0;
    for (j = 0; j < 2; j++) {
      char v = g721_encoder(sample_buf[i+j],
			    AUDIO_ENCODING_LINEAR, &cs->to_g726);
      
#ifdef G726_PACK_RFC3551
	out_buf[i >> 1] |= v << (j * 4);
#else 
	out_buf[i >> 1] |= v << ((1-j) * 4);
#endif
    }
  }
  
  return PCM16_B2S(size) >> 1;
}

static int G726_32_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {

  int i, j;
  if (!h_codec)
    return -1;

  short* pcm_buf = (short*)out_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;
  
  for (i = 0; i < size; i++) {
    for (j = 0; j < 2; j++) {
      char w;
#ifdef G726_PACK_RFC3551
      w = (in_buf[i] >> (j*4)) & 0xf;
#else
      w = (in_buf[i] >> ((1-j)*4)) & 0xf;
#endif
      pcm_buf[2*i+j] = 
	g721_decoder(w, AUDIO_ENCODING_LINEAR,  &cs->from_g726);
    }
  }
	
  return PCM16_S2B(size * 2);
}

static int Pcm16_2_G726_40( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {

  int i, j;  
  if (!h_codec)
    return -1;
  short* sample_buf = (short*)in_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;

  for (i = 0; i < PCM16_B2S(size); i += 8) {
    u_int64_t v = 0;
    for (j = 0; j < 8; j++) {
#ifdef G726_PACK_RFC3551
	v |= ((u_int64_t)g723_40_encoder(sample_buf[i+j],
				       AUDIO_ENCODING_LINEAR, &cs->to_g726)) << (j * 5);
#else
	v |= ((u_int64_t)g723_40_encoder(sample_buf[i+j],
						 AUDIO_ENCODING_LINEAR, &cs->to_g726)) << ((7-j) * 5);
#endif
		}
    out_buf[(i >> 3) * 5] = (v & 0xff);
    out_buf[(i >> 3) * 5 + 1] = ((v >> 8) & 0xff);
    out_buf[(i >> 3) * 5 + 2] = ((v >> 16) & 0xff);
    out_buf[(i >> 3) * 5 + 3] = ((v >> 24) & 0xff);
    out_buf[(i >> 3) * 5 + 4] = ((v >> 32) & 0xff);
  }
	
  return (PCM16_B2S(size) >> 3) * 5;
}

static int G726_40_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			    unsigned int channels, unsigned int rate, long h_codec ) {

  int i, j;
  u_int64_t v;
  if (!h_codec)
    return -1;
  
  short* pcm_buf = (short*)out_buf;
  struct G726_twoway* cs = (struct G726_twoway*)h_codec;
  
  for (i = 0; i < size; i += 5) {
    v = ((u_int64_t)in_buf[i+4]) << 32  | 
      (u_int64_t)(in_buf[i+3]) << 24 | 
      (u_int64_t)(in_buf[i+2]) << 16 | 
      (u_int64_t)(in_buf[i+1]) << 8 | 
      (u_int64_t)(in_buf[i]); 
    
    for (j = 0; j < 8; j++) {
      char w;
#ifdef G726_PACK_RFC3551
	w = (v >> (j*5)) & 0x1f;
#else
	w = (v >> ((7-j)*5)) & 0x1f;
#endif      
      pcm_buf[8*(i/5)+j] = 
	g723_40_decoder(w, AUDIO_ENCODING_LINEAR, &cs->from_g726);
    }
  }

  return PCM16_S2B(size * 8 / 5);
}
