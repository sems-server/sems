/*
 * $Id: parse_header.cpp 1119 2008-10-16 08:54:04Z rco $
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


#include "parse_header.h"
#include "parse_common.h"
#include "defs.h"

#include "log.h"

#include <memory>
using std::auto_ptr;


//
// Header length
//

#define COMPACT_len        1

#define TO_len             SIP_HDR_LEN(SIP_HDR_TO)    // 2
#define VIA_len            SIP_HDR_LEN(SIP_HDR_VIA)   // 3
#define FROM_len           SIP_HDR_LEN(SIP_HDR_FROM)  // 4
#define CSEQ_len           SIP_HDR_LEN(SIP_HDR_CSEQ)  // 4
#define RSEQ_len           SIP_HDR_LEN(SIP_HDR_RSEQ)  // 4
#define RACK_len           SIP_HDR_LEN(SIP_HDR_RACK)  // 4
#define ROUTE_len          SIP_HDR_LEN(SIP_HDR_ROUTE) // 5
#define CALL_ID_len        SIP_HDR_LEN(SIP_HDR_CALL_ID) // 7
#define CONTACT_len        SIP_HDR_LEN(SIP_HDR_CONTACT) // 7
#define REQUIRE_len        SIP_HDR_LEN(SIP_HDR_REQUIRE) // 7
#define CONTENT_TYPE_len   SIP_HDR_LEN(SIP_HDR_CONTENT_TYPE) // 12
#define RECORD_ROUTE_len   SIP_HDR_LEN(SIP_HDR_RECORD_ROUTE) // 12
#define CONTENT_LENGTH_len SIP_HDR_LEN(SIP_HDR_CONTENT_LENGTH) // 14


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


int parse_header_type(sip_header* h)
{
    h->type = sip_header::H_UNPARSED;

    switch(h->name.len){

    case COMPACT_len:{
      switch (LOWER_B(h->name.s[0])) {
      case 'i': { // Call-ID 	
	h->type = sip_header::H_CALL_ID;
      } break;
      case 'm': { // Contact      
	h->type = sip_header::H_CONTACT;
      } break;
	//       case 'e': // Content-Encoding
	// 	{} break;
      case 'l': { // Content-Length
	h->type = sip_header::H_CONTENT_LENGTH;
      } break;
      case 'c': { // Content-Type	
	h->type = sip_header::H_CONTENT_TYPE;
      } break;
      case 'f': { // From
	h->type = sip_header::H_FROM;
      } break;
	//       case 's': // Subject
	// 	{} break;
	//       case 'k': // Supported
	// 	{} break;
      case 't': { // To
	h->type = sip_header::H_TO;
      } break;
      case 'v': {// Via	
	h->type = sip_header::H_VIA;
      } break;
      default:
	h->type = sip_header::H_OTHER;
      } break;
    } break;

    case TO_len:
	if(!lower_cmp(h->name.s,SIP_HDR_TO,TO_len)){
	    h->type = sip_header::H_TO;
	}
	break;

    case VIA_len:
	if(!lower_cmp(h->name.s,SIP_HDR_VIA,VIA_len)){
	    h->type = sip_header::H_VIA;
	}
	break;

    //case FROM_len:
    //case RSEQ_len:
    //case RACK_len:
    case CSEQ_len:
	switch(h->name.s[0]){
	case 'f':
	case 'F':
	    if(!lower_cmp(h->name.s+1,SIP_HDR_FROM+1,FROM_len-1)){
		h->type = sip_header::H_FROM;
	    }
	    break;
	case 'c':
	case 'C':
	    if(!lower_cmp(h->name.s+1,SIP_HDR_CSEQ+1,CSEQ_len-1)){
		h->type = sip_header::H_CSEQ;
	    }
	    break;
        case 'r':
        case 'R':
            switch(h->name.s[1]) {
	    case 's':
	    case 'S':
                    if(!lower_cmp(h->name.s+2, SIP_HDR_RSEQ+2,
                            SIP_HDR_LEN(SIP_HDR_RSEQ)-2))
                        h->type = sip_header::H_RSEQ;
                    break;
	    case 'a':
	    case 'A':
                    if(!lower_cmp(h->name.s+2, SIP_HDR_RACK+2, 
                            SIP_HDR_LEN(SIP_HDR_RACK)-2)) {
                        h->type = sip_header::H_RACK;
                    }
                    break;
            }
            break;
	}
	break;

    case ROUTE_len:
	if(!lower_cmp(h->name.s+1,SIP_HDR_ROUTE+1,ROUTE_len-1)){
	    h->type = sip_header::H_ROUTE;
	}
	break;

    //case CALL_ID_len:
    //case REQUIRE_len:
    case CONTACT_len:
	switch(h->name.s[0]){
	case 'c':
	case 'C':
	    switch(h->name.s[1]){
	    case 'a':
	    case 'A':
		if(!lower_cmp(h->name.s+2,SIP_HDR_CALL_ID+2,CALL_ID_len-2)){
		    h->type = sip_header::H_CALL_ID;
		}
		break;

	    case 'o':
	    case 'O':
		if(!lower_cmp(h->name.s+2,SIP_HDR_CONTACT+2,CONTACT_len-2)){
		    h->type = sip_header::H_CONTACT;
		}
		break;

	    default:
		h->type = sip_header::H_OTHER;
		break;
	    }
	    break;
        case 'r':
        case 'R':
            if (! lower_cmp(h->name.s+1, SIP_HDR_REQUIRE+1,
                SIP_HDR_LEN(SIP_HDR_REQUIRE)-1))
              h->type = sip_header::H_REQUIRE;
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
	    if(!lower_cmp(h->name.s,SIP_HDR_CONTENT_TYPE,CONTENT_TYPE_len)){
		h->type = sip_header::H_CONTENT_TYPE;
	    }
	    break;
	case 'r':
	case 'R':
	    if(!lower_cmp(h->name.s,SIP_HDR_RECORD_ROUTE,RECORD_ROUTE_len)){
		h->type = sip_header::H_RECORD_ROUTE;
	    }
	    break;
	}
	break;

    case CONTENT_LENGTH_len:
	if(!lower_cmp(h->name.s,SIP_HDR_CONTENT_LENGTH,CONTENT_LENGTH_len)){
	    h->type = sip_header::H_CONTENT_LENGTH;
	}
	break;

    }

    if(h->type == sip_header::H_UNPARSED)
	h->type = sip_header::H_OTHER;

    return h->type;
}

void add_parsed_header(list<sip_header*>& hdrs, sip_header* hdr)
{
    parse_header_type(hdr);
    hdrs.push_back(hdr);
}

int parse_headers(list<sip_header*>& hdrs, char** c, char* end)
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
    auto_ptr<sip_header> hdr(new sip_header());

    for(;**c && (*c < end);(*c)++){

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

		    add_parsed_header(hdrs,hdr.release());
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
	    
	    add_parsed_header(hdrs,hdr.release());
	    
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
		
		add_parsed_header(hdrs,hdr.release());
		
		return 0;
	    }
	    break;
	}
	break;
    }
    
    DBG("Incomplete header (st=%i;saved_st=%i)\n",st,saved_st);
    return UNEXPECTED_EOT;
}

void free_headers(list<sip_header*>& hdrs)
{
    while(!hdrs.empty()) {
	delete hdrs.front();
	hdrs.pop_front();
    }
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
