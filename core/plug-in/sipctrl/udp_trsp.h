/*
 * $Id: $
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
#ifndef _udp_trsp_h_
#define _udp_trsp_h_

#include "transport.h"

/**
 * Maximum message length for UDP
 * not including terminating '\0'
 */
#define MAX_UDP_MSGLEN 65535

#include <sys/socket.h>

#include <string>
using std::string;

class udp_trsp: public transport
{
    // socket descriptor
    int sd;

    // bound port number
    unsigned short local_port;

    // bound IP
    string local_ip;

    // bound address
    sockaddr_storage local_addr;

 protected:
    /** @see AmThread */
    void run();
    /** @see AmThread */
    void on_stop();

 public:
    /** @see transport */
    udp_trsp(trans_layer* tl);
    ~udp_trsp();

    /** @see transport */
    int bind(const string& address, unsigned short port);

    /** @see transport */
    int send(const sockaddr_storage* sa, const char* msg, const int msg_len);

    const char* get_local_ip();
    unsigned short get_local_port();
    void copy_local_addr(sockaddr_storage* sa);
};

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
