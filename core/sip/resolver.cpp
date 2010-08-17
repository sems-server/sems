/*
 * $Id: resolver.cpp 1048 2008-07-15 18:48:07Z sayer $
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "resolver.h"

#include <netdb.h>
#include <string.h>

#include "log.h"

#include <sys/socket.h> 
#include <netinet/in.h>

_resolver::_resolver()
{
    
}

_resolver::~_resolver()
{
    
}

int _resolver::resolve_name(const char* name, sockaddr_storage* sa, 
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

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
