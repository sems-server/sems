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
#include "libisac/isac.h"
#include "../../log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define iSAC_SAMPLE_RATE 16000
#define iSAC_FRAME_MS 30

int Pcm16_2_iSAC( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec );
int iSAC_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec );

static long iSAC_create(const char* format_parameters, 
			amci_codec_fmt_info_t* format_description);
static void iSAC_destroy(long handle);

static unsigned int iSAC_bytes2samples(long, unsigned int);
static unsigned int iSAC_samples2bytes(long, unsigned int);

BEGIN_EXPORTS("isac", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

BEGIN_CODECS
#if SYSTEM_SAMPLECLOCK_RATE >= 16000
CODEC(CODEC_iSAC_WB, Pcm16_2_iSAC, iSAC_2_Pcm16, AMCI_NO_CODEC_PLC,
      iSAC_create, iSAC_destroy, 
      iSAC_bytes2samples, iSAC_samples2bytes)
#endif
END_CODECS
  
BEGIN_PAYLOADS
#if SYSTEM_SAMPLECLOCK_RATE >=16000
PAYLOAD(-1, "isac", iSAC_SAMPLE_RATE, iSAC_SAMPLE_RATE, 1, 
	CODEC_iSAC_WB, AMCI_PT_AUDIO_FRAME)
#endif
END_PAYLOADS
  
BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS

static long iSAC_create(const char* format_parameters, 
			amci_codec_fmt_info_t* format_description)
{
  ISACStruct *iSAC_st=NULL;
  int err = WebRtcIsac_Create(&iSAC_st);
  if (err < 0) 
    return 0;

  WebRtcIsac_SetEncSampRate(iSAC_st, kIsacWideband);
  WebRtcIsac_SetDecSampRate(iSAC_st, kIsacWideband);

  if (WebRtcIsac_EncoderInit(iSAC_st, 0) < 0) {
    ERROR("Could not init ISAC encoder\n");
    WebRtcIsac_Free(iSAC_st);
    return 0;
  }

  if (WebRtcIsac_DecoderInit(iSAC_st) < 0) {
    ERROR("Could not init ISAC decoder\n");
    WebRtcIsac_Free(iSAC_st);
    return 0;
  }

  format_description[0].id = AMCI_FMT_FRAME_SIZE;
  format_description[0].value = iSAC_FRAME_MS * iSAC_SAMPLE_RATE / 1000;
  DBG("set AMCI_FMT_FRAME_SIZE to %d\n", format_description[0].value);
    
  format_description[1].id = 0;

  return (long)iSAC_st;
}

static void iSAC_destroy(long handle)
{
  WebRtcIsac_Free((ISACStruct*)handle);
}

static unsigned int iSAC_bytes2samples(long h_codec, unsigned int num_bytes) {
  return num_bytes / 2;
}

static unsigned int iSAC_samples2bytes(long h_codec, unsigned int num_samples) {
  return num_samples * 2;
}

int Pcm16_2_iSAC( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  ISACStruct* st = (ISACStruct*)h_codec;
  const WebRtc_Word16* in = (WebRtc_Word16*)in_buf;
  WebRtc_Word16 len=0;

  DBG("starting ISAC encode\n");
  while((len == 0) && (((unsigned char*)in) - in_buf < size)) {
    len = WebRtcIsac_Encode( st, in, (WebRtc_Word16*)out_buf );
    in += 10 /* ms */ * iSAC_SAMPLE_RATE / 1000;
    DBG("encoding ISAC frame... (len = %i ; size = %i)\n",len,size);
  }
  if( len < 0 ) {
    ERROR( "WebRtcIsac_Encode() returned %d (size=%u)\n", len, size );
    return -1;
  }

  return len;
}

int iSAC_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  ISACStruct* st = (ISACStruct*)h_codec;
  WebRtc_Word16 samples, speechType, *outPtr = (WebRtc_Word16*)out_buf;

  samples = WebRtcIsac_Decode( st, (void *)in_buf, size,
			       outPtr, &speechType );
  if( samples < 0 ) {
    ERROR( "WebRtcIsac_Decode returned %d\n", samples );
    return -1;
  }

  return samples<<1;
}
