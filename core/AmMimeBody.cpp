#include "AmMimeBody.h"
#include "sip/parse_common.h"
#include "sip/parse_header.h"
#include "sip/defs.h"

#include "log.h"

#include <memory>
using std::auto_ptr;

#define MULTIPART             "multipart"
#define MULTIPART_MIXED       "multipart/mixed"
#define MULTIPART_ALTERNATIVE "multipart/alternative"

#define BOUNDARY_str "boundary"
#define BOUNDARY_len (sizeof(BOUNDARY_str)-/*0-term*/1)

AmMimeBody::AmMimeBody()
  : mp_boundary(NULL),
    content_len(0),
    payload(NULL)
{
}

AmMimeBody::~AmMimeBody()
{
  clearCTParams();
  clearParts();
  clearPayload();
}

void AmMimeBody::clearCTParams()
{
  while(!ct_params.empty()){
    delete ct_params.front();
    ct_params.pop_front();
  }
}

void AmMimeBody::clearParts()
{
  while(!parts.empty()){
    delete parts.front();
    parts.pop_front();
  }
}

void AmMimeBody::clearPayload()
{
  delete [] payload;
  payload = NULL;  
}

int AmMimeBody::parseCT(const string& ct)
{
  enum {
    CT_TYPE=0,
    CT_SLASH_SWS,
    CT_SUBTYPE_SWS,
    CT_SUBTYPE
  };

  const char* c = ct.c_str();
  const char* beg = c;
  const char* end = c + ct.length();

  int st = CT_TYPE;
  int saved_st = 0;

  for(;c < end; c++) {
    switch(st){

    case CT_TYPE:
      switch(*c) {
      case_CR_LF;
      case SP:
      case HTAB:
	ct_type = string(beg,c-beg);
	st = CT_SLASH_SWS;
	break;
	
      case SLASH:
	ct_type = string(beg,c-beg);
	st = CT_SUBTYPE_SWS;
	break;
      }
      break;

    case CT_SLASH_SWS:
      switch(*c){
      case_CR_LF;
      case SP:
      case HTAB:
	break;

      case SLASH:
	st = CT_SUBTYPE_SWS;
	break;

      default:
	DBG("Missing '/' after media type in 'Content-Type' hdr field\n");
	return -1;
      }
      break;

    case CT_SUBTYPE_SWS:
      switch(*c){
      case_CR_LF;
      case SP:
      case HTAB:
	break;

      default:
	st = CT_SUBTYPE;
	beg = c;
	break;
      }
      break;

    case CT_SUBTYPE:
      switch(*c){

      case_CR_LF;

      case SP:
      case HTAB:
      case SEMICOLON:
	ct_subtype = string(beg,c-beg);
	return parseCTParams(c,end);
      }
      break;

    case_ST_CR(*c);
    case ST_LF:
    case ST_CRLF:
      switch(saved_st){
      case CT_TYPE:
	if(!IS_WSP(*c)){
	  // should not happen: parse_headers() should already 
	  //                    have triggered an error
	  DBG("Malformed Content-Type value: <%.*s>\n",(int)(end-beg),beg);
	  return -1;
	}
	else {
	  ct_type = string(beg,(c-(st==ST_CRLF?2:1))-beg);
	  saved_st = CT_SLASH_SWS;
	}
	break;

      case CT_SLASH_SWS:
      case CT_SUBTYPE_SWS:
	if(!IS_WSP(*c)){
	  // should not happen: parse_headers() should already 
	  //                    have triggered an error
	  DBG("Malformed Content-Type value: <%.*s>\n",(int)(end-beg),beg);
	  return -1;
	}
	break;

      case CT_SUBTYPE:
	ct_subtype = string(beg,(c-(st==ST_CRLF?2:1))-beg);
	if(!IS_WSP(*c)){
	  // should not happen: parse_headers() should already 
	  //                    have triggered an error
	  return 0;
	}
	return parseCTParams(c,end);
      }
      
      st = saved_st;
      break;
    }
  }

  // end-of-string
  switch(st){
  case CT_TYPE:
  case CT_SLASH_SWS:
  case CT_SUBTYPE_SWS:
    DBG("Malformed Content-Type value: <%.*s>\n",(int)(end-beg),beg);
    return -1;
    
  case CT_SUBTYPE:
    ct_subtype = string(beg,c-beg);
    break;
  }
  
  return 0;
}

int  AmMimeBody::parseCTParams(const char* c, const char* end)
{
  list<sip_avp*> params;
  if(parse_gen_params(&params, &c, end-c, '\0') < 0) {
    if(!params.empty()) free_gen_params(&params);
    return -1;
  }
  
  for(list<sip_avp*>::iterator it_ct_param = params.begin();
      it_ct_param != params.end();++it_ct_param) {

    DBG("parsed new content-type parameter: <%.*s>=<%.*s>",
	(*it_ct_param)->name.len,(*it_ct_param)->name.s,
	(*it_ct_param)->value.len,(*it_ct_param)->value.s);

    CTParam* p = new CTParam(c2stlstr((*it_ct_param)->name),
 			     c2stlstr((*it_ct_param)->value));

    if(parseCTParamType(p)) {
      free_gen_params(&params);
      delete p;
      return -1;
    }
    ct_params.push_back(p);
  }

  free_gen_params(&params);
  return 0;
}

int AmMimeBody::parseCTParamType(CTParam* p)
{
  const char* c = p->name.c_str();
  unsigned  len = p->name.length();
  
  switch(len){
  case BOUNDARY_len:
    if(!lower_cmp(c,BOUNDARY_str,len)){
      if(p->value.empty()) {
	DBG("Content-Type boundary parameter is missing a value\n");
	return -1;
      }
      p->type = CTParam::BOUNDARY;
      mp_boundary = p;
    }
    else p->type = CTParam::OTHER;
    break;
  default:
    p->type = CTParam::OTHER;
    break;
  }

  return 0;
}

int AmMimeBody::findNextBoundary(unsigned char** beg, unsigned char** end)
{
  enum {
    B_START=0,
    B_CR,
    B_LF,
    B_HYPHEN,
    B_BOUNDARY,
    B_HYPHEN2,
    B_HYPHEN3,
    B_CR2,
    B_LF2,
    B_MATCHED
  };

  if(!mp_boundary)
    return -1;

  unsigned char* c = *beg;
  unsigned char* b = (unsigned char*)mp_boundary->value.c_str();
  unsigned char* b_end = b + mp_boundary->value.length();

  int st=B_START;

  // Allow the buffer to start directly 
  // with the boundary delimiter
  if(*c == HYPHEN)
    st=B_LF;

#define case_BOUNDARY(_st_,_ch_,_st1_)		\
               case _st_:			\
		 switch(*c){			\
		 case _ch_:			\
		   st = _st1_;			\
		   break;			\
		 default:			\
		   st = B_START;		\
		   break;			\
		 }				\
		 break

#define case_BOUNDARY2(_st_,_ch_,_st1_)		\
               case _st_:			\
		 switch(*c){			\
		 case _ch_:			\
		   st = _st1_;			\
		   break;				\
		 default:				\
		   b = (unsigned char*)			\
		     mp_boundary->value.c_str();	\
		   is_final = false;			\
		   st = B_START;			\
		   break;				\
		 }					\
		 break


  bool is_final = false;

  for(;st != B_MATCHED && (c < *end-(b_end-b)); c++){

    switch(st){
    case B_START: 
      if(*c == CR) {
	*beg = c;
	st = B_CR;
      }
      break;

    case_BOUNDARY(B_CR,         LF, B_LF);

    case B_LF:
      switch(*c){
      case HYPHEN:
	st = B_HYPHEN;
	break;
      case CR:
	st = B_CR;
	break;
      default:
	st = B_START;
	break;
      }
      break;

    case_BOUNDARY(B_HYPHEN, HYPHEN, B_BOUNDARY);

    case B_BOUNDARY:
      if(*c == *b) {
	if(++b == b_end) {
	  // reached end boundary buffer
	  st = B_HYPHEN2;
	}
      }
      else {
	b = (unsigned char*)mp_boundary->value.c_str();
	st = B_START;
      }
      break;

    case B_HYPHEN2:
      switch(*c) {
      case HYPHEN: 
	is_final = true; 
	st = B_HYPHEN3; 
	break;

      case CR:
	st = B_LF2;
	break;

      default:
	b = (unsigned char*)mp_boundary->value.c_str();
	st = B_START;
	break;
      }
      break;

    case_BOUNDARY2(B_HYPHEN3, HYPHEN, B_CR2);
    case_BOUNDARY2(B_CR2,         CR, B_LF2);
    case_BOUNDARY2(B_LF2,         LF, B_MATCHED);
    }
  }

  if(st == B_MATCHED || (st == B_HYPHEN3) || (st == B_CR2) || (st == B_LF2)) {
    *end = c;
    return is_final ? 1 : 0;
  }

  return -1;
}

int AmMimeBody::parseSinglePart(unsigned char* buf, unsigned int len)
{
  list<sip_header*> hdrs;
  char* c = (char*)buf;
  char* end = c + len;

  // parse headers first
  if(parse_headers(hdrs,&c,c+len) < 0) {
    DBG("could not parse part headers\n");
    free_headers(hdrs);
    return -1;
  }

  auto_ptr<AmMimeBody> sub_part(new AmMimeBody());

  string sub_part_hdrs;
  string sub_part_ct;
  for(list<sip_header*>::iterator it = hdrs.begin();
      it != hdrs.end(); ++it) {

    DBG("Part header: <%.*s>: <%.*s>\n",
	(*it)->name.len,(*it)->name.s,
	(*it)->value.len,(*it)->value.s);

    if((*it)->type == sip_header::H_CONTENT_TYPE) {
      sub_part_ct = c2stlstr((*it)->value);
    }
    else {
      sub_part_hdrs += c2stlstr((*it)->name) + COLSP
	+ c2stlstr((*it)->value) + CRLF;
    }
  }

  if(!sub_part_ct.empty() &&
     (sub_part->parse(sub_part_ct,(unsigned char*)c,end-c) == 0)) {
    // TODO: check errors
    DBG("Successfully parsed subpart.\n");
  }
  else {
    DBG("Either no Content-Type, or subpart parsing failed.\n");
    sub_part->setCTType("");
    sub_part->setCTSubType("");
    sub_part->setPayload((unsigned char*)c,end-c);
  }

  sub_part->setHeaders(sub_part_hdrs);
  parts.push_back(sub_part.release());

  free_headers(hdrs);
  return 0;
}

int AmMimeBody::parseMultipart(unsigned char* buf, unsigned int len)
{
  if(!mp_boundary) {
    DBG("boundary parameter missing in a multipart MIME body\n");
    return -1;
  }

  unsigned char* buf_end   = buf + len;
  unsigned char* part_end  = buf;
  unsigned char* next_part = buf_end;

  int err = findNextBoundary(&part_end,&next_part);
  if(err < 0) {
    DBG("unexpected end-of-buffer\n");
    return -1;
  }

  unsigned char* part_beg  = NULL;
  do {
    part_beg  = next_part;
    part_end  = part_beg;
    next_part = buf_end;

    err = findNextBoundary(&part_end,&next_part);
    if(err < 0) {
      DBG("unexpected end-of-buffer while searching for MIME body boundary\n");
      return -1;
    }
    
    if(parseSinglePart(part_beg,part_end-part_beg) < 0) {
      DBG("Failed parsing part\n");
    }
    else {
      AmMimeBody* part = parts.back();
      DBG("Added new part:\n%.*s\n",
	  part->content_len,part->payload);
    }
    
  } while(!err);

  DBG("End-of-multipart body found\n");

  return 0;
}

int AmMimeBody::parse(const string& ct, unsigned char* buf, unsigned int len)
{
  if(parseCT(ct) < 0)
    return -1;
  
  if( !lower_cmp_n(ct_type.c_str(),ct_type.length(),
		   MULTIPART,sizeof(MULTIPART)-1) ) {

    DBG("parsing multi-part body\n");
    return parseMultipart(buf,len);
  }
  else {
    DBG("saving single-part body\n");
    setPayload(buf,len);
  }

  return 0;
}

void AmMimeBody::setCTType(const string& type)
{
  ct_type = type;
}

void AmMimeBody::setCTSubType(const string& subtype)
{
  ct_subtype = subtype;
}

void AmMimeBody::setHeaders(const string& hdrs)
{
  this->hdrs = hdrs;
}

void AmMimeBody::setPayload(unsigned char* buf, unsigned int len)
{
  payload = new unsigned char [len];
  memcpy(payload,buf,len);
  content_len = len;
}
