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


#include "parse_header.h"
#include "parse_common.h"
#include "sip_parser.h"

#include "log.h"

#include <memory>
using std::auto_ptr;


//
// Header length
//

#define TO_len             2
#define VIA_len            3
#define FROM_len           4
#define CSEQ_len           4
#define ROUTE_len          5
#define CALL_ID_len        7
#define CONTACT_len        7
#define CONTENT_TYPE_len   12
#define RECORD_ROUTE_len   12
#define CONTENT_LENGTH_len 14


//
// Low case headers 
//

char* TO_lc = "to";
char* VIA_lc = "via";
char* FROM_lc = "from";
char* CSEQ_lc = "cseq";
char* ROUTE_lc = "route";
char* CALL_ID_lc = "call-id";
char* CONTACT_lc = "contact";
char* CONTENT_TYPE_lc = "content-type";
char* RECORD_ROUTE_lc = "record-route";
char* CONTENT_LENGTH_lc = "content-length";


sip_header::sip_header()
    : type(H_UNPARSED),
      name(),value(),
      p(NULL)
{}

sip_header::sip_header(const sip_header& hdr)
    : type(hdr.type),
      name(hdr.name),
      value(hdr.value),
      p(NULL)
{
}

sip_header::sip_header(int type, const cstring& name, const cstring& value)
    : type(type),
      name(name),
      value(value),
      p(NULL)
{
}

sip_header::~sip_header()
{
    delete p;
}


static int parse_header_type(sip_msg* msg, sip_header* h)
{
    h->type = sip_header::H_UNPARSED;

    switch(h->name.len){

    case TO_len:
	if(!lower_cmp(h->name.s,TO_lc,TO_len)){
	    h->type = sip_header::H_TO;
	    msg->to = h;
	}
	break;

    case VIA_len:
	if(!lower_cmp(h->name.s,VIA_lc,VIA_len)){
	    h->type = sip_header::H_VIA;
	    if(!msg->via1)
		msg->via1 = h;
	}
	break;

    //case FROM_len:
    case CSEQ_len:
	switch(h->name.s[0]){
	case 'f':
	case 'F':
	    if(!lower_cmp(h->name.s+1,FROM_lc+1,FROM_len-1)){
		h->type = sip_header::H_FROM;
		msg->from = h;
	    }
	    break;
	case 'c':
	case 'C':
	    if(!lower_cmp(h->name.s+1,CSEQ_lc+1,CSEQ_len-1)){
		h->type = sip_header::H_CSEQ;
		msg->cseq = h;
	    }
	    break;
	default:
	    h->type = sip_header::H_OTHER;
	    break;
	}
	break;

    case ROUTE_len:
	if(!lower_cmp(h->name.s+1,ROUTE_lc+1,ROUTE_len-1)){
	    h->type = sip_header::H_ROUTE;
	    msg->route.push_back(h);
	}
	break;

    //case CALL_ID_len:
    case CONTACT_len:
	switch(h->name.s[0]){
	case 'c':
	case 'C':
	    switch(h->name.s[1]){
	    case 'a':
	    case 'A':
		if(!lower_cmp(h->name.s+2,CALL_ID_lc+2,CALL_ID_len-2)){
		    h->type = sip_header::H_CALL_ID;
		    msg->callid = h;
		}
		break;

	    case 'o':
	    case 'O':
		if(!lower_cmp(h->name.s+2,CONTACT_lc+2,CONTACT_len-2)){
		    h->type = sip_header::H_CONTACT;
		    msg->contact = h;
		}
		break;

	    default:
		h->type = sip_header::H_OTHER;
		break;
	    }
	    break;

	default:
	    h->type = sip_header::H_OTHER;
	    break;
	}
	break;

    //case RECORD_ROUTE_len:
    case CONTENT_TYPE_len:
	switch(h->name.s[0]){
	case 'c':
	case 'C':
	    if(!lower_cmp(h->name.s,CONTENT_TYPE_lc,CONTENT_TYPE_len)){
		h->type = sip_header::H_CONTENT_TYPE;
		msg->content_type = h;
	    }
	    break;
	case 'r':
	case 'R':
	    if(!lower_cmp(h->name.s,RECORD_ROUTE_lc,RECORD_ROUTE_len)){
		h->type = sip_header::H_RECORD_ROUTE;
		msg->record_route.push_back(h);
	    }
	    break;
	}
	break;

    case CONTENT_LENGTH_len:
	if(!lower_cmp(h->name.s,CONTENT_LENGTH_lc,CONTENT_LENGTH_len)){
	    h->type = sip_header::H_CONTENT_LENGTH;
	    msg->content_length = h;
	}
	break;

    }

    if(h->type == sip_header::H_UNPARSED)
	h->type = sip_header::H_OTHER;

    return h->type;
}

inline void add_parsed_header(sip_msg* msg, sip_header* hdr)
{
    parse_header_type(msg,hdr);
    msg->hdrs.push_back(hdr);
}

int parse_headers(sip_msg* msg, char** c)
{
    //
    // Header states
    //
    enum {
	H_NAME=0,
	H_HCOLON,
	H_VALUE_SWS,
	H_VALUE,
    };

    int st = H_NAME;
    int saved_st = 0;

    char* begin = *c;
    //bool  cr = false;

    auto_ptr<sip_header> hdr(new sip_header());

    for(;**c;(*c)++){

	switch(st){

	case H_NAME:
	    switch(**c){

	    case_CR_LF;

	    case HCOLON:
		st = H_VALUE_SWS;
		hdr->name.set(begin,*c-begin);
		break;

	    case SP:
	    case HTAB:
		st = H_HCOLON;
		hdr->name.set(begin,*c-begin);
		break;
	    }
	    break;

	case H_VALUE_SWS:
	    switch(**c){

		case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = H_VALUE;
		begin = *c;
		break;
		
	    };
	    break;

	case H_VALUE:
	    switch(**c){
		case_CR_LF;
	    };
	    break;

	case H_HCOLON:
	    switch(**c){
	    case HCOLON:
		st = H_VALUE_SWS;
		begin = *c+1;
		break;

	    case SP:
	    case HTAB:
		break;

	    default:
		DBG("Missing ':' after header name\n");
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case_ST_CR(**c);

	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){

	    case H_NAME:
		if((*c-(st==ST_CRLF?2:1))-begin == 0){
		    //DBG("Detected end of headers\n");
		    return 0;
		}
 		DBG("Illegal CR or LF in header name\n");
 		return MALFORMED_SIP_MSG;

	    case H_VALUE_SWS:
		if(!IS_WSP(**c)){
		    DBG("Malformed header: <%.*s>\n",(int)(*c-begin),begin);
		    begin = *c;
		    saved_st = H_NAME;
		}
		break;

	    case H_VALUE:
		if(!IS_WSP(**c)){
		    hdr->value.set(begin,(*c-(st==ST_CRLF?2:1))-begin);

		    //DBG("hdr: \"%.*s: %.*s\"\n",
		    //     hdr->name.len,hdr->name.s,
		    //     hdr->value.len,hdr->value.s);

		    add_parsed_header(msg,hdr.release());
		    hdr.reset(new sip_header());
		    begin = *c;
		    saved_st = H_NAME;
		    //re-parse cur char w. new state
		    (*c)--;
		}
		break;

	    default:
		DBG("Oooops! st=%i\n",saved_st);
		break;
	    }

	    st = saved_st;
	    break;
	}
    }

    switch(st){

    case H_NAME:
	DBG("Incomplete header (st=%i;saved_st=%i)\n",st,saved_st);
	if(st == H_NAME){
	    DBG("header = \"%.*s\"\n",(int)(*c - begin), begin);
	}
	return UNEXPECTED_EOT;

    case H_VALUE:
	if(*c - begin > 0){
	
	    hdr->value.set(begin,*c - begin);
	    
	    //DBG("hdr: \"%.*s: %.*s\"\n",
	    //	hdr->name.len,hdr->name.s,
	    //	hdr->value.len,hdr->value.s);
	    
	    add_parsed_header(msg,hdr.release());
	    
	    return 0;
	}
	
	break;

    case ST_LF:
    case ST_CRLF:
	switch(saved_st){
	    
	case H_NAME:
	    if((*c-(st==ST_CRLF?2:1))-begin == 0){
		//DBG("Detected end of headers\n");
		return 0;
	    }
	    DBG("Illegal CR or LF in header name\n");
	    return MALFORMED_SIP_MSG;

	case H_VALUE:
	    if(*c - begin > 2){
		
		hdr->value.set(begin,*c - begin - (st==ST_CRLF? 2 : 1));
		
		//DBG("hdr: \"%.*s: %.*s\"\n",
		//	hdr->name.len,hdr->name.s,
		//	hdr->value.len,hdr->value.s);
		
		add_parsed_header(msg,hdr.release());
		
		return 0;
	    }
	    break;
	}
	break;
    }
    
    DBG("Incomplete header (st=%i;saved_st=%i)\n",st,saved_st);
    return UNEXPECTED_EOT;
}
