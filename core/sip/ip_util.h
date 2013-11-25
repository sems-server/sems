/*
 * Copyright (C) 2012 Raphael Coeffic
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
#ifndef _ip_util_h_
#define _ip_util_h_

#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string>
using std::string;

#define SAv4(addr) \
            ((struct sockaddr_in*)addr)

#define SAv6(addr) \
            ((struct sockaddr_in6*)addr)

#define SA_len(addr) \
            ((addr)->ss_family == AF_INET ?				\
	     sizeof(sockaddr_in) : sizeof(sockaddr_in6))

/**
 * Fill the sockaddr_storage structure based 
 * on the address given in 'src'.
 * @param src string representation of the IP address.
 * @param dst address stucture
 * @return 1 on success, 0 if address is not valid
 */
int am_inet_pton(const char* src, struct sockaddr_storage* dst);

/**
 * Print a string representation of the IP address in 'addr'.
 *
 * @param str buffer for the result string.
 * @param size size of the result string buffer.
 * @return NULL if failed, result string otherwise.
 */
const char* am_inet_ntop(const sockaddr_storage* addr, char* str, size_t size);

/**
 * Print a string representation of the IP address in 'addr'.
 *
 * @return empty string if failed, result string otherwise.
 */
string am_inet_ntop(const sockaddr_storage* addr);

/**
 * Print a string representation of the IP address in 'addr'.
 * IPv6 addresses are surrounded by '[' and ']', as needed for SIP header fields.
 *
 * @param str buffer for the result string.
 * @param size size of the result string buffer.
 * @return NULL if failed, result string otherwise
 */
const char* am_inet_ntop_sip(const sockaddr_storage* addr, char* str, size_t size);

void  am_set_port(struct sockaddr_storage* addr, short port);
unsigned short am_get_port(const sockaddr_storage* addr);

#endif
