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
static long opus_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
static void opus_destroy(long h_inst);

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

BEGIN_EXPORTS( "opus" , AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

  BEGIN_CODECS
    CODEC( CODEC_OPUS, pcm16_2_opus, opus_2_pcm16, opus_plc,
           opus_create, 
           opus_destroy,
           NULL, NULL )
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

long opus_create(const char* format_parameters, amci_codec_fmt_info_t* format_description) {
  opus_state_t* codec_inst;
  int error;
 
  if (format_parameters) {
    DBG("OPUS params: >>%s<<.\n", format_parameters);
  } 

  format_description[0].id = AMCI_FMT_FRAME_LENGTH ;
  format_description[0].value = 20;
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = 20 * _OPUS_RATE / 1000;
  format_description[2].id = 0;
    
  codec_inst = (opus_state_t*)malloc(sizeof(opus_state_t));

  if (!codec_inst) 
    return -1;

  codec_inst->opus_enc = opus_encoder_create(_OPUS_RATE,1,_OPUS_APPLICATION_,&error);
  if (error) {
    DBG("OPUS: error %d while creating encoder state.\n", error);
    return -1;
  }

  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_FORCE_CHANNELS(1));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_MAX_BANDWIDTH(_OPUS_MAX_BANDWIDTH_));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_PACKET_LOSS_PERC(_OPUS_PKT_LOSS_PCT_));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_COMPLEXITY(_OPUS_COMPLEXITY_));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_INBAND_FEC(_OPUS_INBAND_FEC_));
  opus_encoder_ctl(codec_inst->opus_enc, OPUS_SET_DTX(_OPUS_DTX_));

  codec_inst->opus_dec = opus_decoder_create(_OPUS_RATE,1,&error);
  if (error) {
    DBG("OPUS: error %d while creating decoder state.\n", error);
    opus_encoder_destroy(codec_inst->opus_enc);
    return -1;
  }

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

