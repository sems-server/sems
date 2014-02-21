#ifndef _parse_next_hop_h_
#define _parse_next_hop_h_

#include "cstring.h"

#include <list>
using std::list;

struct sip_destination
{
  cstring        host;
  unsigned short port;
  cstring        trsp;

  sip_destination()
    : host(), port(0), trsp()
  {}
};

int parse_next_hop(const cstring& next_hop,
		   list<sip_destination>& dest_list);

#endif
