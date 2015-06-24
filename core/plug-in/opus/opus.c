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
/**
 * @file plug-in/opus/opus.c
 * OPUS support 
 * This plug-in imports the OPUS Codec. 
 *
 * See http://www.opus-codec.org/ . 
 * Features: <ul>
 *           <li>OPUS codec/payload/subtype
 *           <li>OPUS file format
 *           </ul>
 *
 */

#include <opus/opus.h>
#include <stdio.h>

#define _OPUS_APPLICATION_ OPUS_APPLICATION_VOIP
/* Allowed values:
OPUS_APPLICATION_VOIP                  Process signal for improved speech intelligibility.
OPUS_APPLICATION_AUDIO                 Favor faithfulness to the original input.
OPUS_APPLICATION_RESTRICTED_LOWDELAY   Configure the minimum possible coding delay by disabling certain modes of operation.*/

#define _OPUS_MAX_BANDWIDTH_ OPUS_BANDWIDTH_FULLBAND
/* Allowed values:
OPUS_BANDWIDTH_NARROWBAND    -  4 kHz passband
OPUS_BANDWIDTH_MEDIUMBAND    -  6 kHz passband
OPUS_BANDWIDTH_WIDEBAND      -  8 kHz passband
OPUS_BANDWIDTH_SUPERWIDEBAND - 12 kHz passband
OPUS_BANDWIDTH_FULLBAND      - 20 kHz passband */

#define _OPUS_PKT_LOSS_PCT_ 5
/* Allowed values: 0 - 100 */

#define _OPUS_COMPLEXITY_ 10
/* Allowed values: 0 - 10, where 10 is highest computational complexity */

#define _OPUS_INBAND_FEC_ 1
/* Forward error correction.
Allowed values: 0 - 1 */

#define _OPUS_DTX_ 0
/* Discontinued transmission
Allowed values: 0 - 1 */

static int opus_2_pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static int opus_plc( unsigned char* out_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );

static int pcm16_2_opus( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );
static long opus_create(const char* format_parameters, const char** format_parameters_out,
			  amci_codec_fmt_info_t** format_description);
static void opus_destroy(long h_inst);

static int opus_negotiate_fmt(int is_offer, const char* params_in, char* params_out, unsigned int params_out_len);

static int opus_load(const char* ModConfigPath);

#if SYSTEM_SAMPLECLOCK_RATE >= 48000
#define _OPUS_RATE 48000
#elif SYSTEM_SAMPLECLOCK_RATE >= 24000
#define _OPUS_RATE 24000
#elif SYSTEM_SAMPLECLOCK_RATE >= 12000
#define _OPUS_RATE 12000
#elif SYSTEM_SAMPLECLOCK_RATE >=  8000
#define _OPUS_RATE  8000
#else
#error Minimal sample rate for OPUS codec is 8000.
#endif

static amci_codec_fmt_info_t opus_fmt_description[] = { {AMCI_FMT_FRAME_LENGTH, 20},
						       {AMCI_FMT_FRAME_SIZE, 20 * _OPUS_RATE / 1000}, {0,0}};

BEGIN_EXPORTS( "opus" , opus_load, AMCI_NO_MODULEDESTROY )

  BEGIN_CODECS
    CODEC_WITH_FMT( CODEC_OPUS, pcm16_2_opus, opus_2_pcm16, opus_plc,
           opus_create, 
           opus_destroy,
           NULL, NULL ,
	   opus_negotiate_fmt)
  END_CODECS
    
  BEGIN_PAYLOADS
    PAYLOAD( -1, "opus", _OPUS_RATE, 48000, 2, CODEC_OPUS, AMCI_PT_AUDIO_FRAME )
  END_PAYLOADS

  BEGIN_FILE_FORMATS
  END_FILE_FORMATS

END_EXPORTS

typedef struct {
  OpusEncoder* opus_enc;
  OpusDecoder* opus_dec;
} opus_state_t;

/* e.g. "maxplaybackrate=8000; stereo=0; useinbandfec=1" */
char default_format_parameters[80];

int opus_load(const char* ModConfigPath) {
  default_format_parameters[0]='\0';
  char conf_file[256];
  if (NULL != ModConfigPath) {
    sprintf(conf_file, "%sopus.conf",ModConfigPath); 
    FILE* fp = fopen(conf_file, "rt");
    if (fp) {
      char line[80];
      while(fgets(line, 80, fp) != NULL) {
	if (!line[0] ||line[0]=='#')
	  continue;
	strcpy(default_format_parameters, line);
	break;
      }
      DBG("initialized default format parameters as '%s'\n", default_format_parameters);
      fclose(fp);
    }
  }
  DBG("OPUS: initialized\n");
  return 0;
}

int opus_negotiate_fmt(int is_offer, const char* params_in, char* params_out, unsigned int params_out_len) {
 // todo: properly negotiating features
  strncpy(params_out, default_format_parameters, params_out_len);
  return 0;
}

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
  while (*input && (*input==' ' || *input==';' || *input=='"'))
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

void decode_format_parameters(const char* format_parameters, unsigned int* maxbandwidth, int* useinbandfec, int* stereo) {
  if (format_parameters && strlen(format_parameters)<=BLEN){
	
    char buffer2[BLEN+1];
    char *buffer = buffer2;

    strcpy(buffer, format_parameters);

    while (*buffer) {
      char *param_value;

      /* maxplaybackrate */
      buffer=read_param(buffer, "maxplaybackrate", &param_value);
      if (param_value) {
	*maxbandwidth = atoi(param_value);
	if (!*maxbandwidth) {
	  *maxbandwidth = _OPUS_RATE;
	  DBG("wrong maxbandwidth value '%s'\n", param_value);
	}
	continue;
      }

      /* stereo */
      buffer=read_param(buffer, "stereo", &param_value);
      if (param_value) {
	if (*param_value == '1')
	  *stereo = 1;
	else
	  *stereo = 0;

	continue;
      }

      /* useinbandfec */
      buffer=read_param(buffer, "useinbandfec", &param_value);
      if (param_value) {
	if (*param_value == '1')
	  *useinbandfec = 1;
	else
	  *useinbandfec = 0;

	continue;
      }

      /* Unknown parameter */
      if (*buffer) {
	param_value = buffer;
	while (*buffer && *buffer!=';')
	  buffer++;
		
	if (*buffer)
	  *buffer++ = 0;

	DBG("OPUS: SDP parameter fmtp: %s ignored in creating encoder.\n", param_value);
      }
    }
  }
}


long opus_create(const char* format_parameters, const char** format_parameters_out,
		 amci_codec_fmt_info_t** format_description) {
  opus_state_t* codec_inst;
  int error;

  unsigned int maxbandwidth = _OPUS_RATE;
  int useinbandfec = _OPUS_INBAND_FEC_;
  int stereo = 0;

  if (format_parameters) {
    DBG("\n\n\n");
    DBG("OPUS params: >>%s<<.\n", format_parameters);
    DBG("\n\n\n");

    decode_format_parameters(format_parameters, &maxbandwidth, &useinbandfec, &stereo);
  } 
    
  codec_inst = (opus_state_t*)malloc(sizeof(opus_state_t));

  if (!codec_inst) 
    return -1;

  DBG("OPUS: creating encoder with maxbandwidth=%u, stereo=%s, useinbandfec=%s\n",
      maxbandwidth, stereo?"true":"false", useinbandfec?"true":"false");

  codec_inst->opus_enc = opus_encoder_create(_OPUS_RATE,1,_OPUS_APPLICATION_,&error);
  if (error) {
    DBG("OPUS: error %d while creating encoder state.\n", error);
    return -1;
  }

  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_FORCE_CHANNELS(stereo ? 2:1));

  unsigned int opus_set_bw = _OPUS_RATE;
  if (maxbandwidth <= 8000) {
    opus_set_bw = OPUS_BANDWIDTH_NARROWBAND;
  } else if (maxbandwidth <= 12000) {
    opus_set_bw = OPUS_BANDWIDTH_MEDIUMBAND;
  } else if (maxbandwidth <= 16000) {
    opus_set_bw = OPUS_BANDWIDTH_WIDEBAND;
  } else if (maxbandwidth <= 24000) {
    opus_set_bw = OPUS_BANDWIDTH_SUPERWIDEBAND;
  } else {
    opus_set_bw = OPUS_BANDWIDTH_FULLBAND;
  }
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_MAX_BANDWIDTH(opus_set_bw));

  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_PACKET_LOSS_PERC(_OPUS_PKT_LOSS_PCT_));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_COMPLEXITY(_OPUS_COMPLEXITY_));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_INBAND_FEC(useinbandfec ? 1:0));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_DTX(_OPUS_DTX_));

  codec_inst->opus_dec = opus_decoder_create(_OPUS_RATE,1,&error);
  if (error) {
    DBG("OPUS: error %d while creating decoder state.\n", error);
    opus_encoder_destroy(codec_inst->opus_enc);
    return -1;
  }

  *format_description = opus_fmt_description;

  return (long)codec_inst;
}

void opus_destroy(long h_inst) {
  opus_state_t* codec_inst;

  if (h_inst) {
    codec_inst = (opus_state_t*)h_inst;
    opus_encoder_destroy(codec_inst->opus_enc);
    opus_decoder_destroy(codec_inst->opus_dec);
    free(codec_inst);
  }
}

int pcm16_2_opus( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  opus_state_t* codec_inst;
  int res;

  if (!h_codec){
    ERROR("opus codec not initialized.\n");
    return 0;
  }
  codec_inst = (opus_state_t*)h_codec;

  res = opus_encode(codec_inst->opus_enc, (opus_int16*)in_buf, size/2/channels, out_buf, AUDIO_BUFFER_SIZE);
  /* returns bytes in encoded frame */

  /* DBG ("OPUS encode: size: %d, chan: %d, rate: %d, result %d.\n", size, channels, rate, res); */
  return res;
}

static int opus_2_pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec )
{
  opus_state_t* codec_inst;
  int res;

  if (!h_codec){
    ERROR("opus codec not initialized.\n");
    return 0;
  }
  codec_inst = (opus_state_t*)h_codec;

  if (0<(res = opus_decode(codec_inst->opus_dec, in_buf, size, (opus_int16*)out_buf, AUDIO_BUFFER_SIZE/2, 0))) {
    /* returns samples in encoded frame */
    res*=2;
  }

  /* DBG ("OPUS decode: size: %d, chan: %d, rate: %d, result %d.\n", size, channels, rate, res); */

  return res;

}

static int opus_plc( unsigned char* out_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec )
{
  opus_state_t* codec_inst;
  int res;

  if (!h_codec){
    ERROR("opus codec not initialized.\n");
    return 0;
  }
  codec_inst = (opus_state_t*)h_codec;

  if (size/channels > AUDIO_BUFFER_SIZE) {
    /* DBG("OPUS plc: size %d, chan %d exceeds buffer size %d.\n", size, channels, AUDIO_BUFFER_SIZE); */
    return 0;
  }

  if (0<(res = opus_decode(codec_inst->opus_dec, NULL, 0, (opus_int16*)out_buf, size/2/channels, 0))) {
    /* returns samples in encoded frame */
    res*=2;
  }

  /* DBG ("OPUS plc: size: %d, chan: %d, rate: %d, result %d.\n", size, channels, rate, res); */

  return res;
}

