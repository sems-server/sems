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

#ifndef _CSTRING_H
#define _CSTRING_H

#include <string.h>

struct cstring 
{
    char*  s;
    int  len;

    cstring()
	: s(0), len(0)
    {}

    cstring(char* s)
	: s(s), len(strlen(s))
    {}

    cstring(char* s, int l)
	: s(s), len(l)
    {}

    void set(char* _s, int _len){
	s = _s;
	len = _len;
    }
    
    void clear(){
	s = 0;
	len = 0;
    }
};

#define c2stlstr(str) \
          string(str.s,str.len)

#define stl2cstr(str) \
          cstring((char*)str.c_str(),str.length())

#endif
