#ifndef _parse_next_hop_h_
#define _parse_next_hop_h_

#include "cstring.h"

#include <list>
using std::list;

struct host_port
{
  cstring        host;
  unsigned short port;
};

int parse_next_hop(const cstring& next_hop,
		   list<host_port>& dest_list);

#endif
