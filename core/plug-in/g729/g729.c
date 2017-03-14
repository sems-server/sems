/*

  This file is part of SEMS, a free SIP media server.

 Copyright (c) 2007, Vadim Lebedev
 Copyright (c) 2010, Stefan Sayer
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the <organization> nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "../../log.h"
#include <stdio.h>
#include "amci.h"
#include "codecs.h"

#include <stdlib.h>
#include <bcg729/decoder.h>
#include <bcg729/encoder.h>

static int pcm16_2_g729(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
			unsigned int channels, unsigned int rate, long h_codec );

static int g729_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec );

static long g729_create(const char* format_parameters, const char** format_parameters_out,
			amci_codec_fmt_info_t** format_description);
static void g729_destroy(long h_codec);

static unsigned int g729_bytes2samples(long, unsigned int);
static unsigned int g729_samples2bytes(long, unsigned int);

static amci_codec_fmt_info_t g729_fmt_description[] =  { { AMCI_FMT_FRAME_LENGTH, 20 },
						       { AMCI_FMT_FRAME_SIZE, 160 },
						       { AMCI_FMT_ENCODED_FRAME_SIZE, 33 },
						       { 0, 0 }
};

#define G729_PAYLOAD_ID          18
#define G729_BYTES_PER_FRAME     10
#define G729_SAMPLES_PER_FRAME   10
#define PCM_BYTES_PER_FRAME      160

BEGIN_EXPORTS( "g729", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

    BEGIN_CODECS
    CODEC( CODEC_G729, pcm16_2_g729, g729_2_pcm16, AMCI_NO_CODEC_PLC,
	   (amci_codec_init_t)g729_create, (amci_codec_destroy_t)g729_destroy,
            g729_bytes2samples, g729_samples2bytes
          )
    END_CODECS

    BEGIN_PAYLOADS
    PAYLOAD( G729_PAYLOAD_ID, "G729", 8000, 8000, 1, CODEC_G729, AMCI_PT_AUDIO_FRAME )
    END_PAYLOADS

    BEGIN_FILE_FORMATS
    END_FILE_FORMATS

END_EXPORTS

struct G729_codec
{
  bcg729DecoderChannelContextStruct* dec;
  bcg729EncoderChannelContextStruct* enc;
};


long g729_create(const char* format_parameters, const char** format_parameters_out,
			amci_codec_fmt_info_t** format_description)
{
    struct G729_codec *codec;

    codec = calloc(sizeof(struct G729_codec), 1);
    codec->enc = initBcg729EncoderChannel(0 /* no VAT/DTX detection */);
    codec->dec = initBcg729DecoderChannel();

    *format_description = g729_fmt_description;

    return (long) codec;
}


static void
g729_destroy(long h_codec)
{
    struct G729_codec *codec = (struct G729_codec *) h_codec;

    if (!h_codec)
      return;

    closeBcg729DecoderChannel(codec->dec);
    closeBcg729EncoderChannel(codec->enc);
    free(codec);
}



static int pcm16_2_g729(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
			unsigned int channels, unsigned int rate, long h_codec )
{
    struct G729_codec *codec = (struct G729_codec *) h_codec;
    unsigned int out_size = 0;

    if (!h_codec)
      return -1;

    if (size % PCM_BYTES_PER_FRAME != 0){
       ERROR("pcm16_2_g729: number of blocks should be integral (block size = %u)\n", PCM_BYTES_PER_FRAME);
       return -1;
    }

    while(size >= PCM_BYTES_PER_FRAME){
        /* Encode a frame  */
        uint8_t olen;
        bcg729Encoder(codec->enc, (int16_t*)in_buf, (uint8_t*)out_buf, &olen);

        size -= PCM_BYTES_PER_FRAME;
        in_buf += PCM_BYTES_PER_FRAME;

        out_buf += olen;
        out_size += olen;
    }

    return out_size;
}

static int g729_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		       unsigned int channels, unsigned int rate, long h_codec )
{
    struct G729_codec *codec = (struct G729_codec *) h_codec;
    unsigned int out_size = 0;

    if (!h_codec)
      return -1;

    if (size % G729_BYTES_PER_FRAME != 0){
       ERROR("g729_2_pcm16: number of blocks should be integral (block size = %u)\n", G729_BYTES_PER_FRAME);
       return -1;
    }

    while(size >= G729_BYTES_PER_FRAME){
        /* Decode a frame  */
        bcg729Decoder(codec->dec, in_buf, G729_BYTES_PER_FRAME, 0 /* no erasure */, 0 /* not SID */, 0 /* not RFC3389 */, (int16_t*)out_buf);

        size -= G729_BYTES_PER_FRAME;
        in_buf += G729_BYTES_PER_FRAME;

        out_buf += PCM_BYTES_PER_FRAME;
        out_size += PCM_BYTES_PER_FRAME;
      }

    return out_size;
}

static unsigned int g729_bytes2samples(long h_codec, unsigned int num_bytes) {
  return  (G729_SAMPLES_PER_FRAME * num_bytes) / G729_BYTES_PER_FRAME;
}

static unsigned int g729_samples2bytes(long h_codec, unsigned int num_samples) {
  return G729_BYTES_PER_FRAME * num_samples /  G729_SAMPLES_PER_FRAME;
}
