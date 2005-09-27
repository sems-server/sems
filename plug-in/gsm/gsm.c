/*
 * $Id: gsm.c,v 1.9.2.1 2005/08/25 06:55:13 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
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

BEGIN_EXPORTS( "gsm" )

    BEGIN_CODECS
      CODEC( CODEC_GSM0610, 1, pcm16_2_gsm, gsm_2_pcm16, 
	     (amci_codec_init_t)gsm_create_if, (amci_codec_destroy_t)gsm_destroy )
    END_CODECS
    
    BEGIN_PAYLOADS
      PAYLOAD( 3, "GSM", 8000, 1, CODEC_GSM0610, AMCI_PT_AUDIO_FRAME )
    END_PAYLOADS

    BEGIN_FILE_FORMATS
    END_FILE_FORMATS

END_EXPORTS

static int pcm16_2_gsm(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec )
{
    div_t blocks;
    blocks = div(size,320);

    if(blocks.rem){
	ERROR("pcm16_2_gsm: number of blocks should be integral (block size = 320)\n");
	return -1;
    }

    gsm_encode((gsm)h_codec,(gsm_signal*)in_buf,out_buf);
    return blocks.quot * 33;
}

static int gsm_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size, 
		       unsigned int channels, unsigned int rate, long h_codec )
{
    div_t blocks;
    unsigned int out_size;

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

    gsm_decode((gsm)h_codec,in_buf,(gsm_signal*)out_buf);

    return out_size;
}


long gsm_create_if(const char* format_parameters, amci_codec_fmt_info_t* format_description) { 
  format_description[0].id = AMCI_FMT_FRAME_LENGTH ;
  format_description[0].value = 20;
  format_description[1].id = AMCI_FMT_FRAME_SIZE;
  format_description[1].value = 160;
  format_description[2].id =  AMCI_FMT_ENCODED_FRAME_SIZE;
  format_description[2].value = 33;
  format_description[3].id = 0;

  return (long)gsm_create();
}



