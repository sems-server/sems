/*
  This is a simple interface to the SILK SDK for SEMS.
  Copyright (C) 2012 Raphael Coeffic

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
#include "SKP_Silk_SDK_API.h"
#include "../../log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// TODO:
//  - use integrated PLC (without FEC)
//  - set 'useinbandfec=0' in SDP


int Pcm16_2_SILK( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec );
int SILK_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec );

long SILK_NB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long SILK_MB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long SILK_WB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
long SILK_UB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
void SILK_destroy(long handle);

static unsigned int SILK_bytes2samples(long, unsigned int);
static unsigned int SILK_samples2bytes(long, unsigned int);

BEGIN_EXPORTS("silk", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

BEGIN_CODECS
#if SYSTEM_SAMPLECLOCK_RATE >=24000
CODEC(CODEC_SILK_UB, Pcm16_2_SILK, SILK_2_Pcm16, AMCI_NO_CODEC_PLC, 
      SILK_UB_create, SILK_destroy, 
      SILK_bytes2samples, SILK_samples2bytes)
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=16000
CODEC(CODEC_SILK_WB, Pcm16_2_SILK, SILK_2_Pcm16, AMCI_NO_CODEC_PLC, 
      SILK_WB_create, SILK_destroy, 
      SILK_bytes2samples, SILK_samples2bytes)
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=12000
CODEC(CODEC_SILK_MB, Pcm16_2_SILK, SILK_2_Pcm16, AMCI_NO_CODEC_PLC, 
      SILK_MB_create, SILK_destroy, 
      SILK_bytes2samples, SILK_samples2bytes)
#endif
CODEC(CODEC_SILK_NB, Pcm16_2_SILK, SILK_2_Pcm16, AMCI_NO_CODEC_PLC, 
      SILK_NB_create, SILK_destroy, 
      SILK_bytes2samples, SILK_samples2bytes)
END_CODECS
  
BEGIN_PAYLOADS
#if SYSTEM_SAMPLECLOCK_RATE >=24000
PAYLOAD(-1, "SILK", 24000, 24000, 1, CODEC_SILK_UB, AMCI_PT_AUDIO_FRAME)
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=16000
PAYLOAD(-1, "SILK", 16000, 16000, 1, CODEC_SILK_WB, AMCI_PT_AUDIO_FRAME)
#endif
#if SYSTEM_SAMPLECLOCK_RATE >=12000
PAYLOAD(-1, "SILK", 12000, 12000, 1, CODEC_SILK_MB, AMCI_PT_AUDIO_FRAME)
#endif
PAYLOAD(-1, "SILK", 8000, 8000, 1, CODEC_SILK_NB, AMCI_PT_AUDIO_FRAME)
END_PAYLOADS
  
BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS

typedef struct
{
  SKP_SILK_SDK_EncControlStruct encControl;
  void* psEnc;

  SKP_SILK_SDK_DecControlStruct decControl;
  void* psDec;
} SILK_state;


static int create_SILK_encoder(SILK_state* st, 
			       unsigned int rtp_Hz,
			       unsigned int avg_bit_rate)
{
  SKP_int32 ret,encSizeBytes;

  /* Create Encoder */
  ret = SKP_Silk_SDK_Get_Encoder_Size( &encSizeBytes );
  if( ret ) {
    ERROR( "SKP_Silk_create_encoder returned %d", ret );
    return ret;
  }

  st->psEnc = malloc( encSizeBytes );
  if(st->psEnc == NULL) {
    ERROR( "could not allocate SILK encoder state" );
    return -1;
  }
  
  /* Reset Encoder */
  ret = SKP_Silk_SDK_InitEncoder( st->psEnc, &st->encControl );
  if( ret ) {
    ERROR( "SKP_Silk_SDK_InitEncoder returned %d", ret );
    return ret;
  }
  
  /* Set Encoder parameters */
  st->encControl.API_sampleRate        = rtp_Hz;
  st->encControl.maxInternalSampleRate = rtp_Hz;
  st->encControl.packetSize            = ( 20 * rtp_Hz ) / 1000;
  st->encControl.packetLossPercentage  = 0;
  st->encControl.useInBandFEC          = 0;
  st->encControl.useDTX                = 0;
  st->encControl.complexity            = 2;
  st->encControl.bitRate               = avg_bit_rate;
  
  return 0;
}

static int create_SILK_decoder(SILK_state* st, 
			       unsigned int rtp_Hz)
{
  SKP_int32 ret, decSizeBytes;

  /* Set the samplingrate that is requested for the output */
  st->decControl.API_sampleRate = rtp_Hz;

  /* Initialize to one frame per packet, for proper concealment before first packet arrives */
  st->decControl.framesPerPacket = 1;

  /* Create decoder */
  ret = SKP_Silk_SDK_Get_Decoder_Size( &decSizeBytes );
  if( ret ) {
    ERROR( "SKP_Silk_SDK_Get_Decoder_Size returned %d", ret );
    return ret;
  }

  st->psDec = malloc( decSizeBytes );
  if(st->psDec == NULL) {
    ERROR( "could not allocate SILK decoder state" );
    return -1;
  }

  /* Reset decoder */
  ret = SKP_Silk_SDK_InitDecoder( st->psDec );
  if( ret ) {
    ERROR( "SKP_Silk_InitDecoder returned %d", ret );
    return ret;
  }

  return 0;
}

static long SILK_create(unsigned int rtp_Hz,
			unsigned int avg_bit_rate,
			const char* format_parameters, 
			amci_codec_fmt_info_t* format_description)
{
  SILK_state* st = malloc(sizeof(SILK_state));
  if(st == NULL) {
    ERROR("could not allocate SILK state\n");
    return 0;
  }

  if(create_SILK_encoder(st,rtp_Hz,avg_bit_rate))
    goto error;

  if(create_SILK_decoder(st,rtp_Hz))
    goto error;

  return (long)st;

 error:
  if(st->psEnc) free(st->psEnc);
  if(st->psDec) free(st->psDec);
  free(st);

  return 0;
}

long SILK_NB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return SILK_create(8000,20000,format_parameters,format_description);
}

long SILK_MB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return SILK_create(12000,25000,format_parameters,format_description);
}

long SILK_WB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return SILK_create(16000,30000,format_parameters,format_description);
}

long SILK_UB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  return SILK_create(24000,40000,format_parameters,format_description);
}

void SILK_destroy(long handle)
{
  SILK_state* st = (SILK_state*)handle;
  if(st->psEnc) free(st->psEnc);
  if(st->psDec) free(st->psDec);
  free(st);  
}

static unsigned int SILK_bytes2samples(long h_codec, unsigned int num_bytes) {
  return num_bytes / 2;
}

static unsigned int SILK_samples2bytes(long h_codec, unsigned int num_samples) {
  return num_samples * 2;
}

int Pcm16_2_SILK( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  SILK_state* st = (SILK_state*)h_codec;
  SKP_int16 ret;

  /* max payload size */
  SKP_int16 nBytes = AUDIO_BUFFER_SIZE;

  /* Silk Encoder */
  ret = SKP_Silk_SDK_Encode( st->psEnc, &st->encControl, (SKP_int16*)in_buf, 
			     (SKP_int16)size/2, out_buf, &nBytes );
  if( ret ) {
    ERROR( "SKP_Silk_Encode returned %d (size=%u)\n", ret, size );
    return -1;
  }

  return nBytes;
}

int SILK_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  SILK_state* st = (SILK_state*)h_codec;
  SKP_int16 ret, len, *outPtr = (SKP_int16*)out_buf;

  do {
    /* Decode 20 ms */
    ret = SKP_Silk_SDK_Decode( st->psDec, &st->decControl, 0/*lost*/,
			       in_buf, size, outPtr, &len );
    if( ret ) {
      ERROR( "SKP_Silk_SDK_Decode returned %d\n", ret );
      return ret;
    }

    outPtr += len;

    if( (unsigned char*)outPtr - out_buf > AUDIO_BUFFER_SIZE ) {
      /* Oooops!!! buffer overflow !!! */
      ERROR("Buffer overflow (size=%li)\n",
	    (unsigned char*)outPtr - out_buf);
      return -1; // TODO
    }
    /* Until last 20 ms frame of packet has been decoded */
  } while( st->decControl.moreInternalDecoderFrames );

  return (unsigned char*)outPtr - out_buf;
}
