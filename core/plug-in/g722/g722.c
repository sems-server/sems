/*
  This is a simple interface to the spandsp's g722 implementation.
  This uses the 8khz compatibility mode - audio is encodec and decoded 
  in 8khz.

  Copyright (C) 2008 iptego GmbH 

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

#include <spandsp.h>
#include "../../log.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char uint8_t;
typedef signed short int16_t;

int Pcm16_2_G722NB( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );
int G722NB_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec );

long G722NB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
void G722NB_destroy(long handle);

static unsigned int G722NB_bytes2samples(long, unsigned int);
static unsigned int G722NB_samples2bytes(long, unsigned int);

BEGIN_EXPORTS("g722", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

  BEGIN_CODECS
    CODEC(CODEC_G722_NB, Pcm16_2_G722NB, G722NB_2_Pcm16, AMCI_NO_CODEC_PLC,
          G722NB_create, G722NB_destroy,
          G722NB_bytes2samples, G722NB_samples2bytes)
  END_CODECS
  
  BEGIN_PAYLOADS
#if SYSTEM_SAMPLECLOCK_RATE >= 16000
    PAYLOAD(9, "g722", 16000, 8000, 1, CODEC_G722_NB, AMCI_PT_AUDIO_FRAME)
#endif
  END_PAYLOADS
  
  BEGIN_FILE_FORMATS
  END_FILE_FORMATS

END_EXPORTS

typedef struct {
  g722_encode_state_t* encode_state;
  g722_decode_state_t* decode_state;    
} G722State;


long G722NB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  G722State* gs;
        
  gs = (G722State*) calloc(1, sizeof(G722State));
  if (!gs) {
    ERROR("error allocating memory for G722 codec state\n");
    return 0;
  }

  gs->encode_state = g722_encode_init(NULL,
				      64000, 0 /* G722_SAMPLE_RATE_8000 */);

  if (!gs->encode_state) {
    ERROR("error initializing G722 encoder\n");
    free(gs);
    return 0;
  }

  gs->decode_state = g722_decode_init(NULL, 
				      64000, 0 /* G722_SAMPLE_RATE_8000 */);
  if (!gs->decode_state) {
    ERROR("error initializing G722 decoder\n");
    free(gs->encode_state);
    free(gs);
    return 0;
  }

  return (long)gs;
}

void G722NB_destroy(long handle)
{
  G722State* gs;

  if (!handle)
    return;

  gs = (G722State*) handle;
  
  if (gs->encode_state) {
    g722_encode_release(gs->encode_state);

    /* 20080616 is 0.0.5 release date
       a bit silly, but looks like spandsp does not have
       version in version.h */
#if SPANDSP_RELEASE_DATE > 20080616
    g722_encode_free(gs->encode_state);
#endif
  }

  if (gs->decode_state) {
    g722_decode_release(gs->decode_state);
    /* 20080616 is 0.0.5 release date
       a bit silly, but looks like spandsp does not have
       version in version.h */
#if SPANDSP_RELEASE_DATE > 20080616
    g722_decode_free(gs->decode_state);
#endif
  }

  free(gs);
}

static unsigned int G722NB_bytes2samples(long h_codec, unsigned int num_bytes) {
  return  num_bytes * 2;
}

static unsigned int G722NB_samples2bytes(long h_codec, unsigned int num_samples) {
  return num_samples / 2;
}


int Pcm16_2_G722NB( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		    unsigned int channels, unsigned int rate, long h_codec )
{
  G722State* gs;

  if (channels!=1) {
    ERROR("only supports 1 channel\n");
    return 0;
  }

  if (rate != 16000 /* 8000 */) {
    ERROR("only supports NB (8khz)\n");
    return 0;
  }

  gs = (G722State*) h_codec;

  return g722_encode(gs->encode_state, out_buf, (signed short*)in_buf, size >> 1);
}

int G722NB_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec )
{

  G722State* gs;

  if (channels!=1) {
    ERROR("only supports 1 channel\n");
    return 0;
  }

  if (rate != 16000 /* 8000 */) {
    ERROR("only supports NB (8khz)\n");
    return 0;
  }

  gs = (G722State*) h_codec;

  return g722_decode(gs->decode_state, (signed short*)out_buf, in_buf, size) << 1;
}
