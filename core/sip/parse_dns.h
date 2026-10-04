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

typedef int (*dns_parse_fct)(dns_record* rr, dns_section_type t, unsigned char* begin, unsigned char* end, void* data);

int dns_msg_parse(unsigned char* msg, int len, dns_parse_fct fct, void* data);
int dns_expand_name(unsigned char** ptr, unsigned char* begin, unsigned char* end, 
		    unsigned char* buf, unsigned int len);

// The fields of a DNS message follow variable-length names, so 'p' is not
// necessarily aligned: assemble the value byte by byte (network byte order)
// rather than loading it through a uint16_t/uint32_t pointer.
inline uint16_t dns_get_16(const unsigned char* p)
{
  return (uint16_t)((p[0] << 8) | p[1]);
}

inline uint32_t dns_get_32(const unsigned char* p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
    | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

#endif
