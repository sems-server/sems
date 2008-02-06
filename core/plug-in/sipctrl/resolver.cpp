
#include "resolver.h"

#include <netdb.h>
#include <string.h>

#include "log.h"

resolver* resolver::_instance=0;

resolver::resolver()
{
    
}

resolver::~resolver()
{
    
}

resolver* resolver::instance()
{
    if(!_instance)
	_instance = new resolver();
    
    return _instance;
}
    
int resolver::resolve_name(const char* name, sockaddr_storage* sa, 
			   const address_type types, const proto_type protos)
{
    struct addrinfo hints,*res=0;
    memset(&hints,0,sizeof(hints));
    
    int err=0;
    if(types & IPv4){

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	
	err = getaddrinfo(name,NULL,&hints,&res);

	if(err){
	    switch(err){
	    case EAI_AGAIN:
	    case EAI_NONAME:
		ERROR("Could not resolve '%s'\n",name);
		break;
	    default:
		ERROR("getaddrinfo('%s'): %s\n",
		      name,gai_strerror(err));
		break;
	    }
	    
	    err = -1;
	}
	else {
	    memcpy(sa,res->ai_addr,res->ai_addrlen);
	    freeaddrinfo(res);
	}
    }
    return err;
}
