/*
 * $Id$
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

#ifndef _SIP_PARSER_H
#define _SIP_PARSER_H

#include "cstring.h"
#include "parse_uri.h"

#include <list>
using std::list;

#include <netinet/in.h>

struct sip_request;
struct sip_reply;
struct sip_header;
struct sip_via_parm;

//
// SIP message types:
//

enum {
    SIP_UNKNOWN=0,
    SIP_REQUEST,
    SIP_REPLY
};


struct sip_request
{
    //
    // Request methods
    //
    
    enum {
	OTHER_METHOD=0,
	INVITE,
	ACK,
	OPTIONS,
	BYE,
	CANCEL,
	REGISTER
    };

    cstring  method_str;
    int      method;

    cstring  ruri_str;
    sip_uri  ruri;
};


struct sip_reply
{
    int     code;
    cstring reason;

    sip_reply()
	: code(0)
    {}
};


struct sip_msg
{
    char*   buf;
    int     len;

    // Request or Reply?
    int     type; 
    
    union {
	sip_request* request;
	sip_reply*   reply;
    }u;

    list<sip_header*>  hdrs;
    
    sip_header*        to;
    sip_header*        from;

    sip_header*        cseq;

    sip_header*        via1;
    sip_via_parm*      via_p1;

    sip_header*        callid;
    sip_header*        contact;
    list<sip_header*>  route;
    list<sip_header*>  record_route;
    sip_header*        content_type;
    sip_header*        content_length;
    cstring            body;

    sockaddr_storage   local_ip;

    sockaddr_storage   remote_ip;

    sip_msg();
    sip_msg(char* msg_buf, int msg_len);
    ~sip_msg();
};

int parse_method(int* method, char* beg, int len);
int parse_sip_msg(sip_msg* msg);

#endif
