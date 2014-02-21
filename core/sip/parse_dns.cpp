#include "parse_dns.h"
#include <string.h>

#include "log.h"

#define SECTION_COUNTS_OFF 4
#define HEADER_OFFSET      12

unsigned short dns_msg_count(u_char* begin, dns_section_type sect);
int dns_skip_name(u_char** p, u_char* end);
int dns_expand_name(u_char** ptr, u_char* begin, u_char* end, 
		    u_char* buf, unsigned int len);


const char* dns_rr_type_str(dns_rr_type t)
{
  switch(t) {
  case dns_r_a:     return "A";
  case dns_r_ns:    return "NS";
  case dns_r_cname: return "CNAME";
  case dns_r_aaaa:  return "AAAA";
  case dns_r_srv:   return "SRV";
  case dns_r_naptr: return "NAPTR";
  default:          return "UNKNOWN";
  };
}



int dns_msg_parse(u_char* msg, int len, dns_parse_fct fct, void* data)
{
  u_char* begin = msg;
  u_char* p = begin + HEADER_OFFSET;
  u_char* end = msg + len;

  if(p >= end) return -1;

  // skip query section
  for(int i=0; i<dns_msg_count(begin,dns_s_qd); i++){
    // query name
    if(dns_skip_name(&p,end) < 0) return -1;
    // skip query type+class
    if((p += 4) > end) return -1;
  }
  
  dns_record rr;
  for(int s = (int)dns_s_an; s < (int)__dns_max_sections; ++s){
    for(int i=0; i<dns_msg_count(begin,(dns_section_type)s); i++){

      // expand name
      if(dns_expand_name(&p,begin,end,(u_char*)rr.name,NS_MAXDNAME) < 0) return -1;

      // at least 8 bytes for type+class+ttl left?
      if((p + 8) > end) return -1;
      
      rr.type = dns_get_16(p);
      p += 2;

      rr.rr_class = dns_get_16(p);
      p += 2;

      rr.ttl = dns_get_32(p);
      p+= 4;

      // fetch rdata len
      if(p+2 > end) return -1;
      rr.rdata_len = *(p++) << 8;
      rr.rdata_len |= *(p++);
      rr.rdata = p;

      // skip rdata
      if((p += rr.rdata_len) > end) return -1;

      // call provided function
      if(fct && (*fct)(&rr,(dns_section_type)s,begin,end,data)) return -1;
    }
  }

  return 0;
}

unsigned short dns_msg_count(u_char* begin, dns_section_type sect)
{
  u_char* p = begin + SECTION_COUNTS_OFF + 2*sect;

  return ((u_short)*p)<<8 | ((u_short)*(p+1));
}

int dns_skip_name(u_char** p, u_char* end)
{
  while(*p < end) {
    
    if(!**p) { // zero label
      if(++(*p) < end)	return 0;
      return -1;
    }
    else if(**p & 0xC0){ // ptr
      if((*p += 2) < end) return 0;
      return -1;
    }
    else { // label
      *p += **p+1;
    }
  }

  return -1;
}

int dns_expand_name(u_char** ptr, u_char* begin, u_char* end, 
		    u_char* start_buf, unsigned int len)
{
  u_char* buf = start_buf;
  u_char* p = *ptr;
  bool    is_ptr=false;

  while(p < end) {
    
    if(!*p) { // reached the end of a label
      if(len){ 
	*buf = '\0'; // zero-term
	if(!is_ptr){ *ptr = p+1; }
	return (buf-start_buf); 
      }
      return -1;
    }
    
    if( (*p & 0xC0) == 0xC0 ){ // ptr

      unsigned short l_off = (((unsigned short)*p & 0x3F) << 8);
      if(++p >= end) return -1;
      l_off |= *p;
      if(++p >= end) return -1;

      if(begin + l_off + 1 >= end) return -1;

      if(!is_ptr){
	*ptr = p;
	is_ptr = true;
      }
      p = begin + l_off;
      continue;
    }

    if( (*p & 0x3F) != *p ){ // NOT a label
      return -1;
    }

    if(p + *p + 1 >= end) return -1;
    if(len <= *p) return -1;
    
    memcpy(buf,p+1,*p);
    len -= *p;
    buf += *p;
    p += *p + 1;

    if(*p){
      if(!(--len)) return -1;
      *(buf++) = '.';
    }
  }

  return -1;
}

