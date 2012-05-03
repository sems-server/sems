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


 Notes:

 This is a wrapper for Intel IPP 0.6 G729 codec.

*/

#ifndef TEST
#include "../../log.h"
#include <stdio.h>
#include "amci.h"
#include "codecs.h"
#else
#include <stdio.h>
#define ERROR  printf
#endif


#include <stdlib.h>
#include <usc.h>


static int pcm16_2_g729(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
			unsigned int channels, unsigned int rate, long h_codec );

static int g729_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec );

static long g729_create(const char* format_parameters, amci_codec_fmt_info_t* format_description);
static void g729_destroy(long h_codec);

static unsigned int g729_bytes2samples(long, unsigned int);
static unsigned int g729_samples2bytes(long, unsigned int);

#define G729_PAYLOAD_ID          18
#define G729_BYTES_PER_FRAME     10
#define G729_SAMPLES_PER_FRAME   10

#ifndef TEST 
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

#endif

struct stream
{
  USC_Handle  handle;
  int nBanks;
  USC_MemBank*  banks;
};

struct G729_codec 
{
  struct  stream  dec, enc;
  USC_CodecInfo pInfo;  
};


extern USC_Fxns USC_G729I_Fxns;
USC_Fxns *fns = &USC_G729I_Fxns; 

static int 
stream_alloc(USC_CodecInfo *info, struct stream *md, const char *name)  
{
  
  int i;

  if (USC_NoError != fns->std.NumAlloc(&info->params, &md->nBanks))
    {
      ERROR("g729_stream_alloc: can't query memory reqirements for %s\n", name);
      return -1;
    }

  /* allocate memory for memory bank table */
  md->banks  =  (USC_MemBank*)malloc(sizeof(USC_MemBank)*md->nBanks);
  
  /* Query how big has to be each block */
  if (USC_NoError != fns->std.MemAlloc(&info->params, md->banks))
    {
      ERROR("g729_stream_alloc: can't query memory bank size for %s\n", name);
      return -1;
    }
 

    /* allocate memory for each block */
  for(i=0; i<md->nBanks; i++)
    {
        md->banks[i].pMem = (char*)malloc(md->banks[i].nbytes);
    }
   
  return 0;
}

static void
stream_free(struct stream *st)
{
  int i;

  for(i = 0; i < st->nBanks; i++)
    free(st->banks[i].pMem);

  free(st->banks);

}


static int
stream_create(USC_CodecInfo *info, struct stream *st, const char *name)
{
   if (stream_alloc(info, st, name))
      {
	return -1;
      }

    /* Create encoder instance */
    if(USC_NoError != fns->std.Init(&info->params, st->banks, &st->handle))
      {
	ERROR("g729_stream_create: can't intialize stream %s\n", name);
	stream_free(st);
	return -1;
      }

    return 0;
}


static void
stream_destroy(struct stream *st)
{
  stream_free(st);
}


long g729_create(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{
    USC_CodecInfo pInfo;  
    struct G729_codec *codec;


    if (USC_NoError != fns->std.GetInfo((USC_Handle)NULL, &pInfo))
      {
	ERROR("g729: Can't query codec info\n");
	return (0);
      }
  

    pInfo.params.direction = 0;             /* Direction: encode */
    pInfo.params.modes.vad = 0;         /* Suppress a silence compression */
    pInfo.params.law = 0;                    /* Linear PCM input */
    pInfo.params.modes.bitrate = 8000; /* pInfo.pRateTbl[pInfo.nRates-1].bitrate; */     /* Set highest bitrate */


    codec = calloc(sizeof(struct G729_codec), 1);

    if (stream_create(&pInfo, &codec->enc, "encoder"))
      {
	free(codec);
	return 0;
      }
     

    pInfo.params.direction = 1;             /* Direction:  decode */
  
    if (stream_create(&pInfo, &codec->dec, "decoder"))
      {
	stream_destroy(&codec->enc);
	free(codec);
	return 0;
      }

    codec->pInfo = pInfo;
    return (long) codec;
}


static void
g729_destroy(long h_codec)
{
    struct G729_codec *codec = (struct G729_codec *) h_codec;

    if (!h_codec)
      return;

    stream_destroy(&codec->dec);
    stream_destroy(&codec->enc);
    free(codec);
}



static int pcm16_2_g729(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
			unsigned int channels, unsigned int rate, long h_codec )
{
    div_t blocks;
    struct G729_codec *codec = (struct G729_codec *) h_codec;
    int out_size = 0;
    int err;

    if (!h_codec)
      return -1;
    
    blocks = div(size, codec->pInfo.params.framesize);

    if (blocks.rem)
      {
	ERROR("pcm16_2_G729: number of blocks should be integral (block size = %d)\n",
	      codec->pInfo.params.framesize);
	return -1;
      }

   

    while(size >= codec->pInfo.params.framesize) 
      {
        USC_PCMStream in;
        USC_Bitstream out;
   

        /* Set input stream params */
        in.bitrate = codec->pInfo.params.modes.bitrate;
        in.nbytes = size;
        in.pBuffer = (char*)in_buf;
        in.pcmType = codec->pInfo.params.pcmType;

        /* Set output buffer */
        out.pBuffer = (char*)out_buf;

        /* Encode a frame  */
	err = fns->Encode (codec->enc.handle, &in, &out);
        if (USC_NoError != err)
	  {
	    ERROR("pcm16_2_G729: error %d encoding\n", err);
	    return -1;
	  }
    
	size -= in.nbytes;
	in_buf += in.nbytes;

	out_buf += out.nbytes;
	out_size += out.nbytes;
      }

    return out_size;
}

static int g729_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec )
{
  /* div_t blocks; */
    unsigned int out_size = 0;
    int frameSize = 0;
    int x;
    struct G729_codec *codec = (struct G729_codec *) h_codec;
    int err;

    if (!h_codec)
      return -1;

    for(x = 0; x < size; x += frameSize) 
      {
        USC_PCMStream out;
        USC_Bitstream in;
	
	
	in.pBuffer = (char*)in_buf;
	in.nbytes = size;
	in.bitrate = codec->pInfo.params.modes.bitrate;
	in.frametype = 3;

	out.pcmType = codec->pInfo.params.pcmType;
	out.pBuffer = (char*)out_buf;


	err = fns->Decode (codec->dec.handle, &in, &out);
	if (USC_NoError != err)
	  {
	    ERROR("g729_2_pcm16: error %d decoding data\n", err);
	    break;
	  }

	in_buf += in.nbytes;
	frameSize = in.nbytes;

	out_buf += out.nbytes;
	out_size += out.nbytes;
      }

    return out_size;
}

static unsigned int g729_bytes2samples(long h_codec, unsigned int num_bytes) {
  return  (G729_SAMPLES_PER_FRAME * num_bytes) / G729_BYTES_PER_FRAME;
}

static unsigned int g729_samples2bytes(long h_codec, unsigned int num_samples) {
  return G729_BYTES_PER_FRAME * num_samples /  G729_SAMPLES_PER_FRAME; 
}

#ifdef TEST
#define N 160

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int inpcm[N];
unsigned char outg729[1024];
int outpcm[N];


void encodeFile(const char *iname, const char *oname)
{
  int ifd = open(iname, O_RDONLY);
  int ofd = open(oname, O_WRONLY|O_CREAT, 0666);
  short  ibuf[ 2 * 8000/100 ];
  unsigned char obuf[1024];
  int h = g729_create();

  int ilen, olen;

  while(0 <= (ilen = read(ifd, ibuf, sizeof(ibuf))))
    {
      olen = pcm16_2_g729(obuf, ibuf, ilen, 1, 8000, h);

      if (olen <= 0)
	break;

      write(ofd, obuf, olen);
    }

  g729_destroy(h);
  close(ofd);
  close(ifd);
}
	
	      

  
void decodeFile(const char *iname, const char *oname)
{
  int ifd = open(iname, O_RDONLY);
  int ofd = open(oname, O_WRONLY|O_CREAT, 0666);
  short  obuf[ 2 * 8000/100 ];
  unsigned char ibuf[20];
  int h = g729_create();

  int ilen, olen;

  while(0 <= (ilen = read(ifd, ibuf, sizeof(ibuf))))
    {
      olen = g729_2_pcm16(obuf, ibuf, ilen, 1, 8000, h);

      if (olen <= 0)
	break;

      write(ofd, obuf, olen);
    }

  g729_destroy(h);
  close(ofd);
  close(ifd);
}
  

int main(int argc, char *argv[])
{

  if (!strcmp(argv[1], "-e"))
    {
      printf("encoding %s to %s\n", argv[2], argv[3]);
      encodeFile(argv[2], argv[3]);
    }
  else if (!strcmp(argv[1], "-d"))
    {
      printf("decoding %s to %s\n", argv[2], argv[3]);
      decodeFile(argv[2], argv[3]);
    }

   
   return(0);
  


} 
#endif
