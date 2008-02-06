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

#ifndef _parse_common_h
#define _parse_common_h

#include "cstring.h"

#include <list>
using std::list;

//
// Constants
//

#define UNDEFINED_ERR     -1
#define UNEXPECTED_EOT    -2
#define UNEXPECTED_EOL    -3
#define MALFORMED_SIP_MSG -4
#define MALFORMED_URI     -5

#define IS_IN(c,l,r) (((c)>=(l))&&((c)<=(r)))

#define CR        (0x0d)
#define LF        (0x0a)
#define SP        (0x20)
#define HTAB      (0x09)
#define IS_WSP(c) (0x20==(c)||0x09==(c))

#define HCOLON    (':')
#define COMMA     (',')
#define DQUOTE    ('"')
#define BACKSLASH ('\\')

#define IS_ALPHA(c) (IS_IN(c,0x41,0x5a) || IS_IN(c,0x61,0x7a))
#define IS_DIGIT(c) IS_IN(c,0x30,0x39)
#define IS_ALPHANUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

#define IS_UPPER(c) IS_IN(c,0x41,0x5a)
#define LOWER_B(c) (IS_UPPER(c) ? ((c)+0x20) : (c))


#define IS_TOKEN(c) \
   (IS_ALPHANUM(c) || \
    ((c)=='-') || ((c)=='.') || ((c)=='!') || ((c)=='%') || \
    ((c)=='*') || ((c)=='_') || ((c)=='+') || ((c)=='`') || \
    ((c)=='\'') || ((c)=='~') )

#define IS_MARK(c) \
   (((c)=='-') || ((c)=='_') || ((c)=='.') || ((c)=='!') || \
    ((c)=='~') || ((c)=='*') || ((c)=='\'') || ((c)=='(') || \
    ((c)==')') )

#define IS_UNRESERVED(c) \
   (IS_ALPHANUM(c) || IS_MARK(c))

#define IS_USER_UNRESERVED(c) \
   (((c)=='&') || ((c)=='=') || ((c)=='+') || ((c)=='$') || \
    ((c)==',') || ((c)==';') || ((c)=='?') || ((c)=='/'))

#define IS_USER(c) \
   (IS_UNRESERVED(c) || IS_USER_UNRESERVED(c)) // Escaped chars missing

#define SIPVER_len 7 // "SIP" "/" 1*DIGIT 1*DIGIT


//
// Common states: (>100)
//

enum {
    ST_CR=100,
    ST_LF,
    ST_CRLF,
    ST_EoL_WSP // [CR] LF WSP
};

#define case_CR_LF \
	    case CR:\
		saved_st = st;\
		st = ST_CR;\
		break;\
	    case LF:\
		saved_st = st;\
		st = ST_LF;\
		break

#define case_ST_CR(c) \
	    case ST_CR:\
	        if((c) == LF){\
	    	    st = ST_CRLF;\
	        }\
	        else {\
 		    DBG("CR without LF\n");\
 		    return MALFORMED_SIP_MSG;\
	        }\
	        break

//
// Structs
//

struct sip_avp
{
    cstring name;
    cstring value;

    sip_avp()
	: name(), value()
    {}

    sip_avp(const cstring& n,
	    const cstring& v)
	: name(n),value(v)
    {}
};


//
// Functions
//

inline int lower_cmp(char* l, char* r, int len)
{
    char* end = l+len;

    while(l!=end){
	if( LOWER_B(*l) != *r ){
	    return 1;
	}
	l++; r++;
    }

    return 0;
}

int parse_sip_version(char* beg, int len);

int parse_gen_params(list<sip_avp*>* params, char** c, int len, char stop_char);

#endif
