#include "parse_route.h"
#include "parse_from_to.h"
#include "parse_common.h"

#include <memory>
using std::auto_ptr;

sip_route::~sip_route()
{
  for(list<sip_nameaddr*>::iterator it = elmts.begin();
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

int parse_first_route_uri(sip_header* fr)
{
    if(fr->p) return 0;
    
    auto_ptr<sip_nameaddr> na(new sip_nameaddr());
    const char* c = fr->value.s;

    if(parse_nameaddr(na.get(), &c, fr->value.len)<0) {
	
	DBG("Parsing name-addr failed\n");
	return -1;
    }
    
    if(parse_uri(&na->uri,na->addr.s,na->addr.len) < 0) {
	
	DBG("Parsing route uri failed\n");
	return -1;
    }

    fr->p = new sip_route();
    ((sip_route*)(fr->p))->elmts.push_back(na.release());

    return 0;
}

sip_uri* get_first_route_uri(sip_header* fr)
{
  int err=0;

  assert(fr);
  if(!fr->p) err = parse_first_route_uri(fr);
  if(err || ((sip_route*)(fr->p))->elmts.empty())
    return NULL;
    
  sip_nameaddr* na = ((sip_route*)(fr->p))->elmts.front();
  assert(na);

  return &(na->uri);
}

