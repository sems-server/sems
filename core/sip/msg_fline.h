/*
 * $Id: msg_fline.h 850 2008-04-04 21:29:36Z sayer $
 *
 * Copyright (C) 2007 Raphael Coeffic
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


#ifndef _msg_fline_h
#define _msg_fline_h

#include "cstring.h"
#include "parse_common.h"

struct sip_msg;

//
// Request-line builder
//
inline int request_line_len(const cstring& method,
			    const cstring& ruri)
{
    return method.len + ruri.len + SIPVER_len
	+ 4; // 2*SP + CRLF
}

void request_line_wr(char** c,
		     const cstring& method,
		     const cstring& ruri);

//
// Status-line builder
//
inline int status_line_len(const cstring& reason)
{
    return SIPVER_len + 3/*status code*/
	+ reason.len
	+ 4; // 2*SP + CRLF
}

void status_line_wr(char** c, int status_code,
		    const cstring& reason);


#endif


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
