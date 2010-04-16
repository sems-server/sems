/*
 * $Id: transport.cpp 1048 2008-07-15 18:48:07Z sayer $
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
#include "transport.h"
#include "../SipCtrlInterface.h"
#include "../log.h"

#include <assert.h>
#include <netinet/in.h>
#include <string.h> // memset, strerror, ...

trsp_socket::trsp_socket()
    : sd(0), ip(), port(0)
{
    memset(&addr,0,sizeof(sockaddr_storage));
}

trsp_socket::~trsp_socket()
{
}

const char* trsp_socket::get_ip()
{
    return ip.c_str();
}

unsigned short trsp_socket::get_port()
{
    return port;
}

void trsp_socket::copy_addr_to(sockaddr_storage* sa)
{
    memcpy(sa,&addr,sizeof(sockaddr_storage));
}

int trsp_socket::get_sd()
{
    return sd;
}

int trsp_socket::send(const sockaddr_storage* sa, const char* msg, const int msg_len)
{
    if ((SipCtrlInterface::log_raw_messages >= 0)
	&& (SipCtrlInterface::log_raw_messages <= log_level)) {
	_LOG(SipCtrlInterface::log_raw_messages, 
	     "send  msg\n--++--\n%.*s--++--\n", msg_len, msg);
    }

  int err;
#ifdef SUPPORT_IPV6
  if (sa->ss_family == AF_INET6) {
    err = sendto(sd, msg, msg_len, 0, (const struct sockaddr*)sa, sizeof(sockaddr_in6));
  }
  else {
#endif
    err = sendto(sd, msg, msg_len, 0, (const struct sockaddr*)sa, sizeof(sockaddr_in));
#ifdef SUPPORT_IPV6
  }
#endif

  if (err < 0) {
    ERROR("sendto: %s\n",strerror(errno));
    return err;
  }
  else if (err != msg_len) {
    ERROR("sendto: sent %i instead of %i bytes\n", err, msg_len);
    return -1;
  }

  return 0;
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
