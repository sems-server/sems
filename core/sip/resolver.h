/*
 * $Id: resolver.h 1460 2009-07-08 12:50:39Z rco $
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
#ifndef _resolver_h_
#define _resolver_h_

#include "singleton.h"
#include "hash_table.h"
#include "atomic_types.h"
#include "parse_dns.h"
#include "parse_next_hop.h"

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
    virtual dns_base_entry* get_rr(dns_record* rr, u_char* begin, u_char* end)=0;

public:
    vector<dns_base_entry*> ip_vec;

    static dns_entry* make_entry(ns_type t);

    dns_entry();
    virtual ~dns_entry();
    virtual void init()=0;
    virtual void add_rr(dns_record* rr, u_char* begin, u_char* end, long now);
    virtual int next_ip(dns_handle* h, sockaddr_storage* sa)=0;
};

typedef ht_map_bucket<string,dns_entry> dns_bucket_base;

class dns_bucket
    : protected dns_bucket_base
{
    friend class _resolver;
public:
    dns_bucket(unsigned long id);
    bool insert(const string& name, dns_entry* e);
    bool remove(const string& name);
    dns_entry* find(const string& name);
};

typedef hash_table<dns_bucket> dns_cache;

struct ip_entry
    : public dns_base_entry
{
    address_type  type;

    union {
	in_addr       addr;
	in6_addr      addr6;
    };

    virtual void to_sa(sockaddr_storage* sa);
};

struct ip_port_entry
    : public ip_entry
{
    unsigned short port;

    virtual void to_sa(sockaddr_storage* sa);
};

class dns_ip_entry
    : public dns_entry
{
public:
    dns_ip_entry()
	: dns_entry()
    {}

    void init(){};
    dns_base_entry* get_rr(dns_record* rr, u_char* begin, u_char* end);
    int next_ip(dns_handle* h, sockaddr_storage* sa);

    int fill_ip_list(const list<host_port>& ip_list);
};

class dns_srv_entry;

struct dns_handle
{
    dns_handle();
    ~dns_handle();

    bool valid();
    bool eoip();

    int next_ip(sockaddr_storage* sa);
    const dns_handle& operator = (const dns_handle& rh);

private:
    friend class _resolver;
    friend class dns_entry;
    friend class dns_srv_entry;
    friend class dns_ip_entry;

    dns_srv_entry* srv_e;
    int            srv_n;
    unsigned int   srv_used;
    unsigned short port;

    dns_ip_entry*  ip_e;
    int            ip_n;
};

class _resolver
    : AmThread
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

    void run();
    void on_stop() {}

private:
    dns_cache cache;
};

typedef singleton<_resolver> resolver;

/** Converts a string into an IP structure*/
int str2ip(const char* name,
	   sockaddr_storage* sa,
	   const address_type types);

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
