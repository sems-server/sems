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

#ifndef _parse_header_h
#define _parse_header_h

#include "cstring.h"

#include <list>
using std::list;

struct sip_parsed_hdr
{
    virtual ~sip_parsed_hdr(){}
};


struct sip_header
{
    //
    // Header types
    //
    
    enum {
	H_UNPARSED=0,
	
	H_TO,
	H_VIA,
	H_FROM,
	H_CSEQ,
	H_ROUTE,
	H_CALL_ID,
	H_CONTACT,
	H_RECORD_ROUTE,
	H_CONTENT_TYPE,
	H_CONTENT_LENGTH,
	
	H_OTHER
    };

    int     type;
    cstring name;
    cstring value;

    sip_parsed_hdr* p;

    sip_header();
    sip_header(const sip_header& hdr);
    sip_header(int type, const cstring& name, const cstring& value);
    ~sip_header();
};

struct sip_msg;

int parse_headers(sip_msg* msg, char** c);

#endif
