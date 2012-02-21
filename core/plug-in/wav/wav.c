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
#include "g711.h"
#include "wav_hdr.h"
#include "../../log.h"

/**
 * @file plug-in/wav/wav.c
 * Wav file support / sample plug-in declaration.
 * This plug-in is a full description of supported codecs, payloads and file formats.
 *
 * Features: <ul>
 *           <li>ulaw codec/payload/subtype
 *           <li>alaw codec/payload/subtype
 *           <li>wav file format including ulaw/alaw/pcm16 subtypes
 *           </ul>
 *
 * Example declaration:
 @code
 BEGIN_EXPORTS( "wav", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY)

 BEGIN_CODECS
 CODEC( CODEC_ULAW, Pcm16_2_ULaw, ULaw_2_Pcm16, 0, 0, 0, 0, 0 )
 CODEC( CODEC_ALAW, Pcm16_2_ALaw, ALaw_2_Pcm16, 0, 0, 0, 0, 0 )
 END_CODECS
    
 BEGIN_PAYLOADS
 PAYLOAD( 0, "ulaw", 1, 8000, 8000,1, CODEC_ULAW )
 PAYLOAD( 8, "alaw", 1, 8000, 8000,1, CODEC_ALAW )
 END_PAYLOADS

 BEGIN_FILE_FORMATS
 BEGIN_FILE_FORMAT( "Wav", "wav", "application/x-wav", wav_open, wav_close, 0, 0)
 BEGIN_SUBTYPES
 SUBTYPE( WAV_PCM,  "Pcm16",  2, -1, -1, CODEC_PCM16 )
 SUBTYPE( WAV_ALAW, "A-Law",  1, -1, -1, CODEC_ALAW )
 SUBTYPE( WAV_ULAW, "Mu-Law", 1, -1, -1, CODEC_ULAW )
 END_SUBTYPES
 END_FILE_FORMAT
 END_FILE_FORMATS

 END_EXPORTS
 @endcode
*/

/** @def WAV_PCM subtype declaration. */
#define WAV_PCM  1 
/** @def WAV_ALAW subtype declaration. */
#define WAV_ALAW 6 
/** @def WAV_ULAW subtype declaration. */
#define WAV_ULAW 7 

static int ULaw_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static int ALaw_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_ULaw( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static int Pcm16_2_ALaw( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec );

static unsigned int g711_bytes2samples(long, unsigned int);
static unsigned int g711_samples2bytes(long, unsigned int);

BEGIN_EXPORTS( "wav" , AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

     BEGIN_CODECS
      CODEC( CODEC_ULAW, Pcm16_2_ULaw, ULaw_2_Pcm16,
             AMCI_NO_CODEC_PLC, AMCI_NO_CODECCREATE, AMCI_NO_CODECDESTROY,
             g711_bytes2samples, g711_samples2bytes )
      CODEC( CODEC_ALAW, Pcm16_2_ALaw, ALaw_2_Pcm16,
	     AMCI_NO_CODEC_PLC, AMCI_NO_CODECCREATE, AMCI_NO_CODECDESTROY,
	     g711_bytes2samples, g711_samples2bytes )
     END_CODECS
    
     BEGIN_PAYLOADS
        PAYLOAD( 0, "PCMU", 8000, 8000, 1, CODEC_ULAW, AMCI_PT_AUDIO_LINEAR )
        PAYLOAD( 8, "PCMA", 8000, 8000, 1, CODEC_ALAW, AMCI_PT_AUDIO_LINEAR )
     END_PAYLOADS

     BEGIN_FILE_FORMATS
       BEGIN_FILE_FORMAT( "Wav", "wav", "audio/x-wav", wav_open, wav_close, wav_mem_open, wav_mem_close)
         BEGIN_SUBTYPES
           SUBTYPE( WAV_PCM,  "Pcm16",  8000, 1, CODEC_PCM16 ) // we only support 8000/1 channel !
           SUBTYPE( WAV_ALAW, "A-Law",  8000, 1, CODEC_ALAW )
           SUBTYPE( WAV_ULAW, "Mu-Law", 8000, 1, CODEC_ULAW )
         END_SUBTYPES
       END_FILE_FORMAT
     END_FILE_FORMATS

END_EXPORTS

static unsigned int g711_bytes2samples(long h_codec, unsigned int num_bytes)
{
  /* ALAW and ULAW formats has one sample per byte */
  return num_bytes;
}

static unsigned int g711_samples2bytes(long h_codec, unsigned int num_samples)
{
  /* ALAW and ULAW formats has one sample per byte */
  return num_samples;
}

static int ULaw_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec )
{
  unsigned short* out_b = (unsigned short*)out_buf;
  unsigned char*  in_b  = in_buf;
  unsigned char*  end   = in_b + size;

  while(in_b != end){
    //	int i = st_ulaw2linear16(*(in_b++));
    //	*(out_b++) = i;
    *(out_b++) = st_ulaw2linear16(*(in_b++));
  }
  return size*2;
}

static int ALaw_2_Pcm16( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
			 unsigned int channels, unsigned int rate, long h_codec )
{
  unsigned short* out_b = (unsigned short*)out_buf;
  unsigned char*  in_b  = in_buf;
  unsigned char*  end   = in_b + size;

  while(in_b != end){
    //	int i = st_alaw2linear16(*(in_b++));
    //	*(out_b++) = i;
    *(out_b++) = st_alaw2linear16(*(in_b++));
  }
  return size*2;
}

int Pcm16_2_ULaw( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  unsigned char* out_b = out_buf;
  short* in_b          = (short*)(in_buf);
  short* end           = (short*)((unsigned char*)in_buf + size);

  while(in_b != end){
    short s = *(in_b++) >> 2;
    *(out_b++) = st_14linear2ulaw(s);
  }
  return size/2;
}

int Pcm16_2_ALaw( unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
		  unsigned int channels, unsigned int rate, long h_codec )
{
  unsigned char* out_b = out_buf;
  short* in_b          = (short*)(in_buf);
  short* end           = (short*)((unsigned char*)in_buf + size);

  while(in_b != end){
    short s = (*(in_b++)) >> 3;
    *(out_b++) = st_13linear2alaw(s);
  }

  return size/2;
}


