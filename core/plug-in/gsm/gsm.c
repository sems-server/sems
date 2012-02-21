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
#include "gsm-1.0-pl10/inc/gsm.h"
#include "../../log.h"

#include <stdlib.h>


static int pcm16_2_gsm(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec );

static int gsm_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec );

static long gsm_create_if(const char* format_parameters, amci_codec_fmt_info_t* format_description); 

static void gsm_destroy_if(long h_codec);

static unsigned int gsm_bytes2samples(long, unsigned int);
static unsigned int gsm_samples2bytes(long, unsigned int);

BEGIN_EXPORTS( "gsm", AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

  BEGIN_CODECS
    CODEC( CODEC_GSM0610, pcm16_2_gsm, gsm_2_pcm16, AMCI_NO_CODEC_PLC,
           gsm_create_if, (amci_codec_destroy_t)gsm_destroy_if, gsm_bytes2samples, gsm_samples2bytes )
  END_CODECS
    
  BEGIN_PAYLOADS
    PAYLOAD( 3, "GSM", 8000, 8000, 1, CODEC_GSM0610, AMCI_PT_AUDIO_FRAME )
  END_PAYLOADS

  BEGIN_FILE_FORMATS
  END_FILE_FORMATS

END_EXPORTS

static unsigned int gsm_bytes2samples(long h_codec, unsigned int num_bytes)
{
  return 160 * (num_bytes / 33);
}

static unsigned int gsm_samples2bytes(long h_codec, unsigned int num_samples)
{
  return 33 * (num_samples / 160);
}

static int pcm16_2_gsm(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec )
{
  int i;
  gsm* h_arr;
  div_t blocks;

  h_arr = (gsm*)h_codec;
  blocks = div(size,320);

  if(blocks.rem){
    ERROR("pcm16_2_gsm: number of blocks should be integral (block size = 320)\n");
    return -1;
  }

  for (i=0;i<blocks.quot;i++)
    gsm_encode(h_arr[0],(gsm_signal*)(in_buf + i*320),out_buf + i*33);

  return blocks.quot * 33;
}

static int gsm_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec )
{
  int i;
  gsm* h_arr;
  div_t blocks;
  unsigned int out_size;

  h_arr = (gsm*)h_codec;
  blocks = div(size,33);

  if(blocks.rem){
    ERROR("gsm_2_pcm16: number of blocks should be integral (block size = 33)\n");
    return -1;
  }

  out_size = blocks.quot * 320;

  if(out_size > AUDIO_BUFFER_SIZE){

    ERROR("gsm_2_pcm16: converting buffer would lead to buffer overrun:\n");
    ERROR("gsm_2_pcm16: input size=%u; needed output size=%u; buffer size=%u\n",
	  size,out_size,AUDIO_BUFFER_SIZE);
    return -1;
  }

  for (i=0;i<blocks.quot;i++) 
    gsm_decode(h_arr[1],in_buf + i*33,(gsm_signal*)(out_buf + i*320));

  return out_size;
}


static long gsm_create_if(const char* format_parameters, amci_codec_fmt_info_t* format_description)
{ 
  gsm* h_codec=0;
    
  h_codec = malloc(sizeof(gsm)*2);
  if(!h_codec){
    ERROR("gsm.c: could not create handle array\n");
    return 0;
  }

  h_codec[0] = gsm_create();
  h_codec[1] = gsm_create();

  format_description[0].id = AMCI_FMT_FRAME_LENGTH ;
  format_description[0].value = 20;
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = 160;
  format_description[2].id =  AMCI_FMT_ENCODED_FRAME_SIZE;
  format_description[2].value = 33;
  format_description[3].id = 0;

    
  return (long)h_codec;
}

static void gsm_destroy_if(long h_codec)
{
  gsm* h_arr = (gsm*)h_codec;

  gsm_destroy(h_arr[0]);
  gsm_destroy(h_arr[1]);

  free(h_arr);
}

