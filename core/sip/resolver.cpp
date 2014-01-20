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
#include "parse_common.h"
#include "ip_util.h"
#include "trans_layer.h"
#include "tr_blacklist.h"
#include "wheeltimer.h"

#include "AmUtils.h"

#include <sys/socket.h> 
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <arpa/nameser.h> 

#include <list>
#include <utility>
#include <algorithm>

using std::pair;
using std::make_pair;
using std::list;

#include "log.h"

// Maximum number of SRV entries
// within a cache entry
//
// (the limit is the # bits in dns_handle::srv_used)
#define MAX_SRV_RR (sizeof(unsigned int)*8)

/* in seconds */
#define DNS_CACHE_CYCLE 10L

/* in us */
#define DNS_CACHE_SINGLE_CYCLE \
  ((DNS_CACHE_CYCLE*1000000L)/DNS_CACHE_SIZE)

struct srv_entry
    : public dns_base_entry
{
    unsigned short   p;
    unsigned short   w;

    unsigned short port;
    string       target;

    virtual string to_str();
};

int dns_ip_entry::next_ip(dns_handle* h, sockaddr_storage* sa)
{
    if(h->ip_e != this){
	if(h->ip_e) dec_ref(h->ip_e);
	h->ip_e = this;
	inc_ref(this);
	h->ip_n = 0;
    }
    
    int& index = h->ip_n;
    if((index < 0) || (index >= (int)ip_vec.size()))
	return -1;
    
    //copy address
    ((ip_entry*)ip_vec[index++])->to_sa(sa);
    
    // reached the end?
    if(index >= (int)ip_vec.size()) { 
	index = -1;
    }
    
    return 0;
}

int dns_ip_entry::fill_ip_list(const list<sip_destination>& ip_list)
{
    int res;
    string ip;
    ip_port_entry e;

    for(list<sip_destination>::const_iterator it = ip_list.begin();
	it != ip_list.end(); ++it) {

	e.port = it->port;
	ip = c2stlstr(it->host);

	res = inet_pton(AF_INET6,ip.c_str(),&e.addr6);
	if(res == 1) {
	    e.type = IPv6;
	    ip_vec.push_back(new ip_port_entry(e));
	    continue;
	}
	else if(res < 0){
	    DBG("inet_pton(AF_INET6,%s,...): %s\n",
		ip.c_str(), strerror(errno));
	}
	
	res = inet_pton(AF_INET,ip.c_str(),&e.addr);
	if(res < 0){
	    DBG("inet_pton(AF_INET,%s,...): %s\n",
		ip.c_str(), strerror(errno));
	}
	else if(res == 0){
	    DBG("<%s> is not a valid IP address\n",ip.c_str());
	    return -1;
	}

	e.type = IPv4;
	ip_vec.push_back(new ip_port_entry(e));
    }

    return 0;
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
	if(h->srv_e != this){
	    if(h->srv_e) dec_ref(h->srv_e);
	    h->srv_e = this;
	    inc_ref(this);
	    h->srv_n = 0;
	    h->srv_used = 0;
	}
	else if(h->ip_n != -1){
	    if(h->port) {
		((sockaddr_in*)sa)->sin_port = h->port;
	    }
	    else {
		((sockaddr_in*)sa)->sin_port = htons(5060);
	    }
	    return h->ip_e->next_ip(h,sa);
	}

	if((index < 0) ||
	   (index >= (int)ip_vec.size()))
	    return -1;
	
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
	if(h->port) {
	    ((sockaddr_in*)sa)->sin_port = h->port;
	}
	else {
	    ((sockaddr_in*)sa)->sin_port = htons(5060);
	}

	// check if name is an IP address
	if(am_inet_pton(e->target.c_str(),sa) == 1) {
	    // target is an IP address
	    h->ip_n = -1; // flag end of IP list
	    return 0;
	}

	// target must be resolved first
	return resolver::instance()->resolve_name(e->target.c_str(),h,sa,IPv4);
    }
};

dns_entry::dns_entry()
    : dns_base_entry()
{
}

dns_entry::~dns_entry()
{
    DBG("dns_entry::~dns_entry(): %s",to_str().c_str());
    for(vector<dns_base_entry*>::iterator it = ip_vec.begin();
	it != ip_vec.end(); ++it) {

	delete *it;
    }
}

dns_entry* dns_entry::make_entry(dns_rr_type t)
{
    switch(t){
    case dns_r_srv:
	return new dns_srv_entry();
    case dns_r_a:
    //case dns_r_aaaa:
	return new dns_ip_entry();
    case dns_r_naptr:
	return new dns_naptr_entry();
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

string dns_entry::to_str()
{
    string res;

    for(vector<dns_base_entry*>::iterator it = ip_vec.begin();
	it != ip_vec.end(); it++) {
	
	if(it != ip_vec.begin())
	    res += ", ";

	res += (*it)->to_str();
    }

    return "[" + res + "]";
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

    u_int64_t now = wheeltimer::instance()->unix_clock.get();
    if(now >= e->expire){
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
    switch(type){
    case IPv4:
	{
	    sockaddr_in* sa_in = (sockaddr_in*)sa;
	    sa_in->sin_family = AF_INET;
	    memcpy(&(sa_in->sin_addr),&addr,sizeof(in_addr));
	} break;
    case IPv6:
	{
	    sockaddr_in6* sa_in6 = (sockaddr_in6*)sa;
	    sa_in6->sin6_family = AF_INET6;
	    memcpy(&(sa_in6->sin6_addr),&addr6,sizeof(in6_addr));
	} break;
    default:
	break;
    }
}

string ip_entry::to_str()
{
    if(type == IPv4) {
	u_char* cp = (u_char*)&addr;
	return int2str(cp[0]) + 
	    "." + int2str(cp[1]) + 
	    "." + int2str(cp[2]) + 
	    "." + int2str(cp[3]);
    }
    else {
	// not supported yet...
	return "[IPv6]";
    }
}


void ip_port_entry::to_sa(sockaddr_storage* sa)
{
    switch(type){
    case IPv4:
	{
	    sockaddr_in* sa_in = (sockaddr_in*)sa;
	    sa_in->sin_family = AF_INET;
	    memcpy(&(sa_in->sin_addr),&addr,sizeof(in_addr));
	    if(port) {
		sa_in->sin_port = htons(port);
	    }
	    else {
		sa_in->sin_port = htons(5060);
	    }
	} break;
    case IPv6:
	{
	    sockaddr_in6* sa_in6 = (sockaddr_in6*)sa;
	    sa_in6->sin6_family = AF_INET6;
	    memcpy(&(sa_in6->sin6_addr),&addr6,sizeof(in6_addr));
	    sa_in6->sin6_port = htons(port);
	} break;
    default:
	break;
    }
}

string ip_port_entry::to_str()
{
    return ip_entry::to_str() + ":" + int2str(port);
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
    	dns_get_16(rdata),
    	dns_get_16(rdata+2),
    	dns_get_16(rdata+4),
    	name_buf);
    
    srv_entry* srv_r = new srv_entry();
    srv_r->p = dns_get_16(rdata);
    srv_r->w = dns_get_16(rdata+2);
    srv_r->port = dns_get_16(rdata+4);
    srv_r->target = (const char*)name_buf;

    return srv_r;
}

string srv_entry::to_str()
{
    return target + ":" + int2str(port)
	+ "/" + int2str(p)
	+ "/" + int2str(w);
};

struct dns_search_h
{
    dns_entry_map entry_map;
    uint64_t      now;

    dns_search_h() {
	now = wheeltimer::instance()->unix_clock.get();
    }
};

int rr_to_dns_entry(dns_record* rr, dns_section_type t,
		    u_char* begin, u_char* end, void* data)
{
    // only answer and additional sections
    if(t != dns_s_an && t != dns_s_ar)
	return 0;

    dns_search_h* h = (dns_search_h*)data;
    string name = ns_rr_name(*rr);

    dns_entry* dns_e = NULL;
    dns_entry_map::iterator it = h->entry_map.find(name);

    if(it == h->entry_map.end()) {
	dns_e = dns_entry::make_entry((dns_rr_type)rr->type);
	if(!dns_e) {
	    // unsupported record type
	    return 0;
	}
	h->entry_map.insert(name,dns_e);
    }
    else {
	dns_e = it->second;
    }

    dns_e->add_rr(rr,begin,end,h->now);
    return 0;
}

dns_handle::dns_handle() 
  : srv_e(0), srv_n(0), ip_e(0), ip_n(0) 
{}

dns_handle::dns_handle(const dns_handle& h)
{
    *this = h;
}

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

const dns_handle& dns_handle::operator = (const dns_handle& rh)
{
    memcpy(this,(const void*)&rh,sizeof(dns_handle));
    
    if(srv_e)
	inc_ref(srv_e);
    
    if(ip_e)
	inc_ref(ip_e);
    
    return *this;
}

static bool naptr_less(const dns_base_entry* le, const dns_base_entry* re)
{
    const naptr_record* l_naptr = (const naptr_record*)le;
    const naptr_record* r_naptr = (const naptr_record*)re;

    if(l_naptr->order != r_naptr->order)
	return l_naptr->order < r_naptr->order;
    else
	return l_naptr->pref < r_naptr->pref;
}

void dns_naptr_entry::init()
{
    stable_sort(ip_vec.begin(),ip_vec.end(),naptr_less);
}

dns_base_entry* dns_naptr_entry::get_rr(dns_record* rr, u_char* begin, u_char* end)
{
    enum NAPTR_FieldIndex {
	NAPTR_Flags       = 0,
	NAPTR_Services    = 1,
	NAPTR_Regexp      = 2,
	NAPTR_Replacement = 3,
	NAPTR_Fields
    };

    if(rr->type != dns_r_naptr)
	return NULL;

    const u_char * rdata = ns_rr_rdata(*rr);

    unsigned short order = dns_get_16(rdata);
    rdata += 2;

    unsigned short pref = dns_get_16(rdata);
    rdata += 2;

    cstring fields[NAPTR_Fields];

    for(int i=0; i < NAPTR_Fields; i++) {

	if(rdata > end) {
	    ERROR("corrupted NAPTR record!!\n");
	    return NULL;
	}

	fields[i].len = *(rdata++);
	fields[i].s = (const char*)rdata;

	rdata += fields[i].len;
    }

    printf("ENUM: TTL=%i P=<%i> W=<%i>"
	   " FL=<%.*s> S=<%.*s>"
	   " REG=<%.*s> REPL=<%.*s>\n",
	   ns_rr_ttl(*rr),
	   order, pref,
	   fields[NAPTR_Flags].len,       fields[NAPTR_Flags].s,
	   fields[NAPTR_Services].len,    fields[NAPTR_Services].s,
	   fields[NAPTR_Regexp].len,      fields[NAPTR_Regexp].s,
	   fields[NAPTR_Replacement].len, fields[NAPTR_Replacement].s);

    naptr_record* naptr_r = new naptr_record();
    naptr_r->order = order;
    naptr_r->pref  = pref;
    naptr_r->flags = c2stlstr(fields[NAPTR_Flags]);
    naptr_r->services = c2stlstr(fields[NAPTR_Services]);
    naptr_r->regexp = c2stlstr(fields[NAPTR_Regexp]);
    naptr_r->replace = c2stlstr(fields[NAPTR_Replacement]);

    return naptr_r;
}

sip_target::sip_target() {}

sip_target::sip_target(const sip_target& target)
{
    *this = target;
}

const sip_target& sip_target::operator = (const sip_target& target)
{
    memcpy(&ss,&target.ss,sizeof(sockaddr_storage));
    memcpy(trsp,target.trsp,SIP_TRSP_SIZE_MAX+1);
    return target;
}

void sip_target::clear()
{
    memset(&ss,0,sizeof(sockaddr_storage));
    memset(trsp,'\0',SIP_TRSP_SIZE_MAX+1);
}

sip_target_set::sip_target_set()
    : dest_list(),
      dest_list_it(dest_list.begin())
{}

void sip_target_set::reset_iterator()
{
    dest_list_it = dest_list.begin();
}

bool sip_target_set::has_next()
{
    return dest_list_it != dest_list.end();
}

int sip_target_set::get_next(sockaddr_storage* ss, cstring& next_trsp,
			     unsigned int flags)
{
    do {
	if(!has_next())
	    return -1;

	sip_target& t = *dest_list_it;
	memcpy(ss,&t.ss,sizeof(sockaddr_storage));
	next_trsp = cstring(t.trsp);

	next();

	// set default transport to UDP
	if(!next_trsp.len)
	    next_trsp = cstring("udp");

    } while(!(flags & TR_FLAG_DISABLE_BL) &&
	    tr_blacklist::instance()->exist(ss));

    return 0;
}

bool sip_target_set::next()
{
    dest_list_it++;
    return has_next();
}

void sip_target_set::debug()
{
    DBG("target list:");

    for(list<sip_target>::iterator it = dest_list.begin();
	it != dest_list.end(); it++) {

	DBG("\t%s:%u/%s to target list",
	    am_inet_ntop(&it->ss).c_str(),
	    am_get_port(&it->ss),it->trsp);
    }
}

dns_entry_map::dns_entry_map()
    : map<string,dns_entry*>()
{
}

dns_entry_map::~dns_entry_map()
{
    for(iterator it = begin(); it != end(); ++it) {
	dec_ref(it->second);
    }
}

std::pair<dns_entry_map::iterator, bool>
dns_entry_map::insert(const dns_entry_map::value_type& x)
{
    return dns_entry_map_base::insert(x);
}

bool dns_entry_map::insert(const string& key, dns_entry* e)
{
    std::pair<iterator, bool> res =
    	insert(make_pair<const key_type&,mapped_type>(key,e));

    if(res.second) {
	inc_ref(e);
	return true;
    }

    return false;
}

dns_entry* dns_entry_map::fetch(const key_type& key)
{
    iterator it = find(key);
    if(it != end())
	return it->second;
    return NULL;
}

bool _resolver::disable_srv = false;

_resolver::_resolver()
    : cache(DNS_CACHE_SIZE)
{
    start();
}

_resolver::~_resolver()
{
    
}

int _resolver::query_dns(const char* name, dns_entry_map& entry_map, dns_rr_type t)
{
    u_char dns_res[NS_PACKETSZ];

    if(!name) return -1;

    DBG("Querying '%s' (%s)...",name,dns_rr_type_str(t));

    int dns_res_len = res_search(name,ns_c_in,(ns_type)t,
				 dns_res,NS_PACKETSZ);
    if(dns_res_len < 0){
	dns_error(h_errno,name);
	return -1;
    }

    /*
     * Initialize a handle to this response.  The handle will
     * be used later to extract information from the response.
     */
    dns_search_h h;
    if (dns_msg_parse(dns_res, dns_res_len, rr_to_dns_entry, &h) < 0) {
	DBG("Could not parse DNS reply");
	return -1;
    }

    for(dns_entry_map::iterator it = h.entry_map.begin();
	it != h.entry_map.end(); it++) {

	dns_entry* e = it->second;
	if(!e || e->ip_vec.empty()) continue;

	e->init();
	entry_map.insert(it->first,e);
    }

    return 0;
}

int _resolver::resolve_name(const char* name,
			    dns_handle* h,
			    sockaddr_storage* sa,
			    const address_type types,
			    dns_rr_type t)
{
    int ret;

    // already have a valid handle?
    if(h->valid()){
	if(h->eoip()) return -1;
	return h->next_ip(sa);
    }

    if(t != dns_r_srv &&
       t != dns_r_naptr) {

	// first try to detect if 'name' is already an IP address
	ret = am_inet_pton(name,sa);
	if(ret == 1) {
	    h->ip_n = -1; // flag end of IP list
	    h->srv_n = -1;
	    return 0; // 'name' is an IP address
	}
    }
    
    // name is NOT an IP address -> try a cache look up
    dns_bucket* b = cache.get_bucket(hashlittle(name,strlen(name),0));
    dns_entry* e = b->find(name);

    // first attempt to get a valid IP
    // (from the cache)
    if(e){
	int ret = e->next_ip(h,sa);
	dec_ref(e);
	return ret;
    }

    // no valid IP, query the DNS
    dns_entry_map entry_map;
    if(query_dns(name,entry_map,t) < 0) {
	return -1;
    }

    for(dns_entry_map::iterator it = entry_map.begin();
	it != entry_map.end(); it++) {

	if(!it->second) continue;

	b = cache.get_bucket(hashlittle(it->first.c_str(),
					it->first.length(),0));
	// cache the new record
	if(b->insert(it->first,it->second)) {
	    // cache insert successful
	    DBG("new DNS cache entry: '%s' -> %s",
		it->first.c_str(), it->second->to_str().c_str());
	}
    }

    e = entry_map.fetch(name);
    if(e) {
	// now we should have a valid IP
	return e->next_ip(h,sa);
    }

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
	    ERROR("while trying to detect an IPv4 address '%s': %s",
		  name,strerror(errno));
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
	    ERROR("while trying to detect an IPv6 address '%s': %s",
		  name,strerror(errno));
	    return ret;
	}
    }

    return 0;
}

int _resolver::set_destination_ip(const cstring& next_hop,
				  unsigned short next_port,
				  const cstring& next_trsp,
				  sockaddr_storage* remote_ip,
				  dns_handle* h_dns)
{

    string nh = c2stlstr(next_hop);

    DBG("checking whether '%s' is IP address...\n", nh.c_str());
    if (am_inet_pton(nh.c_str(), remote_ip) != 1) {

	// nh does NOT contain a valid IP address
    
	if(!next_port) {
	    // no explicit port specified,
	    // try SRV first
	    if (disable_srv) {
		DBG("no port specified, but DNS SRV disabled (skipping).\n");
	    } else {
		string srv_name = "_sip._";
		if(!next_trsp.len || !lower_cmp_n(next_trsp,"udp")){
		    srv_name += "udp";
		}
		else if(!lower_cmp_n(next_trsp,"tcp")) {
		    srv_name += "tcp";
		}
		else {
		    DBG("unsupported transport: skip SRV lookup");
		    goto no_SRV;
		}

		srv_name += "." + nh;

		DBG("no port specified, looking up SRV '%s'...\n",
		    srv_name.c_str());

		if(!resolver::instance()->resolve_name(srv_name.c_str(),
						       h_dns,remote_ip,
						       IPv4,dns_r_srv)){
		    return 0;
		}

		DBG("no SRV record for %s",srv_name.c_str());
	    }
	}

    no_SRV:
	memset(remote_ip,0,sizeof(sockaddr_storage));
	int err = resolver::instance()->resolve_name(nh.c_str(),
						     h_dns,remote_ip,
						     IPv4);
	if(err < 0){
	    ERROR("Unresolvable Request URI domain\n");
	    return -478;
	}
    }
    else {
	am_set_port(remote_ip,next_port);
    }

    if(!am_get_port(remote_ip)) {
	if(!next_port) next_port = 5060;
	am_set_port(remote_ip,next_port);
    }

    DBG("set destination to %s:%u\n",
	nh.c_str(), am_get_port(remote_ip));
    
    return 0;
}

int _resolver::resolve_targets(const list<sip_destination>& dest_list,
			       sip_target_set* targets)
{
    for(list<sip_destination>::const_iterator it = dest_list.begin();
	it != dest_list.end(); it++) {
	
	sip_target t;
	dns_handle h_dns;

	DBG("sip_destination: %.*s:%u/%.*s",
	    it->host.len,it->host.s,
	    it->port,
	    it->trsp.len,it->trsp.s);

	if(set_destination_ip(it->host,it->port,it->trsp,&t.ss,&h_dns) != 0) {
	    ERROR("Unresolvable destination");
	    return -478;
	}
	if(it->trsp.len && (it->trsp.len <= SIP_TRSP_SIZE_MAX)) {
	    memcpy(t.trsp,it->trsp.s,it->trsp.len);
	    t.trsp[it->trsp.len] = '\0';
	}
	else {
	    t.trsp[0] = '\0';
	}

	do {
	    targets->dest_list.push_back(t);

	} while(h_dns.next_ip(&t.ss) == 0);
    }

    return 0;
}

void _resolver::run()
{
    struct timespec tick,rem;
    tick.tv_sec  = (DNS_CACHE_SINGLE_CYCLE/1000000L);
    tick.tv_nsec = (DNS_CACHE_SINGLE_CYCLE - (tick.tv_sec)*1000000L) * 1000L;

    unsigned long i = 0;
    for(;;) {
	nanosleep(&tick,&rem);

	u_int64_t now = wheeltimer::instance()->unix_clock.get();
	dns_bucket* bucket = cache.get_bucket(i);

	bucket->lock();
	    
	for(dns_bucket::value_map::iterator it = bucket->elmts.begin();
	    it != bucket->elmts.end(); ++it){

	    dns_entry* dns_e = (dns_entry*)it->second;
	    if(now >= it->second->expire){

		dns_bucket::value_map::iterator tmp_it = it;
		bool end_of_bucket = (++it == bucket->elmts.end());

		DBG("DNS record expired (%p)",dns_e);
		bucket->elmts.erase(tmp_it);
		dec_ref(dns_e);

		if(end_of_bucket) break;
	    }
	    else {
		//DBG("######### record %p expires in %li seconds ##########",
		//    dns_e,it->second->expire-tv_now.tv_sec);
	    }
	}

	bucket->unlock();

	if(++i >= cache.get_size()) i = 0;
    }
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
