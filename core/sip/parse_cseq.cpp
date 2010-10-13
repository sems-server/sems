/*
 * $Id: parse_cseq.cpp 850 2008-04-04 21:29:36Z sayer $
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

#include "parse_cseq.h"
#include "parse_common.h"

#include "log.h"

int parse_cseq(sip_cseq* cseq, const char* beg, int len)
{
    enum {
	C_NUM=0,
	C_NUM_SWS,
	C_METHOD
    };


    const char* c = beg;
    const char* end = c+len;

    int saved_st=0, st=C_NUM;

    for(;c!=end;c++){

	switch(st){

	case C_NUM:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		st = C_NUM_SWS;
		cseq->num_str.set(beg, c-beg);
		break;
		
	    default:
		if(!IS_DIGIT(*c)){
		    return MALFORMED_SIP_MSG;
		}
		cseq->num = cseq->num*10 + *c - '0';
		break;
	    }
	    break;

	case C_NUM_SWS:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    default:
		st = C_METHOD;
		beg = c;
		break;
	    }
	    break;

	case C_METHOD:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		cseq->method_str.set(beg,c-beg);
		return 0;
	    }
	    break;

	case_ST_CR(*c);

	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){
	    case C_NUM:
		cseq->num_str.set(beg,c-(st==ST_CRLF?2:1)-beg);
		break;
	    case C_METHOD:
		cseq->method_str.set(beg,c-beg);
		return 0;
	    }
	    st = saved_st;
	    break;
	}
    }

    if(st != C_METHOD){
	return MALFORMED_SIP_MSG;
    }

    cseq->method_str.set(beg,c-beg);
    if(parse_method(&cseq->method, cseq->method_str.s, cseq->method_str.len) < 0){
	
	DBG("Cseq method parsing failed\n");
	return MALFORMED_SIP_MSG;
    }

    return 0;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
