/*
 * Copyright (C) 2011 Raphael Coeffic
 *               2012 Frafos GmbH
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
#include "parse_nameaddr.h"
#include "parse_common.h"
#include "log.h"

#include <assert.h>

sip_nameaddr::~sip_nameaddr()
{
  free_gen_params(&params);
}

int parse_nameaddr(sip_nameaddr* na, const char** c, int len)
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


    const char* beg = *c;
    const char* end = *c + len;
    
    const char* uri_end=0;

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
		return parse_gen_params_sc(&na->params,c, end-*c, 0);
	    }
	    break;

	case NA_MAYBE_URI_END:
	    switch(**c){

	    case_CR_LF;

	    case ';':
		na->addr.set(beg, uri_end - beg);
		return parse_gen_params_sc(&na->params,c, end-*c, 0);

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
		return parse_gen_params_sc(&na->params,c, end-*c, 0);
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
    
    return parse_gen_params_sc(&na->params,c, end-*c, 0);
}

int parse_nameaddr_uri(sip_nameaddr* na, const char** c, int len)
{
    if(parse_nameaddr(na, c, len) < 0) {
      
      DBG("Parsing name-addr failed\n");
      return -1;
    }
    
    if(parse_uri(&na->uri,na->addr.s,na->addr.len) < 0) {
	
	DBG("Parsing uri failed\n");
	return -1;
    }

    return 0;
}

static int skip_2_next_nameaddr(const char*& c, 
				const char*& na_end,
				const char*  end)
{
    assert(c && end && (c<=end));

    // detect beginning of next nameaddr
    enum {
	RR_BEGIN=0,
	RR_QUOTED,
	RR_SWS,
	RR_SEP_SWS,  // space(s) after ','
	RR_NXT_NA
    };

    int st = RR_BEGIN;
    na_end = NULL;
    for(;c<end;c++){
		
	switch(st){
	case RR_BEGIN:
	    switch(*c){
	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
	        st = RR_SWS;
	        na_end = c;
		break;
	    case COMMA:
		st = RR_SEP_SWS;
		na_end = c;
		break;
	    case DQUOTE:
		st = RR_QUOTED;
		break;
	    }
	    break;
	case RR_QUOTED:
	    switch(*c){
	    case BACKSLASH:
		if(++c == end) goto error;
		break;
	    case DQUOTE:
		st = RR_BEGIN;
		break;
	    }
	    break;
	case RR_SWS:
	    switch(*c){
	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
		break;
	    case COMMA:
		st = RR_SEP_SWS;
		break;
	    default:
		st = RR_BEGIN;
		na_end = NULL;
		break;
	    }
	    break;
	case RR_SEP_SWS:
	    switch(*c){
	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
		break;
	    default:
		st = RR_NXT_NA;
		goto nxt_nameaddr;
	    }
	    break;
	}
    }

 nxt_nameaddr:
 error:
	    
    switch(st){
    case RR_QUOTED:
	DBG("Malformed nameaddr\n");
	return -1;

    case RR_SEP_SWS: // not fine, but acceptable
    case RR_BEGIN: // end of route header
        na_end = c;
        return 0;

    case RR_NXT_NA:
        return 1; // next nameaddr available
    }

    // should never be reached
    // makes GCC happy
    return 0;
}

int parse_nameaddr_list(list<cstring>& nas, const char* c, int len)
{
    const char* end = c + len;
    const char* na_end = NULL;

    while(c < end) {

      const char* na_begin = c;
      int err = skip_2_next_nameaddr(c,na_end,end);
      if(err < 0){
	ERROR("While parsing nameaddr list ('%.*s')\n",len,na_begin);
	return -1;
      }

      if(na_end) {
	nas.push_back(cstring(na_begin, na_end-na_begin));
      }

      if(err == 0)
	break;
    }

    return 0;
}

int parse_first_nameaddr(sip_nameaddr* na, const char* c, int len)
{
  const char* tmp_c = c;
  const char* end = c + len;
  const char* na_end = NULL;

  int err = skip_2_next_nameaddr(tmp_c,na_end,end);
  if(err < 0){
    ERROR("While parsing first nameaddr ('%.*s')\n",len,c);
    return -1;
  }

  tmp_c = c;
  return parse_nameaddr(na,&tmp_c,na_end-tmp_c);
}
