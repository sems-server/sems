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

#ifndef _wav_hdr_h_
#define _wav_hdr_h_

#include "amci.h"

int wav_open(FILE* fp, struct amci_file_desc_t* fmt_desc, 
	     int options, long h_codec);
int wav_close(FILE* fp, struct amci_file_desc_t* fmt_desc, 
	      int options, long h_codec, struct amci_codec_t *codec);

int wav_mem_open(unsigned char* mptr, unsigned long size, unsigned long* pos, 
		 struct amci_file_desc_t* fmt_desc, int options, long h_codec);

int wav_mem_close(unsigned char* mptr, unsigned long* pos,
		  struct amci_file_desc_t* fmt_desc, int options, 
		  long h_codec, struct amci_codec_t *codec);


#endif
