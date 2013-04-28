/*
 * $Id: parse_uri.h 1133 2008-11-23 11:31:34Z rco $
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

#ifndef _parse_uri_h
#define _parse_uri_h

#include "cstring.h"

#include <list>
using std::list;

struct sip_avp;

struct sip_uri
{
    enum uri_scheme {
	UNKNOWN=0,
	SIP,
	SIPS
    };

    uri_scheme scheme;
    cstring    user;
    cstring    passwd;
    cstring    host;

    cstring    port_str;
    short unsigned int  port;

    list<sip_avp*> params;
    list<sip_avp*> hdrs;
    sip_avp*       trsp;

    sip_uri();
    ~sip_uri();
};

int parse_uri(sip_uri* uri, const char* beg, int len);

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
