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
#ifndef _resolver_h_
#define _resolver_h_

struct sockaddr_storage;

/* struct name_entry */
/* { */
/*     string name; */
/*     string address; */
/* }; */

enum address_type {

    IPv4=1,
    IPv6=2
};

enum proto_type {
    
    TCP=1,
    UDP=2
};

class resolver
{
    static resolver* _instance;

    resolver();
    ~resolver();

 public:

    static resolver* instance();
    
    int resolve_name(const char* name, sockaddr_storage* sa, 
		     const address_type types, const proto_type protos);
};


#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
