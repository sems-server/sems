/*
 * $Id: cstring.h 1713 2010-03-30 14:11:14Z rco $
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

#ifndef _CSTRING_H
#define _CSTRING_H

#include <string.h>

struct cstring 
{
    const char*  s;
    unsigned int  len;

    cstring()
	: s(0), len(0)
    {}

    cstring(char* s)
	: s(s), len(strlen(s))
    {}

     cstring(const char* s) 
     : s(s), len(strlen(s)) 
     {} 

    cstring(const char* s, unsigned int l)
    : s(s), len(l)
    {}

    void set(const char* _s, unsigned int _len){
	s = _s;
	len = _len;
    }
    
    void clear(){
	s = 0;
	len = 0;
    }

    bool operator == (const cstring& rhs_str) {
      return memcmp(rhs_str.s,s,len <= rhs_str.len ? len : rhs_str.len) == 0;
    }

    bool operator == (const char* rhs_str) {
      unsigned int rhs_len = strlen(rhs_str);
      return memcmp(rhs_str,s,len <= rhs_len ? len : rhs_len) == 0;
    }
};

#define c2stlstr(str) \
          string((str).s,(str).len)

#define stl2cstr(str) \
          cstring((char*)(str).c_str(),(str).length())

#endif
