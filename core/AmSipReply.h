#ifndef _AmReply_h_
#define _AmReply_h_

#include "AmEventQueue.h"

#include <string>
using std::string;

/** \brief represents a SIP reply */
struct AmSipReply
{
  unsigned int code;
  string       reason;
  string       next_request_uri;
  string       next_hop;
  string       route;
  string       hdrs;
  string       body;

  // Parsed from the hdrs
  // string       callid;
  string       remote_tag;
  string       local_tag;
  unsigned int cseq;

  AmSipReply();
};

#endif
