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

#include "parse_from_to.h"
#include "parse_common.h"
#include "log.h"

sip_from_to::~sip_from_to()
{
    list<sip_avp*>::iterator it = params.begin();
    for(;it != params.end(); ++it){
	
	delete *it;
    }
}

int parse_nameaddr(sip_nameaddr* na, char** c, int len)
{
    enum {

	NA_SWS,
	NA_MAYBE_URI,
	NA_MAYBE_URI_END,
	NA_DISP,
	NA_DISP_QUOTED,
	NA_DISP_LAQUOT,
	NA_URI
    };


    char* beg = *c;
    char* end = *c + len;
    
    char* uri_end=0;

    int saved_st=0, st=NA_SWS;
    //int ret=0;

    for(;*c!=end;(*c)++){
	
	switch(st){

	case NA_SWS:
	    switch(**c){

	    case '\"':
		st = NA_DISP_QUOTED;
	        beg = *c;
		break;

	    case '<':
		st = NA_URI;
		beg = *c+1;
		break;

	    case CR:
	    case LF:
	    case SP:
	    case HTAB:
		break;

	    default:
		st = NA_MAYBE_URI;
		beg = *c;
		break;
	    }
	    break;

	case NA_MAYBE_URI:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		st = NA_MAYBE_URI_END;
		uri_end = *c;
		break;

	    case '<':
		st = NA_URI;
		na->name.set(beg, *c - beg);
		beg = *c+1;		
		break;

	    case ';':
		na->addr.set(beg, *c - beg);
		return 0;
	    }
	    break;

	case NA_MAYBE_URI_END:
	    switch(**c){

	    case_CR_LF;

	    case ';':
		na->addr.set(beg, uri_end - beg);
		return 0;

	    case '<':
		st = NA_URI;
		na->name.set(beg, uri_end - beg);
		beg = *c+1;
		break;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = NA_DISP;
		break;
	    }
	    break;

	case NA_DISP:
	    switch(**c){

	    case '\"':
		st = NA_DISP_QUOTED;
	        beg = *c;
		break;

	    case '<':
		st = NA_URI;
		na->name.set(beg, *c - beg);
		beg = *c+1;
		break;
	    }
	    break;

	case NA_DISP_QUOTED:
	    switch(**c){

	    case '\"':
		st = NA_DISP_LAQUOT;
		na->name.set(beg, *c - beg + 1);
		break;

	    case '\\':
		if(!*(++(*c))){
		    DBG("Escape char in quoted str at EoT!!!\n");
		    return MALFORMED_SIP_MSG;
		}
		break;
	    }
	    break;

	case NA_DISP_LAQUOT:
	    switch(**c){

	    case_CR_LF;

	    case '<':
		st = NA_URI;
		beg = *c+1;
		break;

	    case SP:
	    case HTAB:
		break;

	    default:
		DBG("'<' expected, found %c\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case NA_URI:
	    if(**c == '>'){

		na->addr.set(beg, *c - beg);
		(*c)++;
		return 0;
	    }
	    break;

	case_ST_CR(**c);

	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){
	    case NA_MAYBE_URI:
		saved_st = NA_MAYBE_URI_END;
		uri_end = *c - (st==ST_CRLF?2:1);
		break;
	    }
	    st = saved_st;
	    break;
	}
    }


    switch(st){
    case NA_MAYBE_URI:
	uri_end = *c;
    case NA_MAYBE_URI_END:
	na->addr.set(beg, uri_end - beg);
	break;

    default:
	DBG("Incomplete name-addr (st=%i) <%.*s>\n",st,(int)(end-beg),beg);
	return MALFORMED_SIP_MSG;
    }
    
    return 0;
}


int parse_from_to(sip_from_to* ft, char* beg, int len)
{
    enum {
	FTP_BEG,

	FTP_TAG1,
	FTP_TAG2,
	FTP_TAG3,

	FTP_OTHER
    };

    char* c = beg;
    char* end = c+len;

    int ret = parse_nameaddr(&ft->nameaddr,&c,len);
    if(ret) return ret;

    ret = parse_gen_params(&ft->params,&c, end-c, 0);
    
    if(!ft->params.empty()){

	list<sip_avp*>::iterator it = ft->params.begin();
	for(;it!=ft->params.end();++it){

	    char* c = (*it)->name.s;
	    char* end = c + (*it)->name.len;
	    int st = FTP_BEG;
	    
	    for(;c!=end;c++){

#define case_FT_PARAM(st1,ch1,ch2,st2)\
	    case st1:\
		switch(*c){\
		case ch1:\
		case ch2:\
		    st = st2;\
		    break;\
		default:\
		    st = FTP_OTHER;\
		}\
		break

		switch(st){
		    case_FT_PARAM(FTP_BEG, 't','T',FTP_TAG1);
		    case_FT_PARAM(FTP_TAG1,'a','A',FTP_TAG2);
		    case_FT_PARAM(FTP_TAG2,'g','G',FTP_TAG3);

		case FTP_OTHER:
		    goto next_param;
		}
	    }

	    switch(st){
	    case FTP_TAG3:
		ft->tag = (*it)->value;
		break;
	    }

	next_param:
	    continue;
	}
    }
    
    return ret;
}
