/*
 * $Id: resolver.h 1460 2009-07-08 12:50:39Z rco $
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

#include "singleton.h"
#include "hash_table.h"
#include "atomic_types.h"

#include <string>
#include <vector>
using std::string;
using std::vector;

#include <netinet/in.h>

#define DNS_CACHE_SIZE 128

enum address_type {

    IPnone=0,
    IPv4=1,
    IPv6=2
};

enum proto_type {
    
    TCP=1,
    UDP=2
};

struct dns_handle;

struct dns_base_entry
{
    long int expire;

    dns_base_entry()
	:expire(0)
    {}

    virtual ~dns_base_entry() {}
};

class dns_entry
    : public atomic_ref_cnt,
      public dns_base_entry
{
public:
    vector<dns_base_entry*> ip_vec;

    dns_entry();
    virtual ~dns_entry();

    virtual int next_ip(dns_handle* h, sockaddr_storage* sa)=0;
};

typedef ht_map_bucket<string,dns_entry> dns_bucket_base;

class dns_bucket
    : protected dns_bucket_base
{
public:
    dns_bucket(unsigned long id);
    bool insert(const string& name, dns_entry* e);
    bool remove(const string& name);
    dns_entry* find(const string& name);
};

typedef hash_table<dns_bucket> dns_cache;

class dns_srv_entry;
class dns_ip_entry;

struct dns_handle
{
    dns_handle();
    ~dns_handle();

    bool valid();
    bool eoip();

    int next_ip(sockaddr_storage* sa);

private:
    friend class _resolver;
    friend class dns_entry;
    friend class dns_srv_entry;
    friend class dns_ip_entry;

    dns_srv_entry* srv_e;
    int            srv_n;
    unsigned short  port;

    dns_ip_entry*  ip_e;
    int            ip_n;
};

class _resolver
{
public:
    int resolve_name(const char* name, 
		     dns_handle* h,
		     sockaddr_storage* sa,
		     const address_type types);
    
protected:
    _resolver();
    ~_resolver();

    int query_dns(const char* name,
		  dns_entry** e,
		  long now);

    int str2ip(const char* name,
	       sockaddr_storage* sa,
	       const address_type types);

private:
    dns_cache cache;
};

typedef singleton<_resolver> resolver;

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
