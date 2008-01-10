/*
 * $Id$
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

#ifndef _codecs_h_
#define _codecs_h_

/** 
 * @file codecs.h
 * Centralized definition of all codec IDs.
 * Look at the source file for declarations.
 * they just need to be different from each other.
 */

#define CODEC_PCM16   0
#define CODEC_ULAW    1
#define CODEC_ALAW    2
#define CODEC_GSM0610 3

#define CODEC_ILBC    4
#define CODEC_MP3     5

#define CODEC_TELEPHONE_EVENT 6

#define CODEC_SPEEX_NB 7

#define CODEC_G726_16 8
#define CODEC_G726_24 9
#define CODEC_G726_32 10
#define CODEC_G726_40 11

#define CODEC_L16     12
#endif
