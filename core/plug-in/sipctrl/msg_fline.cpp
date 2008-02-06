/*
 * $Id$
 *
 * Copyright (C) 2007 Raphael Coeffic
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


#include "msg_fline.h"

#include <assert.h>

inline void status_code_wr(char** c, int code)
{
    int div = code / 100;
    *((*c)++) = div + '0';
    code -= div*100;

    div = code / 10;
    *((*c)++) = div + '0';
    code -= div*10;
    
    *((*c)++) = code + '0';
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
