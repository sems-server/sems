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
  : content_len(0),
    payload(NULL)
{
}

AmMimeBody::AmMimeBody(const AmMimeBody& body)
  : ct(body.ct),
    hdrs(body.hdrs),
    content_len(0),
    payload(NULL)
{
  if(body.payload && body.content_len) {
    setPayload(body.payload,body.content_len);
  }
  
  for(Parts::const_iterator it = body.parts.begin();
      it != body.parts.end(); ++it) {
    parts.push_back(new AmMimeBody(**it));
  }
}

AmMimeBody::~AmMimeBody()
{
  clearParts();
  clearPayload();
}

const AmMimeBody& AmMimeBody::operator = (const AmMimeBody& r_body)
{
  ct = r_body.ct;
  hdrs = r_body.hdrs;

  if(r_body.payload && r_body.content_len) {
    setPayload(r_body.payload,r_body.content_len);
  }
  else {
    clearPayload();
  }

  clearParts();
  for(Parts::const_iterator it = r_body.parts.begin();
      it != r_body.parts.end(); ++it) {
    parts.push_back(new AmMimeBody(**it));
  }

  return r_body;
}

AmContentType::AmContentType()
  : mp_boundary(NULL)
{
}

AmContentType::AmContentType(const AmContentType& ct)
  : type(ct.type),
    subtype(ct.subtype),
    mp_boundary(NULL)
{
  for(Params::const_iterator it = ct.params.begin();
      it != ct.params.end(); ++it) {
    params.push_back(new Param(**it));
    if((*it)->type == Param::BOUNDARY)
      mp_boundary = params.back();
  }
}

AmContentType::~AmContentType()
{
  clearParams();
}

const AmContentType& AmContentType::operator = (const AmContentType& r_ct)
{
  type = r_ct.type;
  subtype = r_ct.subtype;

  clearParams();
  for(Params::const_iterator it = r_ct.params.begin();
      it != r_ct.params.end(); ++it) {
    params.push_back(new Param(**it));
    if((*it)->type == Param::BOUNDARY)
      mp_boundary = params.back();
  }

  return r_ct;
}

void AmContentType::clearParams()
{
  mp_boundary = NULL;
  while(!params.empty()){
    delete params.front();
    params.pop_front();
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

int AmContentType::parse(const string& ct)
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
	type = string(beg,c-beg);
	st = CT_SLASH_SWS;
	break;
	
      case SLASH:
	type = string(beg,c-beg);
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
	subtype = string(beg,c-beg);
	return parseParams(c,end);
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
	  type = string(beg,(c-(st==ST_CRLF?2:1))-beg);
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
	subtype = string(beg,(c-(st==ST_CRLF?2:1))-beg);
	if(!IS_WSP(*c)){
	  // should not happen: parse_headers() should already 
	  //                    have triggered an error
	  return 0;
	}
	return parseParams(c,end);
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
    subtype = string(beg,c-beg);
    break;
  }
  
  return 0;
}

int  AmContentType::parseParams(const char* c, const char* end)
{
  list<sip_avp*> avp_params;
  if(parse_gen_params(&avp_params, &c, end-c, '\0') < 0) {
    if(!avp_params.empty()) free_gen_params(&avp_params);
    return -1;
  }
  
  for(list<sip_avp*>::iterator it_ct_param = avp_params.begin();
      it_ct_param != avp_params.end();++it_ct_param) {

    DBG("parsed new content-type parameter: <%.*s>=<%.*s>",
	(*it_ct_param)->name.len,(*it_ct_param)->name.s,
	(*it_ct_param)->value.len,(*it_ct_param)->value.s);

    Param* p = new Param(c2stlstr((*it_ct_param)->name),
			 c2stlstr((*it_ct_param)->value));

    if(p->parseType()) {
      free_gen_params(&avp_params);
      delete p;
      return -1;
    }
    
    if(p->type == Param::BOUNDARY)
      mp_boundary = p;

    params.push_back(p);
  }

  free_gen_params(&avp_params);
  return 0;
}

int AmContentType::Param::parseType()
{
  const char* c = name.c_str();
  unsigned  len = name.length();
  
  switch(len){
  case BOUNDARY_len:
    if(!lower_cmp(c,BOUNDARY_str,len)){
      if(value.empty()) {
	DBG("Content-Type boundary parameter is missing a value\n");
	return -1;
      }
      type = Param::BOUNDARY;
    }
    else type = Param::OTHER;
    break;
  default:
    type = Param::OTHER;
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

  if(!ct.mp_boundary)
    return -1;

  unsigned char* c = *beg;
  unsigned char* b_ini = (unsigned char*)ct.mp_boundary->value.c_str();
  unsigned char* b = b_ini;
  unsigned char* b_end = b + ct.mp_boundary->value.length();

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
		   break;			\
		 default:			\
		   b = b_ini;			\
		   is_final = false;		\
		   st = B_START;		\
		   break;			\
		 }				\
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
	b = b_ini;
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
	b = b_ini;
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
    sub_part->ct.setType("");
    sub_part->ct.setSubType("");
    sub_part->setPayload((unsigned char*)c,end-c);
  }

  sub_part->setHeaders(sub_part_hdrs);
  parts.push_back(sub_part.release());

  free_headers(hdrs);
  return 0;
}

int AmMimeBody::parseMultipart(const unsigned char* buf, unsigned int len)
{
  if(!ct.mp_boundary) {
    DBG("boundary parameter missing in a multipart MIME body\n");
    return -1;
  }

  unsigned char* buf_end   = (unsigned char*)buf + len;
  unsigned char* part_end  = (unsigned char*)buf;
  unsigned char* next_part = (unsigned char*)buf_end;

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

int AmMimeBody::parse(const string& content_type,
		      const unsigned char* buf, 
		      unsigned int len)
{
  if(ct.parse(content_type) < 0)
    return -1;
  
  if(ct.isType(MULTIPART)) {

    DBG("parsing multi-part body\n");
    return parseMultipart(buf,len);
  }
  else {
    DBG("saving single-part body\n");
    setPayload(buf,len);
  }

  return 0;
}

void AmMimeBody::convertToMultipart()
{
  AmContentType n_ct;
  n_ct.parse(MULTIPART_MIXED); // never fails

  AmMimeBody* n_part = new AmMimeBody(*this);
  n_part->ct = ct;

  parts.push_back(n_part);
  ct = n_ct;
}

void AmContentType::setType(const string& t)
{
  type = t;
}

void AmContentType::setSubType(const string& st)
{
  subtype = st;
}

bool AmContentType::isType(const string& t) const
{
  return !lower_cmp_n(t.c_str(),t.length(),
		      type.c_str(),type.length());
}

bool AmContentType::isSubType(const string& st) const
{
  return !lower_cmp_n(st.c_str(),st.length(),
		      subtype.c_str(),subtype.length());
}


void AmMimeBody::setHeaders(const string& hdrs)
{
  this->hdrs = hdrs;
}

AmMimeBody* AmMimeBody::addPart(const string& content_type)
{
  AmMimeBody* body = NULL;
  if(ct.type.empty() && ct.subtype.empty()) {
    // fill *this* body
    if(ct.parse(content_type)) {
      DBG("could not parse content-type\n");
      return NULL;
    }
    
    body = this;
  }
  else if(!ct.isType(MULTIPART)) {
    // convert to multipart
    convertToMultipart();
    body = new AmMimeBody();
    if(body->ct.parse(content_type)) {
      DBG("parsing new content-type failed\n");
      delete body;
      return NULL;
    }

    // add new part
    parts.push_back(body);
  }
  
  return body;
}

void AmMimeBody::setPayload(const unsigned char* buf, unsigned int len)
{
  if(payload)
    clearPayload();

  payload = new unsigned char [len+1];
  memcpy(payload,buf,len);
  content_len = len;

  // zero-term for the SDP parser
  payload[len] = '\0';
}

bool AmMimeBody::empty() const
{
  return (!payload || !content_len)
    && parts.empty();
}

bool AmContentType::hasContentType(const string& content_type) const
{
  if(content_type.empty() && type.empty() && subtype.empty())
    return true;

  if(content_type.empty() != (type.empty() && subtype.empty()))
    return false;

  // Quick & dirty comparison, might not always be correct
  string cmp_ct = type + "/" + subtype;
  return !lower_cmp_n(cmp_ct.c_str(),cmp_ct.length(),
		      content_type.c_str(),content_type.length());
}

string AmContentType::getStr() const 
{
  if(type.empty() && subtype.empty())
    return "";

  return type + "/" + subtype; 
}

string AmContentType::getHdr() const
{
  string ct = getStr();
  if(ct.empty())
    return ct;

  for(Params::const_iterator it = params.begin();
      it != params.end(); ++it) {
    
    ct += ";" + (*it)->name + "=" + (*it)->value;
  }

  return ct;
}

bool AmMimeBody::isContentType(const string& content_type) const
{
  return ct.hasContentType(content_type);
}

AmMimeBody* AmMimeBody::hasContentType(const string& content_type)
{
  if(isContentType(content_type)) {
    return this;
  }
  else if(ct.isType(MULTIPART)) {
    for(Parts::iterator it = parts.begin();
	it != parts.end(); ++it) {
      
      if((*it)->hasContentType(content_type)) {
	return *it;
      }
    }
  }

  return NULL;
}

const AmMimeBody* AmMimeBody::hasContentType(const string& content_type) const
{
  if(isContentType(content_type)) {
    return this;
  }
  else if(ct.isType(MULTIPART)) {
    for(Parts::const_iterator it = parts.begin();
	it != parts.end(); ++it) {
      
      if((*it)->hasContentType(content_type)) {
	return *it;
      }
    }
  }

  return NULL;
}

void AmMimeBody::print(string& buf) const
{
  if(empty())
    return;

  if(content_len) {
    buf += string((const char*)payload,content_len);
  }
  else {
    for(Parts::const_iterator it = parts.begin();
	it != parts.end(); ++it) {

      buf += "--" + ct.mp_boundary->value + CRLF;
      buf += SIP_HDR_CONTENT_TYPE COLSP + (*it)->getCTHdr() + CRLF;
      buf += (*it)->hdrs + CRLF;
      (*it)->print(buf);
      buf += CRLF;
    }

    if(!parts.empty()) {
      buf += "--" + ct.mp_boundary->value + "--" CRLF;
    }
  }
}
