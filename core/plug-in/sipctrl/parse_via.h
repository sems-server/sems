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

#ifndef _parse_via_h
#define _parse_via_h

#include "parse_header.h"

struct sip_avp;

struct sip_transport
{
    enum {
	UNPARSED=0,
	UDP,
	TCP,
	TLS,
	SCTP,
	OTHER
    };

    int     type;
    cstring val;
};

struct sip_via_parm
{
    sip_transport  trans;
    cstring        host;
    cstring        port; // ?? int/short ??

    list<sip_avp*> params;

    cstring        branch;

    ~sip_via_parm();
};

struct sip_via: public sip_parsed_hdr
{
    list<sip_via_parm*> parms;

    ~sip_via();
};

int parse_via(sip_via* via, char* beg, int len);

#define MAGIC_BRANCH_COOKIE "z9hG4bK"
#define MAGIC_BRANCH_LEN    7

#endif
