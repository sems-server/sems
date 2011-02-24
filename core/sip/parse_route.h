/*
 * Copyright (C) 2011 Raphael Coeffic
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


#ifndef _parse_route_h
#define _parse_route_h

#include "parse_header.h"

struct sip_nameaddr;
struct sip_uri;

struct route_elmt
{
  sip_nameaddr* addr;
  cstring       route;

  route_elmt()
    : addr(NULL), route()
  {}

  ~route_elmt();
};

struct sip_route: public sip_parsed_hdr
{
  list<route_elmt*> elmts;

  sip_route() 
    : sip_parsed_hdr(),
      elmts()
  {}

  ~sip_route();
};

int parse_route(sip_header* rh);

int parse_first_route_uri(sip_header* fr);
sip_uri* get_first_route_uri(sip_header* fr);

bool is_loose_route(const sip_uri* fr_uri);

#endif
