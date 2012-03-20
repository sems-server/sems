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

#include "amci.h"
#include "codecs.h"
#include "../../log.h"

#include <speex/speex.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Speex constants */
#define SPEEX_FRAME_MS			 20
#define SPEEX_NB_SAMPLES_PER_FRAME 	160
#define SPEEX_WB_SAMPLES_PER_FRAME 	320
#define SPEEX_UB_SAMPLES_PER_FRAME 	640

/* Default encoder settings */
#define NB_MODE		  5
#define FRAMES_PER_PACKET 1         /* or other implementations choke */
#define BYTES_PER_FRAME  (160 / 8)  /* depends on mode */

#define WB_MODE           6
#define WB_BYTES_PER_FRAME  (320 / 8)  /* depends on mode */

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

int Pcm16_2_Speex( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		   unsigned int channels, unsigned int rate, long h_codec );
int Speex_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		   unsigned int channels, unsigned int rate, long h_codec );

long speexNB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long speexWB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long speexUB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
void speex_destroy(long handle);

/* static unsigned int speex_bytes2samples(long, unsigned int); */
/* static unsigned int speex_samples2bytes(long, unsigned int); */

BEGIN_EXPORTS("speex", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

BEGIN_CODECS
#if SYSTEM_SAMPLECLOCK_RATE >=32000
CODEC(CODEC_SPEEX_UB, Pcm16_2_Speex, Speex_2_Pcm16, AMCI_NO_CODEC_PLC, 
      speexUB_create, speex_destroy, NULL, NULL)
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=16000
CODEC(CODEC_SPEEX_WB, Pcm16_2_Speex, Speex_2_Pcm16, AMCI_NO_CODEC_PLC, 
      speexWB_create, speex_destroy, NULL, NULL)
#endif
CODEC(CODEC_SPEEX_NB, Pcm16_2_Speex, Speex_2_Pcm16, AMCI_NO_CODEC_PLC, 
      speexNB_create, speex_destroy, NULL, NULL)
END_CODECS

BEGIN_PAYLOADS
#if SYSTEM_SAMPLECLOCK_RATE >=32000
PAYLOAD(-1, "speex", 32000, 32000, 1, CODEC_SPEEX_UB, AMCI_PT_AUDIO_FRAME)
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=16000
PAYLOAD(-1, "speex", 16000, 16000, 1, CODEC_SPEEX_WB, AMCI_PT_AUDIO_FRAME)
#endif
PAYLOAD(-1, "speex", 8000, 8000, 1, CODEC_SPEEX_NB, AMCI_PT_AUDIO_FRAME)
END_PAYLOADS
  
BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS

typedef struct {
  void *state;
  SpeexBits bits;
} OneWay;

typedef struct {
  OneWay encoder;
  OneWay decoder;

  /* Encoder settings */
  unsigned int frames_per_packet; /* in samples */
  unsigned int frame_size;

} SpeexState;

/*
  Search for a parameter assignement in input string.
  If it's not found *param_value is null, otherwise *param_value points to the
  right hand term.
  In both cases a pointer suitable for a new search is returned
*/
static char* read_param(char* input, const char *param, char** param_value)
{
  int param_size;

  /* Eat spaces and semi-colons */
  while (*input && *input==' ' && *input==';' && *input!='"')
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
  while (*input && *input!=' ' && *input!=';' && *input!='"')
    input++;
  if (*input=='"')
    {
      *param_value = *param_value+1; /* remove " */
      /* string will end after next: " */
      while (*input && *input!='"' && *input!='\r' && *input!='\n')
	input++;
      if (*input=='"')
	input--; /* remove " */
    }
  if (*input)
    *input++ = 0;
    
  return input;
}

#define BLEN 63
#if 0
void decode_format_parameters(const char* format_parameters, SpeexState* ss) {
  /* See draft-herlein-avt-rtp-speex-00.txt */
  /* TODO: change according to RFC 5574 */
  if (format_parameters && strlen(format_parameters)<=BLEN){
	
    char buffer2[BLEN+1];
    char *buffer = buffer2;

    strcpy(buffer, format_parameters);
	
    while (*buffer) {
      char *error=NULL;
      char *param_value;
	    
      /* Speex encoding mode */
      buffer=read_param(buffer, "mode", &param_value);
      if (param_value) {
	int mode;
	if (strcmp(param_value, "any")) {
	  mode = strtol(param_value, &error, 10);
	  if (error!=NULL && error!=param_value && mode>=0 && mode<=10)
		DBG("Using speex sub mode %d", mode);
	    ss->mode = mode;
	}
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
}
#endif

long speex_create(unsigned int sample_rate, 
		  const char* format_parameters, 
		  amci_codec_fmt_info_t* format_description)
{
  int speex_mode = 0, on=1, quality=0;
  SpeexState* ss=NULL;

  switch(sample_rate) {
  case 8000:
    speex_mode = SPEEX_MODEID_NB;
    quality = 6;
    break;
#if SYSTEM_SAMPLECLOCK_RATE >=16000
  case 16000:
    speex_mode = SPEEX_MODEID_WB;
    quality = 8;
    break;
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=32000
  case 32000:
    speex_mode = SPEEX_MODEID_UWB;
    quality = 8;
    break;
#endif
  default:
    ERROR("Unsupported sample rate for Speex codec (%u)\n", sample_rate);
    return 0;
  }

  ss = (SpeexState*)malloc(sizeof(SpeexState));
  if (!ss) {
    ERROR("Could not allocate SpeexState\n");
    return 0;
  }

  /* Note that
     1) SEMS ignore a=ptime: SDP parameter so we can choose the one we like
     2) Multiple frames in a packet don't need to be byte-aligned
  */
  ss->frames_per_packet = FRAMES_PER_PACKET;

  /* decode the format parameters */
  /* decode_format_parameters(format_parameters, ss); */

  speex_bits_init(&ss->encoder.bits);
  ss->encoder.state = speex_encoder_init(speex_lib_get_mode(speex_mode));
  speex_encoder_ctl(ss->encoder.state, SPEEX_SET_QUALITY, &quality);

  speex_bits_init(&ss->decoder.bits);
  ss->decoder.state = speex_decoder_init(speex_lib_get_mode(speex_mode));
  speex_decoder_ctl(ss->decoder.state, SPEEX_SET_ENH, &on);
    
  format_description[0].id = AMCI_FMT_FRAME_LENGTH;
  format_description[0].value = SPEEX_FRAME_MS * ss->frames_per_packet;
    
  ss->frame_size = SPEEX_FRAME_MS * (sample_rate / 1000);
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = ss->frame_size * ss->frames_per_packet;
    
  format_description[2].id = 0;

  DBG("set AMCI_FMT_FRAME_LENGTH to %d\n", format_description[0].value);
  DBG("set AMCI_FMT_FRAME_SIZE to %d\n", format_description[1].value);

  DBG("SpeexState %p inserted with %d frames per packet,\n", 
      ss, ss->frames_per_packet);

  return (long)ss;
}

long speexNB_create(const char* format_parameters, 
		    amci_codec_fmt_info_t* format_description)
{
  return speex_create(8000,format_parameters,format_description);
}

long speexWB_create(const char* format_parameters, 
		    amci_codec_fmt_info_t* format_description)
{
  return speex_create(16000,format_parameters,format_description);
}

long speexUB_create(const char* format_parameters, 
		    amci_codec_fmt_info_t* format_description)
{
  return speex_create(32000,format_parameters,format_description);
}

void speex_destroy(long handle)
{
  SpeexState* ss = (SpeexState*) handle;
    
  DBG("SpeexDestroy for handle %ld\n", handle);
    
  if (!ss)
    return;
    
  speex_encoder_destroy(ss->encoder.state);
  speex_bits_destroy(&ss->encoder.bits);
    
  speex_decoder_destroy(ss->decoder.state);
  speex_bits_destroy(&ss->decoder.bits);
    
  free(ss);
}

int Pcm16_2_Speex( unsigned char* out_buf, unsigned char* in_buf, 
		   unsigned int size,
		   unsigned int channels, unsigned int rate, long h_codec )
{
  SpeexState* ss;
  short* pcm = (short*) in_buf;
  char* buffer = (char*)out_buf;
  div_t blocks;
    
  ss = (SpeexState*) h_codec;
    
  if (!ss || channels!=1)
    return -1;

  blocks = div(size>>1, ss->frame_size);
  if (blocks.rem) {
    ERROR("Pcm16_2_Speex: not integral number of blocks %d.%d\n", 
	  blocks.quot, blocks.rem);
    return -1;
  }
    
  /* For each chunk of ss->frame_size bytes, encode a single frame */
  speex_bits_reset(&ss->encoder.bits);
  while (blocks.quot--) {
    speex_encode_int(ss->encoder.state, pcm, &ss->encoder.bits);
    pcm += ss->frame_size;
  }
    
  buffer += speex_bits_write(&ss->encoder.bits, buffer, AUDIO_BUFFER_SIZE);
  return buffer - (char*)out_buf;
}

int Speex_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, 
		   unsigned int size,
		   unsigned int channels, unsigned int rate, long h_codec )
{
  SpeexState* ss;
  short* pcm = (short*) out_buf;
  int frames_out  = 0;

  ss = (SpeexState*) h_codec;
  if (!ss || channels!=1)
    return -1;
    
  speex_bits_read_from(&ss->decoder.bits, (char*)in_buf, size);

  /* We don't know where frame boundaries are,
     but the minimum frame size is 43 */
  while (speex_bits_remaining(&ss->decoder.bits)>40) {
    int ret;
	
    ret = speex_decode_int(ss->decoder.state, &ss->decoder.bits, pcm);
    pcm+= ss->frame_size;
	
    if (ret==-2) {
      ERROR("while calling speex_decode\n");
      return -1;
    }
	
    if (ret==-1) break;
	
    frames_out++;  
  }
    
  return frames_out*ss->frame_size*sizeof(short);
}
