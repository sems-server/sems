#include <stdio.h>
#include "parse_next_hop.h"
#include "parse_common.h"
#include "log.h"

int parse_next_hop(const cstring& next_hop,
		   list<sip_destination>& dest_list)
{
  enum {
    IPL_BEG=0,
    IPL_HOST,
    IPL_V6,
    IPL_HOST_SEP,
    IPL_PORT,
    IPL_TRSP
  };

  int st = IPL_BEG;

  const char* c = next_hop.s;
  const char* end = c + next_hop.len;
  const char* beg = NULL;

  sip_destination dest;

  for(;c<end; c++) {

    switch(st){

    case IPL_BEG:
      switch(*c){
      case SP:
      case HTAB:
	continue;
      default:
	beg = c;
	st = IPL_HOST;
	dest.host.clear();
	dest.port = 0;
	break;
      }
      // fall-through trap
    case IPL_HOST:
      switch(*c){
      case '[':
	st = IPL_V6;
	beg = c+1;
	break;
      case ':':
	st = IPL_PORT;
	dest.host.set(beg,c-beg);
	break;
      case '/':
	st = IPL_TRSP;
	dest.host.set(beg,c-beg);
	beg = c+1;
	break;
      case ',':
	st = IPL_BEG;
	dest.host.set(beg,c-beg);
	dest_list.push_back(dest);
	break;
      case SP:
      case HTAB:
	st = IPL_HOST_SEP;
	dest.host.set(beg,c-beg);
	break;
      default:
	break;
      }
      break;

    case IPL_V6:
      switch(*c){
      case ']':
	st = IPL_HOST_SEP;
	dest.host.set(beg,c-beg);
	break;
      default:
	break;
      }
      break;

    case IPL_HOST_SEP:
      switch(*c){
      case ':':
	st = IPL_PORT;
	break;
      case ',':
	st = IPL_BEG;
	dest_list.push_back(dest);
	break;
      case '/':
	st = IPL_TRSP;
	beg = c+1;
	break;
      default:
	// syntax error
	DBG("error: unexpected character '%c' in IPL_HOST_SEP state.\n",*c);
	return -1;
      }
      break;

    case IPL_PORT:
      switch(*c){
      case '/':
	st = IPL_TRSP;
	beg = c+1;
	break;
      case ',':
	st = IPL_BEG;
	dest_list.push_back(dest);
	break;
      case SP:
      case HTAB:
	break;
      default:
	if(*c < '0' && *c > '9'){
	  DBG("error: unexpected character '%c' in IPL_PORT state.\n",*c);
	  return -1;
	}
	dest.port = dest.port*10 + (*c - '0');
	break;
      }
      break;


    case IPL_TRSP:
      switch(*c){
      case ',':
	st = IPL_BEG;
	dest.trsp.set(beg,c-beg);
	dest_list.push_back(dest);
	break;
      default:
	if( (*c >= 'a' && *c <= 'z') ||
	    (*c >= 'A' && *c <= 'Z') ) {
	  continue;
	}
	// syntax error
	DBG("error: unexpected character '%c' in IPL_TRSP state.\n",*c);
	return -1;
      }
      break;
    }
  }

  switch(st){
  case IPL_BEG:
    // no host at all
    // possibly, the string was empty
    break;
  case IPL_HOST:
    dest.host.set(beg,c-beg);
    dest_list.push_back(dest);
    break;
  case IPL_V6:
  case IPL_HOST_SEP:
  case IPL_PORT:
    dest_list.push_back(dest);
    break;
  case IPL_TRSP:
    dest.trsp.set(beg,c-beg);
    dest_list.push_back(dest);
    break;
  }
  
  return 0;
}
