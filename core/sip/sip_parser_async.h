#ifndef _sip_parser_async_h_
#define _sip_parser_async_h_

#include "parse_header.h"

struct parser_state
{
  char* orig_buf;
  char* c; // cursor
  char* beg; // last marker for field start

  int stage;
  int st; // parser state (within stage)
  int saved_st; // saved parser state (within stage)
  sip_header hdr; // temporary header struct
  
  int content_len; // detected body content-length

  parser_state()
    : orig_buf(NULL),c(NULL),beg(NULL),
      stage(0),st(0),saved_st(0),
      content_len(0)
  {}

  void reset(char* buf) {
    c = orig_buf = buf;
    reset_hdr_parser();
    stage = content_len = 0;
  }

  void reset_hdr_parser() {
    memset(&hdr,0,sizeof(sip_header));
    st = saved_st = 0;
    beg = c;
  }
};

int skip_sip_msg_async(parser_state* pst, char* end);

#endif
