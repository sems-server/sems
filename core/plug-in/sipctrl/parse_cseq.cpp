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

#include "parse_cseq.h"
#include "parse_common.h"

#include "log.h"

int parse_cseq(sip_cseq* cseq, char* beg, int len)
{
    enum {
	C_NUM=0,
	C_NUM_SWS,
	C_METHOD
    };


    char* c = beg;
    char* end = c+len;

    int saved_st=0, st=C_NUM;

    for(;c!=end;c++){

	switch(st){

	case C_NUM:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		st = C_NUM_SWS;
		cseq->str.set(beg, c-beg);
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
		cseq->method.set(beg,c-beg);
		return 0;
	    }
	    break;

	case_ST_CR(*c);

	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){
	    case C_NUM:
		cseq->str.set(beg,c-(st==ST_CRLF?2:1)-beg);
		break;
	    case C_METHOD:
		cseq->method.set(beg,c-beg);
		return 0;
	    }
	    st = saved_st;
	    break;
	}
    }

    if(st != C_METHOD){
	return MALFORMED_SIP_MSG;
    }

    cseq->method.set(beg,c-beg);
    return 0;
}
