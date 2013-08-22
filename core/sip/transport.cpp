/*
 * $Id: transport.cpp 1048 2008-07-15 18:48:07Z sayer $
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "transport.h"
#include "../log.h"

#include <assert.h>
#include <netinet/in.h>
#include <string.h> // memset, strerror, ...

int trsp_socket::log_level_raw_msgs = L_DBG;

trsp_socket::trsp_socket(unsigned short if_num, unsigned int opts,
			 unsigned int sys_if_idx, int sd)
    : sd(sd), ip(), port(0), 
      if_num(if_num), sys_if_idx(sys_if_idx),
      socket_options(opts)
{
    memset(&addr,0,sizeof(sockaddr_storage));
}

trsp_socket::~trsp_socket()
{
}

const char* trsp_socket::get_ip() const
{
    return ip.c_str();
}

unsigned short trsp_socket::get_port() const
{
    return port;
}

void trsp_socket::set_public_ip(const string& ip)
{
    public_ip = ip;
}
    
const char* trsp_socket::get_advertised_ip() const
{
    if(!public_ip.empty())
	return public_ip.c_str();

    return get_ip();
}

bool trsp_socket::is_opt_set(unsigned int mask) const
{
    DBG("trsp_socket::socket_options = 0x%x\n",socket_options);
    return (socket_options & mask) == mask;
}

void trsp_socket::copy_addr_to(sockaddr_storage* sa) const
{
    memcpy(sa,&addr,sizeof(sockaddr_storage));
}

/**
 * Match with the given address
 * @return true if address matches
 */
bool trsp_socket::match_addr(sockaddr_storage* other_addr) const
{
    
    if(addr.ss_family != other_addr->ss_family)
	return false;

    if(addr.ss_family == AF_INET){
	if( !memcmp(&((sockaddr_in*)&addr)->sin_addr, 
		    &((sockaddr_in*)other_addr)->sin_addr, 
		    sizeof(in_addr)) )
	    return true;
    }
    else if(addr.ss_family == AF_INET6) {
	if( !memcmp(&((sockaddr_in6*)&addr)->sin6_addr, 
		    &((sockaddr_in6*)other_addr)->sin6_addr, 
		    sizeof(in6_addr)) )
	    return true;
    }
    
    return false;
}

int trsp_socket::get_sd() const
{
    return sd;
}

unsigned short trsp_socket::get_if() const
{
    return if_num;
}

transport::~transport()
{
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
