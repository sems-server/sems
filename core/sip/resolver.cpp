/*
 * $Id: resolver.cpp 1048 2008-07-15 18:48:07Z sayer $
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

#include "resolver.h"
#include "hash.h"

#include "parse_dns.h"

#include <sys/socket.h> 
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <arpa/nameser.h> 

#include <list>
#include <algorithm>

using std::pair;
using std::list;

#include "log.h"

// Maximum number of SRV entries
// within a cache entry
//
// (the limit is the # bits in dns_handle::srv_used)
#define MAX_SRV_RR (sizeof(unsigned int)*8)

/* The SEMS_GET16 macro and the sems_get16 function were copied from glibc 2.7
 * (include/arpa/nameser.h (NS_GET16) and resolv/ns_netint.c (ns_get16)) to
 *  avoid using private glibc functions.
 */

# define SEMS_GET16(s, cp)              \
  do {                                  \
    uint16_t *t_cp = (uint16_t *) (cp); \
    (s) = ntohs (*t_cp);                \
    (cp) += NS_INT16SZ;                 \
} while (0)

u_int
sems_get16(const u_char *src)
{
       u_int dst;

       SEMS_GET16(dst, src);
       return (dst);
}

struct ip_entry
    : public dns_base_entry
{
    address_type  type;
    in_addr       addr;

    void to_sa(sockaddr_storage* sa);
};

struct srv_entry
    : public dns_base_entry
{
    unsigned short   p;
    unsigned short   w;

    unsigned short port;
    string       target;
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

    int next_ip(dns_handle* h, sockaddr_storage* sa)
    {
	if(h->ip_e != this){
	    if(h->ip_e) dec_ref(h->ip_e);
	    h->ip_e = this;
	    h->ip_n = 0;
	}

	int& index = h->ip_n;
	if(index >= (int)ip_vec.size()) return -1;

	//copy address
	((ip_entry*)ip_vec[index++])->to_sa(sa);

	// reached the end?
	if(index >= (int)ip_vec.size()) { 
	    index = -1;
	}
	
	return 0;
    }
};

static bool srv_less(const dns_base_entry* le, const dns_base_entry* re)
{
    const srv_entry* l_srv = (const srv_entry*)le;
    const srv_entry* r_srv = (const srv_entry*)re;

    if(l_srv->p != r_srv->p)
	return l_srv->p < r_srv->p;
    else
	return l_srv->w < r_srv->w;
};

class dns_srv_entry
    : public dns_entry
{
public:
    dns_srv_entry()
	: dns_entry()
    {}

    void init(){
	stable_sort(ip_vec.begin(),ip_vec.end(),srv_less);
    }

    dns_base_entry* get_rr(dns_record* rr, u_char* begin, u_char* end);

    int next_ip(dns_handle* h, sockaddr_storage* sa)
    {
	int& index = h->srv_n;
	if(index >= (int)ip_vec.size()) return -1;
	
	if(h->srv_e != this){
	    if(h->srv_e) dec_ref(h->srv_e);
	    h->srv_e = this;
	    h->srv_n = 0;
	    h->srv_used = 0;
	}
	else if(h->ip_n != -1){
	    ((sockaddr_in*)sa)->sin_port = h->port;
	    return h->ip_e->next_ip(h,sa);
	}
	
	// reset IP record
	if(h->ip_e){
	    dec_ref(h->ip_e);
	    h->ip_e = NULL;
	    h->ip_n = 0;
	}
	
	list<pair<unsigned int,int> > srv_lst;
	int i = index;
	
	// fetch current priority
	unsigned short p = ((srv_entry*)ip_vec[i])->p;
	unsigned int w_sum = 0;
	
	// and fetch records with same priority
	// which have not been chosen yet
	int srv_lst_size=0;
	unsigned int used_mask=(1<<i);
	while( p==((srv_entry*)ip_vec[i])->p ){
	    
	    if(!(used_mask & h->srv_used)){
		w_sum += ((srv_entry*)ip_vec[i])->w;
		srv_lst.push_back(std::make_pair(w_sum,i));
		srv_lst_size++;
	    }
	    
	    if((++i >= (int)ip_vec.size()) ||
	       (i >= (int)MAX_SRV_RR)){
		break;
	    }
	    
	    used_mask = used_mask << 1;
	}
	
	srv_entry* e=NULL;
	if((srv_lst_size > 1) && w_sum){
	    // multiple records: apply weigthed load balancing
	    // - remember the entries which have already been used
	    unsigned int r = random() % (w_sum+1);
	    
	    list<pair<unsigned int,int> >::iterator srv_lst_it = srv_lst.begin();
	    while(srv_lst_it != srv_lst.end()){
		if(srv_lst_it->first >= r){
		    h->srv_used |= (1<<(srv_lst_it->second));
		    e = (srv_entry*)ip_vec[srv_lst_it->second];
		    break;
		}
		++srv_lst_it;
	    }
	    
	    // will only happen if the algorithm
	    // is broken
	    if(!e)
		return -1;
	}
	else {
	    // single record or all weights == 0
	    e = (srv_entry*)ip_vec[srv_lst.begin()->second];
	    
	    if( (i<(int)ip_vec.size()) && (i<(int)MAX_SRV_RR)){
		index = i;
	    }
	    else if(!w_sum){
		index++;
	    }
	    else {
		index = -1;
	    }
	}
	
	//TODO: find a solution for IPv6
	h->port = htons(e->port);
	((sockaddr_in*)sa)->sin_port = h->port;

	// check if name is an IP address
	if(resolver::instance()->str2ip(e->target.c_str(),sa,IPv4) == 1) {
	    h->ip_n = -1; // flag end of IP list
	    return 0;
	}

	return resolver::instance()->resolve_name(e->target.c_str(),h,sa,IPv4);
    }
};

dns_entry::dns_entry()
    : dns_base_entry()
{
}

dns_entry::~dns_entry()
{
    for(vector<dns_base_entry*>::iterator it = ip_vec.begin();
	it != ip_vec.end(); ++it) {

	delete *it;
    }
}

dns_entry* dns_entry::make_entry(ns_type t)
{
    switch(t){
    case ns_t_srv:
	return new dns_srv_entry();
    case ns_t_a:
    //case ns_t_aaaa:
	return new dns_ip_entry();
    default:
	return NULL;
    }
}

void dns_entry::add_rr(dns_record* rr, u_char* begin, u_char* end, long now)
{
    dns_base_entry* e = get_rr(rr,begin,end);
    if(!e) return;

    e->expire = rr->ttl + now;
    if(expire < e->expire)
	expire = e->expire;

    ip_vec.push_back(e);
}

dns_bucket::dns_bucket(unsigned long id) 
  : dns_bucket_base(id) 
{
}

bool dns_bucket::insert(const string& name, dns_entry* e)
{
    if(!e) return false;

    lock();
    if(!(elmts.insert(std::make_pair(name,e)).second)){
	// if insertion failed
	unlock();
	return false;
    }

    inc_ref(e);
    unlock();

    return true;
}

bool dns_bucket::remove(const string& name)
{
    lock();
    value_map::iterator it = elmts.find(name);
    if(it != elmts.end()){
	
	dns_entry* e = it->second;
	elmts.erase(it);

	dec_ref(e);
	unlock();
	
	return true;
    }

    unlock();
    return false;
}


dns_entry* dns_bucket::find(const string& name)
{
    lock();
    value_map::iterator it = elmts.find(name);
    if(it == elmts.end()){
	unlock();
	return NULL;
    }

    dns_entry* e = it->second;

    timeval now;
    gettimeofday(&now,NULL);
    if(now.tv_sec >= e->expire){
	elmts.erase(it);
	dec_ref(e);
	unlock();
	return NULL;
    }

    inc_ref(e);
    unlock();
    return e;
}

static void dns_error(int error, const char* domain)
{
    switch(error){
        case HOST_NOT_FOUND:
          DBG("Unknown domain: %s\n", domain);
          break;
        case NO_DATA:
	  DBG("No records for %s\n", domain);
          break;
        case TRY_AGAIN:
          DBG("No response for query (try again)\n");
          break;
        default:
          ERROR("Unexpected error\n");
          break;
    }
}

void ip_entry::to_sa(sockaddr_storage* sa)
{
    sockaddr_in* sa_in = (sockaddr_in*)sa;
    sa_in->sin_family = AF_INET;
    memcpy(&(sa_in->sin_addr),&addr,sizeof(in_addr));
}

dns_base_entry* dns_ip_entry::get_rr(dns_record* rr, u_char* begin, u_char* end)
{
    if(rr->type != dns_r_a)
	return NULL;

    DBG("A:\tTTL=%i\t%s\t%i.%i.%i.%i\n",
	ns_rr_ttl(*rr),
	ns_rr_name(*rr),
	ns_rr_rdata(*rr)[0],
	ns_rr_rdata(*rr)[1],
	ns_rr_rdata(*rr)[2],
	ns_rr_rdata(*rr)[3]);
    
    ip_entry* new_ip = new ip_entry();
    new_ip->type = IPv4;
    memcpy(&(new_ip->addr), ns_rr_rdata(*rr), sizeof(in_addr));

    return new_ip;
}

dns_base_entry* dns_srv_entry::get_rr(dns_record* rr, u_char* begin, u_char* end)
{
    if(rr->type != dns_r_srv)
	return NULL;

    u_char name_buf[NS_MAXDNAME];
    const u_char * rdata = ns_rr_rdata(*rr);
	
    /* Expand the target's name */
    u_char* p = (u_char*)rdata+6;
    if (dns_expand_name(&p,begin,end,
    			   name_buf,         /* Result                */
    			   NS_MAXDNAME)      /* Size of result buffer */
    	< 0) {    /* Negative: error       */
	
    	ERROR("dns_expand_name failed\n");
    	return NULL;
    }
    
    DBG("SRV:\tTTL=%i\t%s\tP=<%i> W=<%i> P=<%i> T=<%s>\n",
    	ns_rr_ttl(*rr),
    	ns_rr_name(*rr),
    	sems_get16(rdata),
    	sems_get16(rdata+2),
    	sems_get16(rdata+4),
    	name_buf);
    
    srv_entry* srv_r = new srv_entry();
    srv_r->p = sems_get16(rdata);
    srv_r->w = sems_get16(rdata+2);
    srv_r->port = sems_get16(rdata+4);
    srv_r->target = (const char*)name_buf;

    return srv_r;
}

struct dns_entry_h
{
    dns_entry* e;
    long     now;
};

int rr_to_dns_entry(dns_record* rr, dns_section_type t, u_char* begin, u_char* end, void* data)
{
    dns_entry* dns_e = ((dns_entry_h*)data)->e;
    long     now = ((dns_entry_h*)data)->now;

    if(t == dns_s_an)
	dns_e->add_rr(rr,begin,end,now);

    // TODO: parse the additional section as well.
    //       there might be some A/AAAA records related to
    //       the SRV targets.
    
    return 0;
}

dns_handle::dns_handle() 
  : srv_e(0), srv_n(0), ip_e(0), ip_n(0) 
{}

dns_handle::~dns_handle() 
{ 
    if(ip_e) 
	dec_ref(ip_e); 

    if(srv_e) 
	dec_ref(srv_e); 
}

bool dns_handle::valid() 
{ 
    return (ip_e);
}

bool dns_handle::eoip()  
{ 
    if(srv_e)
	return (srv_n == -1) && (ip_n == -1);
    else
	return (ip_n == -1);
}

int dns_handle::next_ip(sockaddr_storage* sa)
{
    if(!valid() || eoip()) return -1;

    if(srv_e)
	return srv_e->next_ip(this,sa);
    else
	return ip_e->next_ip(this,sa);
}


_resolver::_resolver()
    : cache(DNS_CACHE_SIZE)
{
    start();
}

_resolver::~_resolver()
{
    
}

int _resolver::query_dns(const char* name, dns_entry** e, long now)
{
    u_char dns_res[NS_PACKETSZ];

    if(!name) return -1;

    ns_type t = (name[0] == '_') ? ns_t_srv : ns_t_a;
    
    //TODO: add AAAA record support
    int dns_res_len = res_search(name,ns_c_in,t,dns_res,NS_PACKETSZ);
    if(dns_res_len < 0){
	dns_error(h_errno,name);
	return -1;
    }

    *e = dns_entry::make_entry(t);

    /*
     * Initialize a handle to this response.  The handle will
     * be used later to extract information from the response.
     */
    dns_entry_h dns_h = { *e, now };
    if (dns_msg_parse(dns_res, dns_res_len, rr_to_dns_entry, &dns_h) < 0) {
	DBG("Could not parse DNS reply");
	return -1;
    }
    
    *e = dns_h.e;
    if(!*e) {
	DBG("no dns_entry created");
	return -1;
    }

    if((*e)->ip_vec.empty()){
    	delete *e;
    	*e = NULL;
    	return -1;
    }

    (*e)->init();
    inc_ref(*e);

    return 0;
}

int _resolver::resolve_name(const char* name,
			    dns_handle* h,
			    sockaddr_storage* sa,
			    const address_type types)
{
    int ret;

    // already have a valid handle?
    if(h->valid()){
	if(h->eoip()) return -1;
	return h->next_ip(sa);
    }

    // first try to detect if 'name' is already an IP address
    ret = str2ip(name,sa,types);
    if(ret == 1) {
	h->ip_n = -1; // flag end of IP list
	h->srv_n = -1;
	return 0; // 'name' is an IP address
    }
    
    // name is NOT an IP address -> try a cache look up
    dns_bucket* b = cache.get_bucket(hashlittle(name,strlen(name),0));
    dns_entry* e = b->find(name);

    // first attempt to get a valid IP
    // (from the cache)
    if(e){
	return e->next_ip(h,sa);
    }

    timeval tv_now;
    gettimeofday(&tv_now,NULL);

    // no valid IP, query the DNS
    if(query_dns(name,&e,tv_now.tv_sec) < 0) {
	return -1;
    }

    if(e) {

	// if ttl != 0
	if(e->expire != tv_now.tv_sec){
	    // cache the new record
	    b->insert(name,e);
	}
    
	// now we should have a valid IP
	return e->next_ip(h,sa);
    }

    // should not happen...
    return -1;
}

int _resolver::str2ip(const char* name,
		      sockaddr_storage* sa,
		      const address_type types)
{
    if(types & IPv4){
	int ret = inet_pton(AF_INET,name,&((sockaddr_in*)sa)->sin_addr);
	if(ret==1) {
	    ((sockaddr_in*)sa)->sin_family = AF_INET;
	    return 1;
	}
	else if(ret < 0) {
	    ERROR("while trying to detect an IPv4 address '%s': %s",name,strerror(errno));
	    return ret;
	}
    }
    
    if(types & IPv6){
	int ret = inet_pton(AF_INET6,name,&((sockaddr_in6*)sa)->sin6_addr);
	if(ret==1) {
	    ((sockaddr_in6*)sa)->sin6_family = AF_INET6;
	    return 1;
	}
	else if(ret < 0) {
	    ERROR("while trying to detect an IPv6 address '%s': %s",name,strerror(errno));
	    return ret;
	}
    }

    return 0;
}

void _resolver::run()
{
    for(;;) {
	sleep(10);

	timeval tv_now;
	gettimeofday(&tv_now,NULL);

	//DBG("starting DNS cache garbage collection");
	for(unsigned long i=0; i<cache.get_size(); i++){

	    dns_bucket* bucket = cache.get_bucket(i);
	    bucket->lock();
	    
	    for(dns_bucket::value_map::iterator it = bucket->elmts.begin();
		it != bucket->elmts.end(); ++it){

		dns_entry* dns_e = (dns_entry*)it->second;
		if(tv_now.tv_sec >= it->second->expire){

		    dns_bucket::value_map::iterator tmp_it = it;
		    bool end_of_bucket = (++it == bucket->elmts.end());

		    DBG("DNS record expired (%p)",dns_e);
		    bucket->elmts.erase(tmp_it);
		    dec_ref(dns_e);

		    if(end_of_bucket) break;
		}
		else {
		    //DBG("######### record %p expires in %li seconds ##########",dns_e,it->second->expire-tv_now.tv_sec);
		}
	    }
	    bucket->unlock();
	}
    }
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
