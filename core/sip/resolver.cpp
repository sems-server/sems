/*
 * $Id: resolver.cpp 1048 2008-07-15 18:48:07Z sayer $
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

#include "resolver.h"
#include "hash.h"

#include <sys/socket.h> 
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <arpa/nameser.h> 
#include <arpa/nameser_compat.h> // Darwin

#include <list>

using std::pair;
using std::list;

#include "log.h"

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

    int next_ip(dns_handle* h, sockaddr_storage* sa);
};

int dns_ip_entry::next_ip(dns_handle* h, sockaddr_storage* sa)
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

class dns_srv_entry
    : public dns_entry
{
public:
    dns_srv_entry()
	: dns_entry()
    {}

    int next_ip(dns_handle* h, sockaddr_storage* sa);
};

int dns_srv_entry::next_ip(dns_handle* h, sockaddr_storage* sa)
{
    int& index = h->srv_n;
    if(index >= (int)ip_vec.size()) return -1;

    if(h->srv_e != this){
	if(h->srv_e) dec_ref(h->srv_e);
	h->srv_e = this;
	h->srv_n = 0;
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
    unsigned int w_sum = ((srv_entry*)ip_vec[i])->w;
    srv_lst.push_back(std::make_pair(w_sum,i));

    // and fetch records with same priority
    while( (++i != (int)ip_vec.size()) && 
	   (p==((srv_entry*)ip_vec[i])->p) ){
	
	w_sum += ((srv_entry*)ip_vec[i])->w;
	srv_lst.push_back(std::make_pair(w_sum,i));
    }

    srv_entry* e=NULL;
    if((i - index > 1) && w_sum){
	// multiple records: apply weigthed load balancing
	
	// TODO:
	// - generate random number
	// - pick the first record with cum. sum >= random number

	unsigned int r = rand() % (w_sum+1);

	list<pair<unsigned int,int> >::iterator srv_lst_it = srv_lst.begin();
	while(srv_lst_it != srv_lst.end()){
	    if(srv_lst_it->first >= r){
		//TODO: add this entry to some "already tried" list
		e = (srv_entry*)ip_vec[srv_lst_it->second];
	    }
	}

	// should never trigger
	assert(e);
    }
    else {
	// single record or all weights == 0
	e = (srv_entry*)ip_vec[index];
	if(++index >= (int)ip_vec.size()){
	    h->srv_n = -1;
	}
    }

    //TODO: find a solution for IPv6
    h->port = htons(e->port);
    ((sockaddr_in*)sa)->sin_port = h->port;
    return resolver::instance()->resolve_name(e->target.c_str(),h,sa,IPv4);
}


dns_entry::dns_entry()
    : dns_base_entry()
{
}

dns_entry::~dns_entry()
{
    DBG("~dns_entry()");
    for(vector<dns_base_entry*>::iterator it = ip_vec.begin();
	it != ip_vec.end(); ++it) {

	delete *it;
    }
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

_resolver::_resolver()
    : cache(DNS_CACHE_SIZE)
{
    start();
}

_resolver::~_resolver()
{
    
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

static int collect_rr(ns_msg* handle, ns_sect section, ns_type type,
		      dns_entry* dns_e, long now,
		      dns_base_entry* (rr_to_entry)(ns_msg*,ns_rr*))
{
  /*
   * Look at all the resource records in this section.
   */

  assert(handle);
  assert(dns_e);

  ns_rr rr;

  for(int rrnum = 0; rrnum < ns_msg_count(*handle, section); rrnum++) {
    /*
     * Expand the resource record number rrnum into rr.
     */
    if (ns_parserr(handle, section, rrnum, &rr)) {
      ERROR("ns_parserr: %s\n", strerror(errno));
      continue;
    }
    
    /*
     * If the record type is correct, save the data into
     * the proper entry.
     */
    if(ns_rr_type(rr) == type) {
	dns_base_entry* new_entry = (*rr_to_entry)(handle,&rr);
	if(!new_entry){ continue; }
	new_entry->expire = now + ns_rr_ttl(rr);
	dns_e->ip_vec.push_back(new_entry);
	if(dns_e->expire < new_entry->expire)
	    dns_e->expire = new_entry->expire;
    }
  }

  return 0;
}

static dns_base_entry* rr_to_a_entry(ns_msg*, ns_rr* rr)
{
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

static int collect_a_rr(ns_msg* handle, ns_sect section, dns_entry* dns_e, long now)
{
    return collect_rr(handle,section,ns_t_a,dns_e,now,rr_to_a_entry);
}

static dns_base_entry* rr_to_srv_entry(ns_msg* handle, ns_rr* rr)
{
    char name_buf[MAXDNAME];
    const u_char * rdata = ns_rr_rdata(*rr);
	
    /* Expand the target's name */
    if (ns_name_uncompress(
			   ns_msg_base(*handle),/* Start of the packet   */
			   ns_msg_end(*handle), /* End of the packet     */
			   rdata+6,            /* Position in the packet*/
			   name_buf,           /* Result                */
			   MAXDNAME)           /* Size of result buffer */
	< 0) {    /* Negative: error       */
	
	ERROR("ns_name_uncompress failed\n");
	return NULL;
    }
    
    printf("SRV:\tTTL=%i\t%s\tP=<%i> W=<%i> P=<%i> T=<%s>\n",
	   ns_rr_ttl(*rr),
	   ns_rr_name(*rr),
	   ns_get16(rdata),
	   ns_get16(rdata+2),
	   ns_get16(rdata+4),
	   name_buf);
    
    srv_entry* srv_r = new srv_entry();
    srv_r->p = ns_get16(rdata);
    srv_r->w = ns_get16(rdata+2);
    srv_r->port = ns_get16(rdata+4);
    srv_r->target = (const char*)name_buf;

    return srv_r;
}

static bool srv_less(const dns_base_entry* le, const dns_base_entry* re)
{
    const srv_entry* l_srv = (const srv_entry*)le;
    const srv_entry* r_srv = (const srv_entry*)re;

    if(l_srv->p != r_srv->p)
	return l_srv->p < r_srv->p;
    else
	return l_srv->w < r_srv->w;
};

static int collect_srv_rr(ns_msg* handle, ns_sect section, dns_entry* dns_e, long now)
{
    int ret = collect_rr(handle,section,ns_t_srv,dns_e,now,rr_to_srv_entry);
    if(!ret){
	stable_sort(dns_e->ip_vec.begin(),dns_e->ip_vec.end(),srv_less);
    }

    return ret;
}


int _resolver::query_dns(const char* name, dns_entry** e, long now)
{
    typedef union {
        HEADER hdr;              /* defined in resolv.h */
        u_char buf[NS_PACKETSZ]; /* defined in arpa/nameser.h */
    } dns_response;              /* response buffers */
    
    if(!name) return -1;

    dns_response dns_res;
    ns_type t = (name[0] == '_') ? ns_t_srv : ns_t_a;
    //TODO: add AAAA record support
    int dns_res_len = res_search(name,ns_c_in,t,
				 (u_char *)&dns_res.buf,sizeof(dns_response));
    
    if(dns_res_len < 0){
	dns_error(h_errno,name);
	return -1;
    }
    
    /*
     * Initialize a handle to this response.  The handle will
     * be used later to extract information from the response.
     */
    ns_msg handle;
    if (ns_initparse(dns_res.buf, dns_res_len, &handle) < 0) {
	ERROR("ns_initparse: %s\n", strerror(errno));
	return -1;
    }
    
    if(!ns_msg_count(handle,ns_s_an)) {
	// nothing in the answer section
	return -1;
    }
	
    int     ret;
    switch(t){
    case ns_t_srv:
	*e = new dns_srv_entry();
	ret = collect_srv_rr(&handle,ns_s_an,*e,now);
	break;
    case ns_t_a:
	*e = new dns_ip_entry();
	ret = collect_a_rr(&handle,ns_s_an,*e,now);
	break;
    default:
	ret = -1;
	break;
    }

    if((ret < 0) || (*e)->ip_vec.empty()){
	delete *e;
	*e = NULL;

	return -1;
    }

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
	// DNS query failed
	WARN("DNS query failed");
	return -1;
    }

    // if ttl != 0
    if(e->expire != tv_now.tv_sec){
	// cache the new record
	b->insert(name,e);
    }
    
    if(e) {
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
	    DBG("inet_pton() succeeded");
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
	    DBG("inet_pton() succeeded");
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

dns_handle::dns_handle() 
  : srv_e(0), srv_n(0), ip_e(0), ip_n(0) 
{}

dns_handle::~dns_handle() 
{ 
    DBG("~dns_handle()");
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
    if(srv_n)
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

void _resolver::run()
{
    for(;;) {
	sleep(10);

	timeval tv_now;
	gettimeofday(&tv_now,NULL);

	DBG("starting DNS cache garbage collection");
	for(unsigned long i=0; i<cache.get_size(); i++){

	    dns_bucket* bucket = cache.get_bucket(i);
	    bucket->lock();
	    
	    for(dns_bucket::value_map::iterator it = bucket->elmts.begin();
		it != bucket->elmts.end(); ++it){

		dns_entry* dns_e = (dns_entry*)it->second;
		if(tv_now.tv_sec >= it->second->expire){

		    dns_bucket::value_map::iterator tmp_it = it;
		    bool end_of_bucket = (++it == bucket->elmts.end());

		    DBG("########### expiring record %p #############",dns_e);
		    bucket->elmts.erase(tmp_it);
		    dec_ref(dns_e);

		    if(end_of_bucket) break;
		}
		else {
		    DBG("######### record %p expires in %li seconds ##########",dns_e,it->second->expire-tv_now.tv_sec);
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
