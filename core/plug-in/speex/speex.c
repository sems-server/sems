/*
  This is a simple interface to the Speex library for Sems.
  Copyright (C) 2005 Ettore Benedetti e.benedetti@elitel.biz
  updated 2007 Stefan Sayer sayer@iptel.org

  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation.
  
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <stdlib.h>

#include "amci.h"
#include "codecs.h"
#include "speex/speex.h"
#include "../../log.h"

/* Speex constants */
#define SPEEX_FRAME_MS			 20
#define SPEEX_NB_SAMPLES_PER_FRAME 	160
#define SPEEX_WB_SAMPLES_PER_FRAME 	320

/* Default encoder settings */
#define MODE		  5
#define FRAMES_PER_PACKET 1         /* or other implementations choke */
#define BYTES_PER_FRAME  (160 / 8)  /* depends on mode */

/* #ifdef NOFPU */
/* #warning Using integer encoding/decoding algorithms */
/* #else */
/* #warning Using floating point encoding/decoding algorithms */
/* #endif */

/*
  Important: Frame length is always 20ms!

  Narrow-band mode (8khz)
    
  Mode	Bit-rate(bps)	Frame size(bits)	Mips 	Notes
  0		250		5			N/A	No transmission (DTX)
  1		2150		43			6	Vocoder (mostly for comfort noise)
  2		5950		119			9	Very noticeable artifacts/noise, good intelligibility
  3		8000		160			10	Artifacts/noise sometimes noticeable
  4		11000		220			14	Artifacts usually noticeable only with headphones
  5		15000		300			11	Need good headphones to tell the difference
  6		18200		364			17.5	Hard to tell the difference even with good headphones
  7		24600		492			14.5	Completely transparent for voice, good quality music
  8		3950		79			10.5	Very noticeable artifacts/noise, good intelligibility

  Wide-band mode (16khz). A wide-band packet is composed
  by a narrow-band packet plus an extension, described here.
  The two band components are created by means of a QMF.
    
  Mode	Additional bit-rate(bps)	Additional frame size(bits)
  0		+200				+4
  1		+1800				+36
  2		+5600				+112
  3		+9600				+192
  4		+17600				+352
*/

int Pcm16_2_SpeexNB( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );
int SpeexNB_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );

long speexNB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
void speexNB_destroy(long handle);

static unsigned int speexNB_bytes2samples(long, unsigned int);
static unsigned int speexNB_samples2bytes(long, unsigned int);

BEGIN_EXPORTS("speex")

BEGIN_CODECS
CODEC(CODEC_SPEEX_NB, Pcm16_2_SpeexNB, SpeexNB_2_Pcm16, NULL, speexNB_create, speexNB_destroy, 
      speexNB_bytes2samples, speexNB_samples2bytes)
END_CODECS
  
BEGIN_PAYLOADS
PAYLOAD(-1, "speex", 8000, 1, CODEC_SPEEX_NB, AMCI_PT_AUDIO_FRAME)
END_PAYLOADS
  
BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS

typedef struct {
  void *state;
  SpeexBits bits;
#ifndef NOFPU
  float pool[AUDIO_BUFFER_SIZE];
#endif
} OneWay;

typedef struct {
  OneWay *encoder;
  OneWay *decoder;
    
  /* Encoder settings */
  int frames_per_packet;
  int mode;				/* 0..15 */
    
  /* Decoder settings */
  int perceptual;			/* 0,1 */
    
} SpeexState;

/* See table, above */
static const int nb_encoded_frame_bits[] = { 5, 43, 119, 160, 220, 300, 364, 492, 79 };

/*
  Search for a parameter assignement in input string.
  If it's not found *param_value is null, otherwise *param_value points to the right hand term.
  In both cases a pointer suitable for a new search is returned
*/
static char* read_param(char* input, const char *param, char** param_value)
{
  int param_size;

  /* Eat spaces and semi-colons */
  while (*input==' ' && *input==';')
    input++;

  *param_value = NULL;
  param_size = strlen(param);
  if (strncmp(input, param, param_size))
    return input;
  if (*(input+param_size) != '=')
    return input;
  input+=param_size+1;

  /* Found and discarded a matching parameter */
  *param_value = input;
  while (*input && *input!=' ' && *input!=';')
    input++;

  if (*input)
    *input++ = 0;
    
  return input;
}

long speexNB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  SpeexState* ss;
  const int BLEN = 63;
  int bits;
        
  ss = (SpeexState*) calloc(1, sizeof(SpeexState));
    
  if (!ss)
    return -1;
    
  /* Note that
     1) SEMS ignore a=ptime: SDP parameter so we can choose the one we like
     2) Multiple frames in a packet don't need to be byte-aligned
  */
  ss->frames_per_packet = FRAMES_PER_PACKET;

  /* Compression mode (see table) */
  ss->mode = MODE;
    
  /* Perceptual enhancement. Turned on by default */
  ss->perceptual = 1;

  /* See draft-herlein-avt-rtp-speex-00.txt */
  if (format_parameters && strlen(format_parameters)<=BLEN){
	
    char buffer2[BLEN+1];
    char *buffer = buffer2;

    strcpy(buffer, format_parameters);
	
    while (*buffer) {
      char* error;
      char *param_value;
	    
      /* Speex encoding mode (assume NB) */
      buffer=read_param(buffer, "mode", &param_value);
      if (param_value) {
	int mode;
	if (strcmp(param_value, "any")) {
	  mode = strtol(param_value, &error, 10);
	  if (!*error && mode>=0 && mode<=8)
	    ss->mode = mode;
	}
	continue;
      }

      /* Perceptual enhancement */
      buffer=read_param(buffer, "penh", &param_value);
      if (param_value) {
	if (!strcmp(param_value,"no"))
	  ss->perceptual=0;
	continue;
      }

      /* Unknown parameter */
      if (*buffer) {
	param_value = buffer;
	while (*buffer && *buffer!=';')
	  buffer++;
		
	if (*buffer)
	  *buffer++ = 0;

	WARN("SDP parameter fmtp: %s not set in speex.\n", param_value);
      }
    }
  }
    
  /* Meaningful bits in one packet */
  bits = nb_encoded_frame_bits[ss->mode] * ss->frames_per_packet;
    
  format_description[0].id = AMCI_FMT_FRAME_LENGTH;
  format_description[0].value = SPEEX_FRAME_MS * ss->frames_per_packet;
    
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = SPEEX_NB_SAMPLES_PER_FRAME * ss->frames_per_packet;
    
  format_description[2].id = AMCI_FMT_ENCODED_FRAME_SIZE;
  format_description[2].value = bits/8 + (bits % 8 ? 1 : 0) + 1;

  DBG("set AMCI_FMT_FRAME_LENGTH to %d\n", format_description[0].value);
  DBG("set AMCI_FMT_FRAME_SIZE to %d\n", format_description[1].value);
  DBG("set AMCI_FMT_ENCODED_FRAME_SIZE to %d\n", format_description[2].value);
    
  format_description[3].id = 0;
    
  DBG("SpeexState %p inserted with mode %d and %d frames per packet,\n", ss, ss->mode, ss->frames_per_packet);
    
  return (long)ss;
}

void speexNB_destroy(long handle)
{
  SpeexState* ss = (SpeexState*) handle;
    
  DBG("SpeexDestroy for handle %ld\n", handle);
    
  if (!ss)
    return;
    
  if (ss->encoder) {
    speex_encoder_destroy(ss->encoder->state);
    speex_bits_destroy(&ss->encoder->bits);
    free(ss->encoder);
  }
    
  if (ss->decoder) {
    speex_decoder_destroy(ss->decoder->state);
    speex_bits_destroy(&ss->decoder->bits);
    free(ss->decoder);
  }
    
  free(ss);
}

static unsigned int speexNB_bytes2samples(long h_codec, unsigned int num_bytes) {
  return  (SPEEX_NB_SAMPLES_PER_FRAME * num_bytes) / BYTES_PER_FRAME;
}

static unsigned int speexNB_samples2bytes(long h_codec, unsigned int num_samples) {
  return BYTES_PER_FRAME * num_samples /  SPEEX_NB_SAMPLES_PER_FRAME; 
}


int Pcm16_2_SpeexNB( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec )
{
  SpeexState* ss;
  OneWay *encoder;
  short* pcm = (short*) in_buf;
  char* buffer = (char*)out_buf;
  div_t blocks;
    
  ss = (SpeexState*) h_codec;
    
  if (!ss || channels!=1 || rate!=8000)
    return -1;
    
  /* encoder init */
  if (!(encoder=ss->encoder)) {
    ss->encoder = encoder = (OneWay*) calloc(1, sizeof(OneWay));
    if (!encoder)
      return -1;
    encoder->state = speex_encoder_init(&speex_nb_mode);
    speex_bits_init(&encoder->bits);
    speex_encoder_ctl(encoder->state, SPEEX_SET_MODE, &ss->mode);
  }
    
  blocks = div(size, sizeof(short)*SPEEX_NB_SAMPLES_PER_FRAME);
  if (blocks.rem) {
    ERROR("Pcm16_2_Speex: not integral number of blocks %d.%d\n", blocks.quot, blocks.rem);
    return -1;
  }
    
  /* For each chunk of ss->frame_size bytes, encode a single frame */
  speex_bits_reset(&encoder->bits);
  while (blocks.quot--) {

#ifdef NOFPU
    speex_encode_int(encoder->state, pcm, &encoder->bits);
    pcm+=SPEEX_NB_SAMPLES_PER_FRAME;
#else
    int i;
    for (i=0; i<SPEEX_NB_SAMPLES_PER_FRAME; i++)
      encoder->pool[i] = (float)*pcm++;
    speex_encode(encoder->state, encoder->pool, &encoder->bits);
#endif
  }
    
  buffer += speex_bits_write(&encoder->bits, buffer, AUDIO_BUFFER_SIZE);

  return buffer - (char*)out_buf;
}

int SpeexNB_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec )
{
  SpeexState* ss;
  OneWay *decoder;
  short* pcm = (short*) out_buf;
  int frames_out  = 0;
    
  ss = (SpeexState*) h_codec;
  if (!ss || channels!=1 || rate!=8000)
    return -1;
    
  /* Decoder init */
  if (!(decoder=ss->decoder)) {
    decoder = ss->decoder = (OneWay*) calloc(1, sizeof(OneWay));
    if (!decoder)
      return -1;
    decoder->state = speex_decoder_init(&speex_nb_mode);
    speex_decoder_ctl(decoder->state, SPEEX_SET_ENH, &ss->perceptual);
    speex_bits_init(&decoder->bits);
  }
    
  speex_bits_read_from(&decoder->bits, (char*)in_buf, size);
    
  /* We don't know where frame boundaries are,
     but the minimum frame size is 43 */
  while (speex_bits_remaining(&decoder->bits)>40) {
    int ret;
	
#ifdef NOFPU
    ret = speex_decode_int(decoder->state, &decoder->bits, pcm);
    pcm+=SPEEX_NB_SAMPLES_PER_FRAME;
#else
    int i;
    ret = speex_decode(decoder->state, &decoder->bits, decoder->pool);
    for (i=0; i<SPEEX_NB_SAMPLES_PER_FRAME; i++) {
      *pcm++ = (short) decoder->pool[i];
    }
#endif    
	
    if (ret==-2) {
      ERROR("while calling speex_decode\n");
      return -1;
    }
	
    if (ret==-1) break;
	
    frames_out++;  
  }
    
  return frames_out*SPEEX_NB_SAMPLES_PER_FRAME*sizeof(short);
}
