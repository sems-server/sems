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

#include <string.h>
#include <stdlib.h>

#include "amci.h"
#include "codecs.h"
#include <math.h>
typedef unsigned char uint8_t;
typedef signed short int16_t;

#include "spandsp/g722.h"
#include "../../log.h"


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
PAYLOAD(9, "g722", 8000, 1, CODEC_G722_NB, AMCI_PT_AUDIO_FRAME)
END_PAYLOADS
  
BEGIN_FILE_FORMATS
END_FILE_FORMATS

END_EXPORTS

typedef struct {
  g722_encode_state_t encode_state;
  g722_decode_state_t decode_state;    
} G722State;


long G722NB_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
  G722State* gs;
        
  gs = (G722State*) calloc(1, sizeof(G722State));
  if (!gs) {
    ERROR("error allocating memory for G722 codec state\n");
    return 0;
  }

  if (!g722_encode_init(&gs->encode_state, 
			64000, G722_SAMPLE_RATE_8000)) {
    ERROR("error initializing G722 encoder\n");
    free(gs);
    return 0;
  }

  if (!g722_decode_init(&gs->decode_state, 
			64000, G722_SAMPLE_RATE_8000)) {
    ERROR("error initializing G722 decoder\n");
    free(gs);
    return 0;
  }

  return (long)gs;
}

void G722NB_destroy(long handle)
{
}

static unsigned int G722NB_bytes2samples(long h_codec, unsigned int num_bytes) {
  return  num_bytes;
}

static unsigned int G722NB_samples2bytes(long h_codec, unsigned int num_samples) {
  return num_samples;
}


int Pcm16_2_G722NB( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		    unsigned int channels, unsigned int rate, long h_codec )
{
  G722State* gs;

  if (channels!=1) {
    ERROR("only supports 1 channel\n");
    return 0;
  }

  if (rate != 8000) {
    ERROR("only supports NB (8khz)\n");
    return 0;
  }

  gs = (G722State*) h_codec;

  return g722_encode(&gs->encode_state, out_buf, (signed short*)in_buf, size >> 1);
}

int G722NB_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		     unsigned int channels, unsigned int rate, long h_codec )
{

  G722State* gs;

  if (channels!=1) {
    ERROR("only supports 1 channel\n");
    return 0;
  }

  if (rate != 8000) {
    ERROR("only supports NB (8khz)\n");
    return 0;
  }

  gs = (G722State*) h_codec;

  return g722_decode(&gs->decode_state, (signed short*)out_buf, in_buf, size) << 1;
}
