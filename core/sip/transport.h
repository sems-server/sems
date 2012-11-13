/*
 * $Id: transport.h 1048 2008-07-15 18:48:07Z sayer $
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
#ifndef _transport_h_
#define _transport_h_

#include "../AmThread.h"
#include <sys/socket.h>

#include <string>
using std::string;

class trsp_socket
{
public:
    enum socket_options {
	force_via_address = (1 << 0),
	force_outbound_if = (1 << 1)
    };

    static int log_level_raw_msgs;
    
protected:
    // socket descriptor
    int sd;

    // bound address
    sockaddr_storage addr;

    // bound IP
    string           ip;

    // bound port number
    unsigned short   port;

    // internal interface number
    unsigned short   if_num;

    // network interface index
    unsigned int sys_if_idx;

    // ORed field of socket_option
    unsigned int socket_options;

public:
    trsp_socket(unsigned short if_num, unsigned int opts,
		unsigned int sys_if_idx = 0);
    virtual ~trsp_socket();

    /**
     * Binds the transport socket to an address
     * @return -1 if error(s) occured.
     */
    virtual int bind(const string& address, unsigned short port)=0;

    /**
     * Getter for IP address
     */
    const char* get_ip();
    
    /**
     * Getter for the port number
     */
    unsigned short get_port();

    /**
     *  Getter for the socket descriptor
     */
    int get_sd();

    /**
     * Getter for the interface number
     */
    unsigned short get_if();

    /**
     * Checks for socket options
     */
    bool is_opt_set(unsigned int mask);

    /**
     * Copy the internal address into the given one (sa).
     */
    void copy_addr_to(sockaddr_storage* sa);

    /**
     * Match with the given address
     * @return true if address matches
     */
    bool match_addr(sockaddr_storage* other_addr);

    /**
     * Sends a message.
     * @return -1 if error(s) occured.
     */
    virtual int send(const sockaddr_storage* sa, const char* msg, const int msg_len)=0;
};

class transport: public AmThread
{
protected:
    trsp_socket* sock;

public:
    transport(trsp_socket* sock): sock(sock) {}
    virtual ~transport();
};

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
