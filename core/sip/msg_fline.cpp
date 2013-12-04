/*
 * $Id: msg_fline.cpp 850 2008-04-04 21:29:36Z sayer $
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


#include "msg_fline.h"

#include <assert.h>
#include <stdlib.h>

inline void status_code_wr(char** c, int code)
{
    div_t d = div(code, 100);
    *((*c)++) = d.quot + '0';
    d = div(d.rem, 10);
    *((*c)++) = d.quot + '0';
    *((*c)++) = d.rem + '0';
}


void status_line_wr(char** c, int status_code,
		    const cstring& reason)
{
    memcpy(*c,"SIP/2.0 ",8);
    *c += 8;
    
    status_code_wr(c,status_code);

    *((*c)++) = SP;

    memcpy(*c,reason.s,reason.len);
    *c += reason.len;

    *((*c)++) = CR;
    *((*c)++) = LF;
}

void request_line_wr(char** c,
		     const cstring& method,
		     const cstring& ruri)
{
    memcpy(*c,method.s,method.len);
    *c += method.len;

    *((*c)++) = SP;
    
    memcpy(*c,ruri.s,ruri.len);
    *c += ruri.len;
    
    memcpy(*c," SIP/2.0",8);
    *c += 8;

    *((*c)++) = CR;
    *((*c)++) = LF;
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
