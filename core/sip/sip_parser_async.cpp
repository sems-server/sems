#include "sip_parser_async.h"
#include "parse_common.h"
#include "log.h"

#include "AmUtils.h"

#include <string.h>

#include <string>
using std::string;

static int skip_line_async(parser_state* pst, char* end)
{
  char*& c = pst->c;
  int& st = pst->st;
  int& saved_st = pst->saved_st;

  for(; (c < end) && *c; c++){

    switch(st){

    case 0/* START */:
      switch(*c) {
      case_CR_LF;
      default: break;
      }
      break;

    case_ST_CR(*c);

    case ST_LF:
    case ST_CRLF:
      return 0;

    default:
      DBG("Bad state! st=%i\n",st);
      return -99;
    }
  }

  return UNEXPECTED_EOT;
}

static int parse_header_async(sip_header* hdr, parser_state* pst, char* end)
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

    int& st = pst->st;
    int& saved_st = pst->saved_st;

    char** c = &(pst->c);
    char*& begin = pst->beg;

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
 		DBG("Illegal CR or LF in header name: <%.*s>\n",
		    (int)(*c-begin),begin);
 		return MALFORMED_SIP_MSG;

	    case H_VALUE_SWS:
		if(!IS_WSP(**c)){
		    DBG("Malformed header: <%.*s>\n",(int)(*c-begin),begin);
		    return MALFORMED_SIP_MSG;
		}
		break;

	    case H_VALUE:
		if(!IS_WSP(**c)){
		    hdr->value.set(begin,(*c-(st==ST_CRLF?2:1))-begin);

		    //DBG("hdr: \"%.*s: %.*s\"\n",
		    //     hdr->name.len,hdr->name.s,
		    //     hdr->value.len,hdr->value.s);
		    return 0;
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
    case H_VALUE:
	return UNEXPECTED_EOT;

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
	}
	break;
    }
    
    DBG("Incomplete header (st=%i;saved_st=%i)\n",st,saved_st);
    return UNEXPECTED_EOT;
}

int parse_headers_async(parser_state* pst, char* end)
{
  char*& c = pst->c;
  int& st = pst->st;
  int& saved_st = pst->saved_st;
  sip_header* hdr = &(pst->hdr);

  while(c < end) {

    int err = parse_header_async(hdr, pst, end);
    if(err) return err;

    if(hdr->name.len && hdr->value.len) {
      int type = parse_header_type(hdr);
      if(type == sip_header::H_CONTENT_LENGTH)
	str2int(c2stlstr(hdr->value),pst->content_len);
    }

    if(!hdr->name.len && !hdr->value.len) {
      // end-of-headers
      return 0;
    }

    // reset header struct
    memset(hdr,0,sizeof(sip_header));
    st = 0;
    saved_st = 0;
    pst->beg = c;
  }

  return UNEXPECTED_EOT;
}

int skip_sip_msg_async(parser_state* pst, char* end)
{
  enum {
    ST_FL=0,
    ST_HDRS,
    ST_BODY
  };

  int err=0;

  char*& c = pst->c;
  int& stage = pst->stage;

  while(c < end) {

    switch(stage) {
    case ST_FL:
      err = skip_line_async(pst,end);
      break;

    case ST_HDRS:
      err = parse_headers_async(pst,end);
      break;

    case ST_BODY:
      if(!pst->content_len)
	return 0;
      if(pst->content_len > end-c)
	return UNEXPECTED_EOT;
      else
	return 0;
      break;

    default:
      ERROR("unkown state!!!");
      return -1;
    }

    if(!err) {
      switch(stage) {
      case ST_FL:
	stage = ST_HDRS;
	pst->reset_hdr_parser();
	break;
      case ST_HDRS:
	if(!pst->hdr.name.len && !pst->hdr.value.len) {
	  // End-of-Header found
	  stage = ST_BODY;
	  continue;
	}
	else {
	  // End-of-one-Header
	  pst->reset_hdr_parser();
	  continue;
	}
	break;
      }
    }
    else {
      return err;
    }
  }

  return err;
}
