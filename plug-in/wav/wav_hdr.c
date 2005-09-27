/*
 * $Id: wav_hdr.c,v 1.6.2.2 2005/08/25 06:55:13 rco Exp $
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

#include "wav_hdr.h"
#include "../../log.h"

#include <string.h>

#define SAFE_READ(buf,s,fp,sr) \
    sr = fread(buf,s,1,fp);\
    if((sr != 1) || ferror(fp)) return -1;

#define SAFE_WRITE(buf,s,fp) \
    fwrite(buf,s,1,fp);\
    if(ferror(fp)) return -1;

static int wav_read_header(FILE* fp, struct amci_file_desc_t* fmt_desc)
{
    unsigned int s;

    char tag[4]={'\0'};
    unsigned int file_size=0;
    unsigned int chunk_size=0;
    unsigned short fmt=0;
    unsigned short channels=0;
    unsigned int rate=0;
    unsigned short bits_per_sample=0;

    if(!fp)
	return -1;
    rewind(fp);

    DBG("trying to read WAV file\n");

    SAFE_READ(tag,4,fp,s);
    DBG("tag = <%.4s>\n",tag);
    if(strncmp(tag,"RIFF",4)){
	DBG("wrong format !");
	return -1;
    }

    SAFE_READ(&file_size,4,fp,s);
    DBG("file size = <%u>\n",file_size);

    SAFE_READ(tag,4,fp,s);
    DBG("tag = <%.4s>\n",tag);
    if(strncmp(tag,"WAVE",4)){
	DBG("wrong format !");
	return -1;
    }

    SAFE_READ(tag,4,fp,s);
    DBG("tag = <%.4s>\n",tag);
    if(strncmp(tag,"fmt ",4)){
	DBG("wrong format !");
	return -1;
    }
    
    SAFE_READ(&chunk_size,4,fp,s);
    DBG("chunk_size = <%u>\n",chunk_size);
    
    SAFE_READ(&fmt,2,fp,s);
    DBG("fmt = <%.2x>\n",fmt);

    SAFE_READ(&channels,2,fp,s);
    DBG("channels = <%i>\n",channels);

    SAFE_READ(&rate,4,fp,s);
    DBG("rate = <%i>\n",rate);

    /* do not read bytes/sec and block align */
    fseek(fp,6,SEEK_CUR);

    SAFE_READ(&bits_per_sample,2,fp,s);
    DBG("bits/sample = <%i>\n",bits_per_sample);

    fmt_desc->subtype = fmt;
    fmt_desc->sample = bits_per_sample>>3;
    fmt_desc->rate = rate;
    fmt_desc->channels = channels;

    if( (fmt == 0x01) && (fmt_desc->sample == 1)){
	ERROR("Sorry, we don't support PCM 8 bit\n");
	return -1;
    }

    fseek(fp,chunk_size-16,SEEK_CUR);

    for(;;) {

	SAFE_READ(tag,4,fp,s);
	DBG("tag = <%.4s>\n",tag);
	
	SAFE_READ(&chunk_size,4,fp,s);
	DBG("chunk size = <%i>\n",chunk_size);
	
	if(!strncmp(tag,"data",4))
	    break;

	fseek(fp,chunk_size,SEEK_CUR);
    }

    return 0;
}

int wav_open(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{
    if(options == AMCI_RDONLY){
	return wav_read_header(fp,fmt_desc);
    }
    else {
	/*  Reserve some space for the header */
	/*  We need this, as information for headers  */
	/*  like 'size' is not known yet */
	fseek(fp,44L,SEEK_CUR); 
	return (ferror(fp) ? -1 : 0);
    }
}

int wav_write_header(FILE* fp, struct amci_file_desc_t* fmt_desc)
{
    unsigned int file_size;
    unsigned int chunk_size;
    unsigned short fmt;
    unsigned short channels;
    unsigned int rate;
    unsigned short block_align;
    unsigned int bytes_per_sec;
    unsigned short bits_per_sample;

    SAFE_WRITE("RIFF",4,fp);

    file_size = fmt_desc->data_size + 36;
    SAFE_WRITE(&file_size,4,fp);

    SAFE_WRITE("WAVE",4,fp);

    SAFE_WRITE("fmt ",4,fp);

    chunk_size = 16;
    SAFE_WRITE(&chunk_size,4,fp);

    fmt = fmt_desc->subtype;
    SAFE_WRITE(&fmt,2,fp);
    DBG("(wav_hdr.c) fmt = <%i>\n",fmt);

    channels = (unsigned short)fmt_desc->channels;
    SAFE_WRITE(&channels,2,fp);
    DBG("(wav_hdr.c) channels = <%i>\n",channels);

    rate = (unsigned int)fmt_desc->rate;
    SAFE_WRITE(&rate,4,fp);
    DBG("(wav_hdr.c) rate = <%i>\n",rate);

    block_align = channels * fmt_desc->sample;
    bytes_per_sec = rate * (unsigned int)block_align;
    SAFE_WRITE(&bytes_per_sec,4,fp);
    SAFE_WRITE(&block_align,2,fp);

    bits_per_sample = (unsigned short)(fmt_desc->sample * 8);
    SAFE_WRITE(&bits_per_sample,2,fp);

    SAFE_WRITE("data",4,fp);
    SAFE_WRITE(&fmt_desc->data_size,4,fp);
    DBG("(wav_hdr.c) data_size = <%i>\n",fmt_desc->data_size);

    return 0;
}


int wav_close(FILE* fp, struct amci_file_desc_t* fmt_desc, int options, long h_codec)
{
    if(options == AMCI_WRONLY){
	rewind(fp);
	return wav_write_header(fp,fmt_desc);
    }
    return 0;
}









