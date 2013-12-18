#ifndef _parse_dns_h_
#define _parse_dns_h_

#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <stdint.h>

enum dns_section_type {

  dns_s_qd=0, // question section
  dns_s_an,   // answer section
  dns_s_ns,   // authority section
  dns_s_ar,    // additional section
  __dns_max_sections
};

enum dns_rr_type {

  dns_r_a     = 1,
  dns_r_ns    = 2,
  dns_r_cname = 5,
  dns_r_aaaa  = 28,
  dns_r_srv   = 33,
  dns_r_naptr = 35
};

const char* dns_rr_type_str(dns_rr_type t);

struct dns_record
{
  char           name[NS_MAXDNAME];
  unsigned short type;
  unsigned short rr_class;
  unsigned int   ttl;

  unsigned short rdata_len;
  unsigned char* rdata;
};

class dns_entry;

typedef int (*dns_parse_fct)(dns_record* rr, dns_section_type t, u_char* begin, u_char* end, void* data);

int dns_msg_parse(u_char* msg, int len, dns_parse_fct fct, void* data);
int dns_expand_name(u_char** ptr, u_char* begin, u_char* end, 
		    u_char* buf, unsigned int len);

inline uint16_t dns_get_16(const u_char* p)
{
  return ntohs(*(uint16_t*)p);
}

inline uint32_t dns_get_32(const u_char* p)
{
  return ntohl(*(uint32_t*)p);
}

#endif
