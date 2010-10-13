/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

/*
 * telephone_event.h  --  RTP Payload for DTMF Digits (RFC 2833)
 */

#ifndef _telephone_event_h_
#define _telephone_event_h_

#include <sys/types.h>

/*
 * The type definitions below are valid for 32-bit architectures and
 * may have to be adjusted for 16- or 64-bit architectures.
 */
typedef unsigned char  u_int8;
typedef unsigned short u_int16;
typedef unsigned int   u_int32;

/*
 * DTMF Payload
 */
typedef struct {
    
    u_int8  event;

#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
    u_int8  e:1;
    u_int8  r:1;
    u_int8  volume:6;
#else
    u_int8  volume:6;
    u_int8  r:1;
    u_int8  e:1;
#endif

    u_int16 duration;

} dtmf_payload_t;

#endif
