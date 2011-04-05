#include "parse_route.h"
#include "parse_from_to.h"
#include "parse_common.h"

#include <memory>
using std::auto_ptr;

route_elmt::~route_elmt() {
  if(addr) delete addr;
}

sip_route::~sip_route()
{
  for(list<route_elmt*>::iterator it = elmts.begin();
      it != elmts.end(); ++it)
    delete *it;
}

bool is_loose_route(const sip_uri* fr_uri)
{
    bool is_lr = false;

    if(!fr_uri->params.empty()){
	
	list<sip_avp*>::const_iterator it = fr_uri->params.begin();
	for(;it != fr_uri->params.end(); it++){
	    
	    if( ((*it)->name.len == 2) && 
		(!memcmp((*it)->name.s,"lr",2)) ) {
		
		is_lr = true;
		break;
	    }
	}	
    }

    return is_lr;
}

static int skip_2_next_route(const char*& c, 
			     const char*& eor,
			     const char*  end)
{
    assert(c && end && (c<=end));

    // detect beginning of next route
    enum {
	RR_BEGIN=0,
	RR_QUOTED,
	RR_SWS,
	RR_SEP_SWS,  // space(s) after ','
	RR_NXT_ROUTE
    };

    int st = RR_BEGIN;
    eor = NULL;
    for(;c<end;c++){
		
	switch(st){
	case RR_BEGIN:
	    switch(*c){
	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
	        st = RR_SWS;
	        eor = c;
		break;
	    case COMMA:
		st = RR_SEP_SWS;
		eor = c;
		break;
	    case DQUOTE:
		st = RR_QUOTED;
		break;
	    }
	    break;
	case RR_QUOTED:
	    switch(*c){
	    case BACKSLASH:
		if(++c == end) goto error;
		break;
	    case DQUOTE:
		st = RR_BEGIN;
		break;
	    }
	    break;
	case RR_SWS:
	    switch(*c){
	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
		break;
	    case COMMA:
		st = RR_SEP_SWS;
		break;
	    default:
		st = RR_BEGIN;
		eor = NULL;
		break;
	    }
	    break;
	case RR_SEP_SWS:
	    switch(*c){
	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
		break;
	    default:
		st = RR_NXT_ROUTE;
		goto nxt_route;
	    }
	    break;
	}
    }

 nxt_route:
 error:
	    
    switch(st){
    case RR_QUOTED:
	DBG("Malformed route header\n");
	return -1;

    case RR_SEP_SWS: // not fine, but acceptable
    case RR_BEGIN: // end of route header
        eor = c;
        return 0;

    case RR_NXT_ROUTE:
        return 1; // next route available
    }

    // should never be reached
    // makes GCC happy
    return 0;
}

int parse_route(sip_header* rh)
{
    if(rh->p) return 0;

    sip_route* route = new sip_route();
    rh->p = route;

    const char* c = rh->value.s;
    const char* end = rh->value.s + rh->value.len;
    const char* eor = NULL;

    while(c < end) {

      const char* route_begin = c;
      int err = skip_2_next_route(c,eor,end);
      if(err < 0){
	ERROR("While parsing route header\n");
	return -1;
      }

      if(eor) {
	route_elmt* re = new route_elmt();
	re->route.s = route_begin;
	re->route.len = eor - route_begin;
	route->elmts.push_back(re);
      }

      if(err == 0)
	break;
    }

    return 0;
}

int parse_first_route_uri(sip_header* fr)
{
    if(parse_route(fr) < 0) {
        DBG("Could not parse route hf [%.*s]\n",
	    fr->value.len,fr->value.s);
        return -1;
    }
    
    sip_route* route = (sip_route*)fr->p;
    assert(route);

    if(route->elmts.empty()) {
      
        DBG("No first route\n");
	return -1;
    }

    list<route_elmt*>::iterator route_it = route->elmts.begin();
    if((*route_it)->addr)
      return 0;

    cstring route_str((*route_it)->route);
    const char* c = route_str.s;

    auto_ptr<sip_nameaddr> na(new sip_nameaddr());
    if(parse_nameaddr(na.get(), &c, route_str.len)<0) {
      
      DBG("Parsing name-addr failed\n");
      return -1;
    }
    
    if(parse_uri(&na->uri,na->addr.s,na->addr.len) < 0) {
	
	DBG("Parsing route uri failed\n");
	return -1;
    }

    (*route_it)->addr = na.release();

    return 0;
}

sip_uri* get_first_route_uri(sip_header* fr)
{
  int err=0;

  assert(fr);
  err = parse_first_route_uri(fr);
  if(err < 0)
    return NULL;
    
  sip_route* route = (sip_route*)(fr->p);
  if(!route || route->elmts.empty()){
    DBG("No first route\n");
    return NULL;
  }

  route_elmt* re = route->elmts.front();
  sip_nameaddr* na = re->addr;

  return &(na->uri);
}

