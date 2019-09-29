/*
 * $Id: trans_layer.cpp 1713 2010-03-30 14:11:14Z rco $
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

#include "trans_layer.h"
#include "sip_parser.h"
#include "trans_table.h"
#include "parse_cseq.h"
#include "parse_from_to.h"
#include "parse_route.h"
#include "parse_100rel.h"
#include "parse_extensions.h"
#include "parse_next_hop.h"
#include "sip_trans.h"
#include "msg_fline.h"
#include "msg_hdrs.h"
#include "udp_trsp.h"
#include "ip_util.h"
#include "resolver.h"
#include "sip_ua.h"
#include "msg_logger.h"

#include "wheeltimer.h"
#include "sip_timers.h"
#include "tr_blacklist.h"

#define DEFAULT_BL_TTL 60000 /* 60s */

#include "log.h"

#include "AmUtils.h"
#include "AmConfig.h"
#include "AmSipEvent.h"

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

#include <algorithm>

bool _trans_layer::accept_fr_without_totag = false;
unsigned int _trans_layer::default_bl_ttl = DEFAULT_BL_TTL;

bool _trans_layer::less_case_i::operator () (const string& lhs, const string& rhs) const
{
    return lower_cmp_n(lhs.c_str(),lhs.length(),
		       rhs.c_str(),rhs.length()) < 0;
}

_trans_layer::_trans_layer()
    : ua(NULL),
      transports()
{
}

_trans_layer::~_trans_layer()
{}


void _trans_layer::register_ua(sip_ua* ua)
{
    this->ua = ua;
}

int _trans_layer::register_transport(trsp_socket* trsp)
{
    int if_num = trsp->get_if();
    if(transports.size() <= (size_t)if_num)
	transports.resize(if_num+1);

    if(transports[if_num].find(trsp->get_transport())
       != transports[if_num].end()) {
	WARN("transport already registered for this interface");
	return -1;
    }

    transports[if_num][trsp->get_transport()] = trsp;
    return 0;
}

void _trans_layer::clear_transports()
{
    transports.clear();
}

int _trans_layer::set_trsp_socket(sip_msg* msg, const cstring& next_trsp,
				  int out_interface)
{
    if((out_interface < 0)
       || ((unsigned int)out_interface >= transports.size())) {

	out_interface = find_outbound_if(&msg->remote_ip);
	if(out_interface < 0) {
	    DBG("could not find any suitable outbound interface");
	    return -1;
	}
    }

    if(transports[out_interface].empty()) {
	ERROR("no transport for this interface");
	return -1;
    }

    prot_collection::iterator prot_sock_it =
	transports[out_interface].find(c2stlstr(next_trsp));

    if(prot_sock_it == transports[out_interface].end()) {

	DBG("could not find transport '%.*s' in outbound interface %i",
	    next_trsp.len,next_trsp.s,out_interface);

	prot_sock_it = transports[out_interface].find("udp");
	
	// if we couldn't find anything, take whatever is there...
	if(prot_sock_it == transports[out_interface].end()) {
	    DBG("could not find transport 'udp' in outbound interface %i",
		out_interface);
	    prot_sock_it = transports[out_interface].begin();
	}
    }

    if(msg->local_socket) dec_ref(msg->local_socket);
    msg->local_socket = prot_sock_it->second;
    inc_ref(msg->local_socket);

    return 0;
}

static int patch_contact_transport(sip_header* contact, const cstring& trsp,
				   string& n_contact)
{
    DBG("contact: <%.*s>", contact->value.len, contact->value.s);

    list<cstring> contact_list;
    if(parse_nameaddr_list(contact_list, contact->value.s,
			   contact->value.len) < 0) {
	DBG("Could not parse contact list\n");
	return -1;
    }

    const char* marker = contact->value.s;
    const char* hf_end = marker + contact->value.len;

    for(list<cstring>::iterator ct_it = contact_list.begin();
	ct_it != contact_list.end(); ct_it++) {

	sip_nameaddr na;
	const char* c = ct_it->s;
	if(parse_nameaddr_uri(&na,&c,ct_it->len) < 0) {
	    DBG("Could not parse nameaddr & URI (%.*s)\n",ct_it->len,ct_it->s);
	    return -1;
	}

	bool found_trsp = false;
	for(list<sip_avp*>::iterator p_it = na.uri.params.begin();
	    p_it != na.uri.params.end(); p_it++) {

	    if(!lower_cmp_n((*p_it)->name.s,(*p_it)->name.len,"transport",9)) {
		found_trsp = true;
		if(lower_cmp_n((*p_it)->value.s,(*p_it)->value.len,
			     trsp.s,trsp.len)) {
		    // copy everything from last marker until param value
		    n_contact.append(marker, (*p_it)->value.s - marker);
		    // copy param value
		    n_contact.append(trsp.s, trsp.len);
		    // set marker after transport value
		    marker = (*p_it)->value.s + (*p_it)->value.len;
		}
	    }
	}

	if(!found_trsp){
	    // copy everything from last marker until end of addr
	    n_contact.append(marker, na.addr.s + na.addr.len - marker);
	    // copy new param + value
	    n_contact.append(";transport=");
	    n_contact.append(trsp.s, trsp.len);
	    // set marker after transport value
	    marker = na.addr.s + na.addr.len;
	}
    }

    if(!n_contact.empty()) {
	// finish copy
	n_contact.append(marker, hf_end - marker);
	contact->value = stl2cstr(n_contact);
    }

    return 0;
}

int _trans_layer::send_reply(sip_msg* msg, const trans_ticket* tt,
			     const cstring& dialog_id, const cstring& to_tag,
			     msg_logger* logger)
{
    // Ref.: RFC 3261 8.2.6, 12.1.1
    //
    // Fields to copy (from RFC 3261):
    //  - From
    //  - Call-ID
    //  - CSeq
    //  - Vias (same order)
    //  - To (+ tag if not yet present in request)
    //  - (if a dialog is created) Record-Route
    //
    // Fields to generate (if INVITE transaction):
    //    - Contact
    //    - Route: copied from 
    // 
    // SHOULD be contained:
    //  - Allow, Supported
    //
    // MAY be contained:
    //  - Accept

    assert(tt);

    if (!tt->_bucket || !tt->_t) {
	ERROR("Invalid transaction ticket\n");
	return -1;
    }

    trans_bucket* bucket = tt->_bucket;
    sip_trans*    t = tt->_t;

    bucket->lock();
    if(!bucket->exist(t)){
	bucket->unlock();
	ERROR("Invalid transaction key: transaction does not exist (%p;%p)\n",bucket,t);
	return -1;
    }

    if(t->reply_status >= 200){
	bucket->unlock();
	ERROR("Transaction has already been closed with a final reply\n");
	return -1;
    }

    sip_msg* req = t->msg;
    assert(req);

	    // patch Contact-HF
    vector<string> contact_buf;
    trsp_socket* local_socket = req->local_socket;
    if(!local_socket->is_opt_set(trsp_socket::no_transport_in_contact)) {
	    cstring trsp(local_socket->get_transport());

	    contact_buf.resize(msg->contacts.size());
	    vector<string>::iterator contact_buf_it = contact_buf.begin();

	    for(list<sip_header*>::iterator contact_it = msg->contacts.begin();
		contact_it != msg->contacts.end(); contact_it++, contact_buf_it++) {
	
		patch_contact_transport(*contact_it,trsp,*contact_buf_it);
	    }
    }
    
    bool have_to_tag = false;
    int  reply_len   = status_line_len(msg->u.reply->reason);

    // add 'received' should be added
    // check if first Via has rport parameter

    assert(req->via1);
    assert(req->via_p1);

    unsigned int new_via1_len = copy_hdr_len(req->via1);
    string remote_ip_str = get_addr_str(&req->remote_ip);

    bool append_received = !(req->via_p1->host == remote_ip_str.c_str());
    if(append_received) {
	new_via1_len += 10/*;received=*/ + remote_ip_str.length();
    }

    // needed if rport parameter was present but empty
    string remote_port_str;
    if(req->via_p1->has_rport) {
	if(!req->via_p1->rport.len){
	    remote_port_str = int2str(ntohs(((sockaddr_in*)&req->remote_ip)->sin_port));
	    new_via1_len += remote_port_str.length() + 1/* "=<port number>" */;
	}
    }

    unsigned int rel100_ext = 0;
    unsigned int rseq = 0;
    int reply_code = msg->u.reply->code;
    
    // copy necessary headers
    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	assert((*it));
	switch((*it)->type){

	case sip_header::H_VIA:
	    // if first via, take the possibly modified one
	    if((*it) == req->via1)
		reply_len += new_via1_len;
	    else
		reply_len += copy_hdr_len(*it);
	    break;

	case sip_header::H_TO:
	    if (!(*it)->p) break; // ignore if not parsed
	    if(to_tag.len) {
		if(! ((sip_from_to*)(*it)->p)->tag.len ) {
		    
		    reply_len += 5/* ';tag=' */
			+ to_tag.len; 
		}
		else {
		    // To-tag present in request...
		    have_to_tag = true;
		    // ... save it:
		    t->to_tag = ((sip_from_to*)(*it)->p)->tag;
		}
	    }
	    else if(reply_code >= 300) {
		// Let final error replies clear 
		// the to-tag if not present:
		// (necessary to match pre-RFC3261 non-200 ACKs)
		t->to_tag.clear();
	    }
	    reply_len += copy_hdr_len(*it);
	    break;

	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_RECORD_ROUTE:
	    reply_len += copy_hdr_len(*it);
	    break;

	case sip_header::H_REQUIRE:
	    if (rel100_ext)
		// there was already a(nother?) Require HF
		continue;
	    if(!parse_extensions(&rel100_ext, (*it)->value.s, (*it)->value.len)) {
		ERROR("failed to parse(own?) 'Require' hdr.\n");
		continue;
	    }

	    rel100_ext = rel100_ext & SIP_EXTENSION_100REL;

	    if (rel100_ext && rseq) { // our RSeq's are never 0
		t->last_rseq = rseq;
		continue; // the end.
	    }
	    break;

	case sip_header::H_RSEQ:
	    if (rseq) {
		ERROR("multiple 'RSeq' headers in reply.\n");
		continue;
	    }
	    if (!parse_rseq(&rseq, (*it)->value.s, (*it)->value.len)) {
		ERROR("failed to parse (own?) 'RSeq' hdr.\n");
		continue;
	    }
	    if (rel100_ext) {
		t->last_rseq = rseq;
		continue; // the end.
	    }
	    break;
	}
    }

    reply_len += copy_hdrs_len(msg->hdrs);

    string c_len = int2str(msg->body.len);
    reply_len += content_length_len((char*)c_len.c_str());

    if(msg->body.len){
	
	reply_len += msg->body.len;
    }

    reply_len += 2/*CRLF*/;
    
    // Allocate buffer for the reply
    //
    char* reply_buf = new char[reply_len];
    char* c = reply_buf;

    DBG("reply_len = %i\n",reply_len);

    status_line_wr(&c,reply_code,msg->u.reply->reason);

    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	switch((*it)->type){

	case sip_header::H_VIA:
	    
	    if((*it) == req->via1) {// 1st Via
		
		// move this code to something like:
		// write_reply_via(old_via1,old_via_p1,remote_ip_str,remote_port_str)

		unsigned int len;

		memcpy(c,(*it)->name.s,(*it)->name.len);
		c += (*it)->name.len;
		
		*(c++) = ':';
		*(c++) = SP;
		
		if(req->via_p1->has_rport && !req->via_p1->rport.len){

		    // copy everything from the beginning up to the "rport" param:
		    len = (req->via_p1->rport.s + req->via_p1->rport.len) - req->via1->value.s;
		    memcpy(c,req->via1->value.s,len);
		    c += len;

		    // add '='
		    *(c++) = '=';

		    // add the remote port
		    memcpy(c,remote_port_str.c_str(),remote_port_str.length());
		    c += remote_port_str.length();

		    //copy up to the end of the first Via parm
		    len = req->via_p1->eop - (req->via_p1->rport.s + req->via_p1->rport.len);
		    memcpy(c,req->via_p1->rport.s + req->via_p1->rport.len, len);
		    c += len;
		}
		else {
		    //copy up to the end of the first Via parm
		    len = req->via_p1->eop - req->via1->value.s;
		    memcpy(c,req->via1->value.s,len);
		    c += len;
		}

		if(append_received) {

		    memcpy(c,";received=",10);
		    c += 10;

		    memcpy(c,remote_ip_str.c_str(),remote_ip_str.length());
		    c += remote_ip_str.length();
		}


		//copy the rest of the first Via header 
		len = req->via1->value.s + req->via1->value.len - req->via_p1->eop;
		memcpy(c,req->via_p1->eop,len);
		c += len;

		*(c++) = CR;
		*(c++) = LF;
	    }
	    else {
		copy_hdr_wr(&c,*it);
	    }
	    break;

	case sip_header::H_TO:
	    if (!(*it)->p) break; // ignore if not parsed
	    if(!to_tag.len || have_to_tag){
		copy_hdr_wr(&c,*it);
	    }
	    else {
		memcpy(c,(*it)->name.s,(*it)->name.len);
		c += (*it)->name.len;
		
		*(c++) = ':';
		*(c++) = SP;
		
		memcpy(c,(*it)->value.s,(*it)->value.len);
		c += (*it)->value.len;
		
		memcpy(c,";tag=",5);
		c += 5;

		t->to_tag.s = c;
		t->to_tag.len = to_tag.len;

		memcpy(c,to_tag.s,to_tag.len);
		c += to_tag.len;

		*(c++) = CR;
		*(c++) = LF;
	    }
	    break;

	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_RECORD_ROUTE:
	    copy_hdr_wr(&c,*it);
	    break;
	}
    }

    copy_hdrs_wr(&c,msg->hdrs);
    content_length_wr(&c,(char*)c_len.c_str());

    *c++ = CR;
    *c++ = LF;

    if(msg->body.len){
	memcpy(c,msg->body.s,msg->body.len);
    }

    int err = -1;

    // Inspect topmost 'Via' and select proper addr (TODO: resolve DNS names)
    // refs: RFC3261 18.2.2; RFC3581

    sockaddr_storage remote_ip;
    if(!local_socket) {
	
	ERROR("request to be replied has no transport socket set\n");
	delete [] reply_buf;
	goto end;
    }

    memcpy(&remote_ip,&req->remote_ip,sizeof(sockaddr_storage));

    // force_via_address option? send to 1st via
    if(local_socket->is_opt_set(trsp_socket::force_via_address)) {
	string via_host = c2stlstr(req->via_p1->host);
	DBG("force_via_address: setting remote IP to via '%s'\n", via_host.c_str());
	if (resolver::instance()->str2ip(via_host.c_str(), &remote_ip,
					 (address_type)(IPv4 | IPv6)) != 1) {
	    ERROR("Invalid via_host '%s'\n", via_host.c_str());
	    delete [] reply_buf;
	    goto end;
	}
    }

    if(local_socket->is_opt_set(trsp_socket::force_via_address)) {

	if(req->via_p1->has_rport){
	
	    if(req->via_p1->rport_i){
		// use 'rport'
		((sockaddr_in*)&remote_ip)->sin_port = htons(req->via_p1->rport_i);
	    }
	    // else: use the source port from the replied request (from IP hdr)
	    //       (already set).
	}
	else {
	
	    if(req->via_p1->port_i){
		// use port from 'sent-by' via address
		((sockaddr_in*)&remote_ip)->sin_port = htons(req->via_p1->port_i);
	    }
	    else {
		// use 5060
		((sockaddr_in*)&remote_ip)->sin_port = htons(5060);
	    }
	}
    }

    DBG("Sending to %s:%i <%.*s...>\n",
	get_addr_str(&remote_ip).c_str(),
	ntohs(((sockaddr_in*)&remote_ip)->sin_port),
	50 /* preview - instead of p_msg->len */,reply_buf);

    //TODO: pass send-flags down to here
    err = local_socket->send(&remote_ip,reply_buf,reply_len,0);
    if(err < 0){
	ERROR("could not send to %s:%i <%.*s...>\n",
	      get_addr_str(&remote_ip).c_str(),
	      ntohs(((sockaddr_in*)&remote_ip)->sin_port),
	      50 /* preview - instead of p_msg->len */,reply_buf);

	delete [] reply_buf;

	if(!local_socket->is_reliable()) {
	    // set timer to capture retransmissions
	    // and delete transaction afterwards
	    t->reset_timer(STIMER_J,J_TIMER,bucket->get_id()); 
	}
	else {
	    // reliable transport
	    bucket->remove(t);
	}
	goto end;
    }

    stats.inc_sent_replies();

    if (t->retr_buf) {
	// delete old retry-buffer 
	// before overwriting it
	delete [] t->retr_buf;
    }

    t->retr_buf = reply_buf;
    t->retr_len = reply_len;
    memcpy(&t->retr_addr,&remote_ip,sizeof(sockaddr_storage));
    inc_ref(local_socket);
    if(t->retr_socket) dec_ref(t->retr_socket);
    t->retr_socket = local_socket;

    if(logger) {
	sockaddr_storage src_ip;
	local_socket->copy_addr_to(&src_ip);
	logger->log(reply_buf,reply_len,&src_ip,&remote_ip,
		    req->u.request->method_str,reply_code);

	if(!t->logger){
	    t->logger = logger;
	    inc_ref(logger);
	}
    }

    if(update_uas_reply(bucket,t,reply_code) == TS_REMOVED)
	goto end;

    if(dialog_id.len && !(t->dialog_id.len)) {
	t->dialog_id.s = new char[dialog_id.len];
	t->dialog_id.len = dialog_id.len;
	memcpy((void*)t->dialog_id.s,dialog_id.s,dialog_id.len);
    }
    
 end:
    bucket->unlock();
    return err;
}

int _trans_layer::send_sf_error_reply(const trans_ticket* tt, const sip_msg* req,
				      int reply_code, const cstring& reason,
				      const cstring& hdrs, const cstring& body)
{
    char to_tag_buf[SL_TOTAG_LEN];
    cstring to_tag(to_tag_buf,SL_TOTAG_LEN);
    compute_sl_to_tag(to_tag_buf,req);

    sip_msg reply;
    reply.u.reply = new sip_reply(reply_code,reason);

    char* c = (char*)hdrs.s;
    int err = parse_headers(&reply,&c,c+hdrs.len);
    if(err){
	ERROR("Malformed additional header\n");
	return -1;
    }
    reply.body = body;

    return send_reply(&reply,tt,cstring(),to_tag);
}

int _trans_layer::send_sl_reply(sip_msg* req, int reply_code, 
			       const cstring& reason, const cstring& hdrs, 
			       const cstring& body)
{
    // Ref.: RFC 3261 8.2.6, 12.1.1
    //
    // Fields to copy (from RFC 3261):
    //  - From
    //  - Call-ID
    //  - CSeq
    //  - Vias (same order)
    //  - To (+ tag if not yet present in request)
    //  - (if a dialog is created) Record-Route
    //
    // Fields to generate (if INVITE transaction):
    //    - Contact
    //    - Route: copied from 
    // 
    // SHOULD be contained:
    //  - Allow, Supported
    //
    // MAY be contained:
    //  - Accept

    assert(req);

    bool have_to_tag = false;
    int  reply_len   = status_line_len(reason);

    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	assert(*it);
	switch((*it)->type){

	case sip_header::H_TO:

	    if((!(*it)->p) || (!((sip_from_to*)(*it)->p)->tag.len) ) {

		reply_len += 5/* ';tag=' */
		    + SL_TOTAG_LEN; 
	    }
	    else {
		// To-tag present in request
		have_to_tag = true;
	    }
	    // fall-through-trap
	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_VIA:
	case sip_header::H_RECORD_ROUTE:
	    reply_len += copy_hdr_len(*it);
	    break;
	}
    }

    reply_len += hdrs.len;

    string c_len = int2str(body.len);
    reply_len += content_length_len((char*)c_len.c_str());

    if(body.len){
	
	reply_len += body.len;
    }

    reply_len += 2/*CRLF*/;
    
    // Allocate buffer for the reply
    //
    char* reply_buf = new char[reply_len];
    char* c = reply_buf;

    status_line_wr(&c,reply_code,reason);

    for(list<sip_header*>::iterator it = req->hdrs.begin();
	it != req->hdrs.end(); ++it) {

	switch((*it)->type){

	case sip_header::H_TO:

	    if(have_to_tag){
		copy_hdr_wr(&c,*it);
	    }
	    else {
		memcpy(c,(*it)->name.s,(*it)->name.len);
		c += (*it)->name.len;
		
		*(c++) = ':';
		*(c++) = SP;
		
		memcpy(c,(*it)->value.s,(*it)->value.len);
		c += (*it)->value.len;
	    
		memcpy(c,";tag=",5);
		c += 5;

		char to_tag[SL_TOTAG_LEN];
		compute_sl_to_tag(to_tag,req);
		memcpy(c,to_tag,SL_TOTAG_LEN);
		c += SL_TOTAG_LEN;

		*(c++) = CR;
		*(c++) = LF;
	    }
	    break;

	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
	case sip_header::H_VIA:
	case sip_header::H_RECORD_ROUTE:
	    copy_hdr_wr(&c,*it);
	    break;
	}
    }

    if (hdrs.len) {
	memcpy(c,hdrs.s,hdrs.len);
	c += hdrs.len;
    }

    content_length_wr(&c,(char*)c_len.c_str());

    *c++ = CR;
    *c++ = LF;

    if(body.len){
	
	memcpy(c,body.s,body.len);
    }

    assert(req->local_socket);

    int err = req->local_socket->send(&req->remote_ip,reply_buf,reply_len,0);
    delete [] reply_buf;

    stats.inc_sent_replies();

    return err;
}


static void prepare_strict_routing(sip_msg* msg, string& ext_uri_buffer)
{
    if(msg->route.empty())
	return;

    sip_header* fr = msg->route.front();
    sip_uri* route_uri = get_first_route_uri(fr);

    // Loose routing is used,
    // no need for further processing
    if(!route_uri || is_loose_route(route_uri))
	return;

    sip_route* route = (sip_route*)fr->p;
    sip_nameaddr* fr_na = route->elmts.front()->addr;
    cstring fr_na_addr = fr_na->addr;

    if(route->elmts.size() == 1){
	// remove current route header from message
 	msg->route.pop_front();

	list<sip_header*>::iterator h_it = 
	    std::find(msg->hdrs.begin(),msg->hdrs.end(),fr);

	if(h_it != msg->hdrs.end()) 
	    msg->hdrs.erase(h_it);

 	delete fr;
    }
    else if(route->elmts.size() > 1) {
	// remove first element from the list
	delete route->elmts.front();
	route->elmts.pop_front();

	// fetch the next route element
	route_elmt* nxt_re = route->elmts.front();

 	// adjust route header
 	fr->value.len = (fr->value.s + fr->value.len) - nxt_re->route.s;
 	fr->value.s   = nxt_re->route.s;
    }

    // copy r_uri at the end of the route set.
    // ext_uri_buffer must have the same scope as 'msg'
    ext_uri_buffer = "<" + c2stlstr(msg->u.request->ruri_str) + ">";
    msg->hdrs.push_back(new sip_header(0,"Route",stl2cstr(ext_uri_buffer)));

    // and replace the R-URI with the first route URI 
    msg->u.request->ruri_str = fr_na_addr;
}


//
// Ref. RFC 3261 "12.2.1.1 Generating the Request"
//
int _trans_layer::set_next_hop(sip_msg* msg, 
			       cstring* next_hop,
			       unsigned short* next_port,
			       cstring* next_trsp)
{
    static const cstring default_trsp("udp");
    assert(msg);

    list<sip_header*>& route_hdrs = msg->route; 
    int err=0;

    if(!route_hdrs.empty()){
	
	sip_header* fr = route_hdrs.front();
	sip_uri* route_uri = get_first_route_uri(fr);
	if(route_uri == NULL) {
	    
	    DBG("Parsing 1st route uri failed\n");
	    return -1;
	}
	
	if (next_hop->len == 0) {
	    *next_hop  = route_uri->host;
	    if(route_uri->port_str.len)
		*next_port = route_uri->port;
	    if(route_uri->trsp && route_uri->trsp->value.len)
		*next_trsp = route_uri->trsp->value;
	}
    }
    else {

	sip_uri parsed_r_uri;
	cstring& r_uri = msg->u.request->ruri_str;

	err = parse_uri(&parsed_r_uri,r_uri.s,r_uri.len);
	if(err < 0){
	    ERROR("Invalid Request URI\n");
	    return -1;
	}
	DBG("setting next-hop based on request-URI\n");
	*next_hop  = parsed_r_uri.host;
	if(parsed_r_uri.port_str.len)
	    *next_port = parsed_r_uri.port;
	if(parsed_r_uri.trsp)
	    *next_trsp = parsed_r_uri.trsp->value;
    }

    if(!next_trsp->len) {
	DBG("no transport specified, setting default one (%.*s)",
	    default_trsp.len,default_trsp.s);
	*next_trsp = default_trsp;
    }

    DBG("next_hop:next_port is <%.*s:%u/%.*s>\n",
	next_hop->len, next_hop->s, *next_port,
	next_trsp ? next_trsp->len : 0,
	next_trsp ? next_trsp->s : 0);
    
    return 0;
}

static void set_err_reply_from_req(sip_msg* err, sip_msg* req,
				   int code, const char* reason)
{
    err->type = SIP_REPLY;
    err->u.reply = new sip_reply();
    err->u.reply->code = code;
    err->u.reply->reason = cstring(reason);

    // pre-parse message
    if(!req->from->p) {
	DBG("parsing From-HF...");
	req->from->p = new sip_from_to();
	parse_from_to((sip_from_to*)req->from->p,
		      req->from->value.s,req->from->value.len);
    }
    err->from = req->from;

    if(!req->to->p) {
	DBG("parsing To-HF...");
	req->to->p = new sip_from_to();
	parse_from_to((sip_from_to*)req->to->p,
		      req->to->value.s,req->to->value.len);
    }
    err->to = req->to;

    if(!req->cseq->p) {
	DBG("parsing CSeq-HF...");
	req->cseq->p = new sip_cseq();
	parse_cseq((sip_cseq*)req->cseq->p,
		   req->cseq->value.s,req->cseq->value.len);
    }
    err->cseq = req->cseq;
    err->callid = req->callid;
    err->via_p1 = req->via_p1;
}

void _trans_layer::transport_error(sip_msg* msg)
{
    char* err_msg=0;
    int ret = parse_sip_msg(msg,err_msg);
    if(ret){
	DBG("parse_sip_msg returned %i\n",ret);

	if(!err_msg){
	    err_msg = (char*)"unknown parsing error";
	}
	DBG("parsing error: %s\n",err_msg);

	DBG("Message was: \"%.*s\"\n",msg->len,msg->buf);
	return;
    }

    // transport errors for replies and ACK requests
    // should be silently dropped
    if((msg->type != SIP_REQUEST) ||
       (msg->u.request->method == sip_request::ACK))
	return;

    // generate error reply
    sip_msg* err = new sip_msg();
    set_err_reply_from_req(err,msg,503,"Transport Error");

    // err will be deleted there, as any received message
    process_rcvd_msg(err);
}

static void translate_string(sip_msg* dst_msg, cstring& dst,
			     const sip_msg* src_msg, const cstring& src)
{
    dst.s = (char*)src.s + (dst_msg->buf - src_msg->buf);
    dst.len = src.len;
}

static void translate_hdr(sip_msg* dst_msg, sip_header*& dst, 
			  const sip_msg* src_msg, const sip_header* src)
{
    dst = new sip_header();
    dst_msg->hdrs.push_back(dst);
    dst->type = src->type;
    translate_string(dst_msg,dst->name,src_msg,src->name);
    translate_string(dst_msg,dst->value,src_msg,src->value);
    dst->p = NULL;
}

static void gen_error_reply_from_req(sip_msg& reply, const sip_msg* req,
				     int code, const char* reason)
{
    reply.copy_msg_buf(req->buf,req->len);

    reply.type = SIP_REPLY;
    reply.u.reply = new sip_reply();

    reply.u.reply->code = code;
    reply.u.reply->reason = cstring(reason);

    translate_hdr(&reply,reply.from, req,req->from);
    reply.from->p = new sip_from_to();
    parse_from_to((sip_from_to*)reply.from->p,
		  reply.from->value.s,reply.from->value.len);

    translate_hdr(&reply,reply.to, req,req->to);
    reply.to->p = new sip_from_to();
    parse_from_to((sip_from_to*)reply.to->p,
		  reply.to->value.s,reply.to->value.len);

    translate_hdr(&reply,reply.cseq, req,req->cseq);
    reply.cseq->p = new sip_cseq();
    parse_cseq((sip_cseq*)reply.cseq->p,
	       reply.cseq->value.s,reply.cseq->value.len);

    translate_hdr(&reply,reply.callid, req,req->callid);
}

void _trans_layer::timeout(trans_bucket* bucket, sip_trans* t)
{
    t->reset_all_timers();
    t->state = TS_TERMINATED;

    // send 408 to 'ua'
    sip_msg reply;
    gen_error_reply_from_req(reply,t->msg,408,"Timeout");

    string dialog_id(t->dialog_id.s,t->dialog_id.len);

    if(t->flags & TR_FLAG_DISABLE_BL) {
	bucket->remove(t);
    }
    else {
	// set blacklist timer
	t->reset_timer(STIMER_BL,BL_TIMER,bucket->get_id());
    }
    bucket->unlock();

    ua->handle_sip_reply(dialog_id,&reply);
}

static int patch_ruri_with_remote_ip(string& n_uri, sip_msg* msg)
{
    // - parse R-URI
    // - replace host/port
    // - generate new R-URI
    cstring old_ruri = msg->u.request->ruri_str;
    struct sip_uri parsed_uri;
    if(parse_uri(&parsed_uri, old_ruri.s, old_ruri.len) < 0) {
 	ERROR("could not parse local R-URI ('%.*s')",old_ruri.len,old_ruri.s);
 	return -1;
    }
 	
    // copy from the beginning until URI-host
    n_uri = string(old_ruri.s, parsed_uri.host.s - old_ruri.s);
 
    // append new host and port
    n_uri += get_addr_str(&msg->remote_ip);
    unsigned short new_port = am_get_port(&msg->remote_ip);
    if(new_port != 5060) {
 	n_uri += ":" + int2str(new_port);
    }
 	
    if(parsed_uri.port_str.len) {
 	// copy from end of port-string until the end of old R-URI
 	n_uri += string(parsed_uri.port_str.s + parsed_uri.port_str.len,
 			old_ruri.s + old_ruri.len
 			- (parsed_uri.port_str.s 
 			   + parsed_uri.port_str.len));
    }
    else {
 	// copy from end of host-string until the end of old R-URI
 	n_uri += string(parsed_uri.host.s + parsed_uri.host.len,
 			old_ruri.s + old_ruri.len
 			- (parsed_uri.host.s 
 			   + parsed_uri.host.len));
    }
 
    msg->u.request->ruri_str = stl2cstr(n_uri);
 
    return 0;
}

static int generate_and_parse_new_msg(sip_msg* msg, sip_msg*& p_msg)
{
    int request_len = request_line_len(msg->u.request->method_str,
 				       msg->u.request->ruri_str);
 
    char branch_buf[BRANCH_BUF_LEN];
    compute_branch(branch_buf,msg->callid->value,msg->cseq->value);
    cstring branch(branch_buf,BRANCH_BUF_LEN);
     
    string via(msg->local_socket->get_advertised_ip());
    if(msg->local_socket->get_port() != 5060)
 	via += ":" + int2str(msg->local_socket->get_port());

    cstring trsp(msg->local_socket->get_transport());

    // patch Contact-HF transport parameter
    vector<string> contact_buffers(msg->contacts.size());
    vector<string>::iterator contact_buf_it = contact_buffers.begin();
    list<sip_header*> n_contacts;
    
    //TODO: patch copies of the Contact-HF instead of the original HFs
    for(list<sip_header*>::iterator contact_it = msg->contacts.begin();
	contact_it != msg->contacts.end(); contact_it++, contact_buf_it++) {
	
	n_contacts.push_back(new sip_header(**contact_it));
	patch_contact_transport(n_contacts.back(),trsp,*contact_buf_it);
    }

    // add 'rport' parameter defaultwise? yes, for now
    request_len += via_len(trsp,stl2cstr(via),branch,true);
 
    request_len += copy_hdrs_len(msg->vias);
    request_len += copy_hdrs_len_no_via_contact(msg->hdrs);
    request_len += copy_hdrs_len(n_contacts);
     
    string content_len = int2str(msg->body.len);
     
    request_len += content_length_len(stl2cstr(content_len));
    request_len += 2/* CRLF end-of-headers*/;
     
    if(msg->body.len){
 	request_len += msg->body.len;
    }
     
    // Allocate new message
    p_msg = new sip_msg();
    p_msg->buf = new char[request_len+1];
    p_msg->len = request_len;
 
    // generate it
    char* c = p_msg->buf;
    request_line_wr(&c,msg->u.request->method_str,
 		    msg->u.request->ruri_str);
 
    via_wr(&c,trsp,stl2cstr(via),branch,true);
    copy_hdrs_wr(&c,msg->vias);
    copy_hdrs_wr_no_via_contact(&c,msg->hdrs);

    copy_hdrs_wr(&c,n_contacts);
    free_headers(n_contacts);

    content_length_wr(&c,stl2cstr(content_len));
 
    *c++ = CR;
    *c++ = LF;
 
    if(msg->body.len){
 	memcpy(c,msg->body.s,msg->body.len);
 
 	c += msg->body.len;
    }
    *c++ = '\0';

    // and parse it
    char* err_msg=0;
    if(parse_sip_msg(p_msg,err_msg)){
 	ERROR("Parser failed on generated request\n");
 	ERROR("Message was: <%.*s>\n",p_msg->len,p_msg->buf);
 	delete p_msg;
 	p_msg = NULL;
 	return MALFORMED_SIP_MSG;
    }

    // copy msg->remote_ip
    memcpy(&p_msg->remote_ip,&msg->remote_ip,sizeof(sockaddr_storage));
    p_msg->local_socket = msg->local_socket;
    inc_ref(p_msg->local_socket);
 
    return 0;
}
 
int _trans_layer::send_request(sip_msg* msg, trans_ticket* tt,
			       const cstring& dialog_id,
			       const cstring& _next_hop, 
			       int out_interface, unsigned int flags,
			       msg_logger* logger)
{
    // Request-URI
    // To
    // From
    // Call-ID
    // CSeq
    // Max-Forwards
    // Via
    // Contact
    // Supported / Require
    // Content-Length / Content-Type

    assert(msg);
    assert(tt);

    int res=0;
    list<sip_destination> dest_list;
    if (_next_hop.len) {

	res = parse_next_hop(_next_hop,dest_list);
	if(res || dest_list.empty()) {
	    DBG("parse_next_hop %.*s failed (%i)\n",
		_next_hop.len, _next_hop.s, res);
	    return res;
	}
    }
    else {
	sip_destination dest;
	if(set_next_hop(msg,&dest.host,&dest.port,&dest.trsp) < 0){
	    DBG("set_next_hop failed\n");
	    return -1;
	}
	dest_list.push_back(dest);
    }

    std::unique_ptr<sip_target_set> targets(new sip_target_set());
    res = resolver::instance()->resolve_targets(dest_list,targets.get());
    if(res < 0){
	DBG("resolve_targets failed\n");
	return res;
    }

    targets->debug();
    targets->reset_iterator();

    string uri_buffer; // must have the same scope as 'msg'
    prepare_strict_routing(msg,uri_buffer);

    if(!msg->u.request->ruri_str.len ||
       !msg->u.request->method_str.len) {
	
	ERROR("empty method name or R-URI");
	return -1;
    }
    else {
	DBG("send_request to R-URI <%.*s>",
	    msg->u.request->ruri_str.len,
	    msg->u.request->ruri_str.s);
    }

    int err = 0;
    string ruri; // buffer needs to be @ function scope
    cstring next_trsp;
    sip_msg* p_msg=NULL;

    tt->_bucket = 0;
    tt->_t = 0;

 try_next_dest:
    if(targets->get_next(&msg->remote_ip,next_trsp,flags) < 0) {
	DBG("next_ip(): no more destinations! reply 500");
	sip_msg err;
	set_err_reply_from_req(&err,msg,500,
			       "No destination available");
	ua->handle_sip_reply(c2stlstr(dialog_id),&err);
	return 0;
    }

    if(set_trsp_socket(msg,next_trsp,out_interface) < 0)
	return -1;

    if((flags & TR_FLAG_NEXT_HOP_RURI) &&
       (patch_ruri_with_remote_ip(ruri,msg) < 0)) {
 	return -1;
    }

    // generate new msg and parse it
    err = generate_and_parse_new_msg(msg,p_msg);
    if(err != 0) { return err; }

    DBG("Sending to %s:%i <%.*s...>\n",
	get_addr_str(&p_msg->remote_ip).c_str(),
	ntohs(((sockaddr_in*)&p_msg->remote_ip)->sin_port),
	p_msg->len,p_msg->buf);

    tt->_bucket = get_trans_bucket(p_msg->callid->value,
				   get_cseq(p_msg)->num_str);
    tt->_bucket->lock();
    
    err = p_msg->send(flags);
    if(err < 0){
	ERROR("Error from transport layer\n");

	if(default_bl_ttl) {
	    tr_blacklist::instance()->insert(&p_msg->remote_ip,
					     default_bl_ttl,"503");
	}

	delete p_msg;
	p_msg = NULL;
	tt->_bucket->unlock();
	goto try_next_dest;
    }
    else {
        stats.inc_sent_requests();

	// save parsed method, as update_uac_request
	// might delete p_msg, and msg->u.request->method is not set
	int method = p_msg->u.request->method;

	DBG("update_uac_request tt->_t =%p\n", tt->_t);
	err = update_uac_request(tt->_bucket,tt->_t,p_msg);
	if(err < 0){
	    DBG("Could not update UAC state for request\n");
	    delete p_msg;
	    tt->_bucket->unlock();
	    return err;
	}

	if(tt->_t && (method != sip_request::ACK)) {
	    // save flags & target set in transaction
	    tt->_t->flags = flags;

	    if(tt->_t->targets)	delete tt->_t->targets;
	    tt->_t->targets = targets.release();

	    if(tt->_t->targets->has_next()){
		tt->_t->reset_timer(STIMER_M,M_TIMER,
				    tt->_bucket->get_id());
	    }
	}

	DBG("logger = %p\n",logger);

	if(logger) {
	    sockaddr_storage src_ip;
	    msg->local_socket->copy_addr_to(&src_ip);

	    cstring method_str = msg->u.request->method_str;
	    char* msg_buffer=NULL;
	    unsigned int msg_len=0;

	    if(tt->_t && (method == sip_request::ACK)) {
		// in case of ACK, p_msg gets deleted in update_uac_request
		msg_buffer = tt->_t->retr_buf;
		msg_len = tt->_t->retr_len;
	    }
	    else {
		msg_buffer = p_msg->buf;
		msg_len = p_msg->len;
	    }

	    logger->log(msg_buffer,msg_len,
			&src_ip,&msg->remote_ip,
			method_str);

	    if(tt->_t && !tt->_t->logger) {
		tt->_t->logger = logger;
		inc_ref(logger);
	    }
	}

	if(dialog_id.len && tt->_t && !(tt->_t->dialog_id.len)) {
	    tt->_t->dialog_id.s = new char[dialog_id.len];
	    tt->_t->dialog_id.len = dialog_id.len;
	    memcpy((void*)tt->_t->dialog_id.s,dialog_id.s,dialog_id.len);
	}
    }

    tt->_bucket->unlock();
    
    return err;
}

int _trans_layer::cancel(trans_ticket* tt, const cstring& dialog_id,
			 unsigned int inv_cseq, const cstring& hdrs)
{
    assert(tt);
    assert(tt->_bucket && tt->_t);

    trans_bucket* bucket = tt->_bucket;
    sip_trans*    t = tt->_t;

    bucket->lock();
    if(!bucket->exist(t) || (t->state == TS_ABANDONED)){
	if(dialog_id.len)
	    t = bucket->find_uac_trans(dialog_id,inv_cseq);
	else
	    t = NULL;
    }

    if(!t){
	bucket->unlock();
	DBG("No transaction to cancel: wrong key or finally replied\n");
	return 0;
    }

    if(t->canceled) {
	DBG("Transaction has already been canceled\n");
	bucket->unlock();
	return 0;
    }

    sip_msg* req = t->msg;
    
    // RFC 3261 says: SHOULD NOT be sent for other request
    // than INVITE.
    if(req->u.request->method != sip_request::INVITE){
	t->dump();
	bucket->unlock();
	ERROR("Trying to cancel a non-INVITE request (we SHOULD NOT do that); inv_cseq: %u, i:%.*s\n",
	      inv_cseq, dialog_id.len,dialog_id.s);
	return -1;
    }
    
    switch(t->state){
    case TS_CALLING: {
	// Abandon canceled transaction
	t->clear_timer(STIMER_A);
	t->clear_timer(STIMER_M);
	t->flags |= TR_FLAG_DISABLE_BL;
	t->state = TS_ABANDONED;

	// Answer request internally to terminate the dialog...
	sip_msg reply;
	gen_error_reply_from_req(reply, t->msg, 487, "Request Terminated");
	string dlg_id(t->dialog_id.s, t->dialog_id.len);
	bucket->unlock();
	ua->handle_sip_reply(dlg_id, &reply);
	return 0;
    }

    case TS_COMPLETED:
	ERROR("Trying to cancel a request while in TS_COMPLETED state; inv_cseq: %u, i:%.*s\n",
	      inv_cseq, dialog_id.len,dialog_id.s);
	t->dump();
	bucket->unlock();
	return -1;
	
    case TS_PROCEEDING:
    case TS_ABANDONED:
	// continue with CANCEL request
	break;

    default:
	ERROR("Trying to cancel a request while in %s state; inv_cseq: %u, i:%.*s\n",
	      t->state_str(), inv_cseq, dialog_id.len,dialog_id.s);
	t->dump();
	bucket->unlock();
	return -1;
    }

    cstring cancel_str("CANCEL");
    cstring zero("0");

    int request_len = request_line_len(cancel_str,
				       req->u.request->ruri_str);

    request_len += copy_hdr_len(req->via1);

    request_len += copy_hdr_len(req->to)
	+ copy_hdr_len(req->from)
	+ copy_hdr_len(req->callid)
	+ cseq_len(get_cseq(req)->num_str,cancel_str)
	+ copy_hdrs_len(req->route)
	+ copy_hdrs_len(req->contacts);

    request_len += hdrs.len;
    request_len += content_length_len(zero);
    request_len += 2/* CRLF end-of-headers*/;

    // Allocate new message
    sip_msg* p_msg = new sip_msg();
    p_msg->buf = new char[request_len+1];
    p_msg->len = request_len;

    // generate it
    char* c = p_msg->buf;
    request_line_wr(&c,cancel_str,
		    req->u.request->ruri_str);

    copy_hdr_wr(&c,req->via1);
    copy_hdr_wr(&c,req->to);
    copy_hdr_wr(&c,req->from);
    copy_hdr_wr(&c,req->callid);
    cseq_wr(&c,get_cseq(req)->num_str,cancel_str);
    copy_hdrs_wr(&c,req->route);
    copy_hdrs_wr(&c,req->contacts);

    if (hdrs.len) {
      memcpy(c,hdrs.s,hdrs.len);
      c += hdrs.len;
    }

    content_length_wr(&c,zero);

    *c++ = CR;
    *c++ = LF;
    *c   = '\0';

    // and parse it
    char* err_msg=0;
    if(parse_sip_msg(p_msg,err_msg)){
	ERROR("Parser failed on generated request\n");
	ERROR("Message was: <%.*s>\n",p_msg->len,p_msg->buf);
	delete p_msg;
	bucket->unlock();
	return MALFORMED_SIP_MSG;
    }

    memcpy(&p_msg->remote_ip,&req->remote_ip,sizeof(sockaddr_storage));
    p_msg->local_socket = req->local_socket;
    inc_ref(p_msg->local_socket);

    DBG("Sending to %s:%i:\n<%.*s>\n",
	get_addr_str(&p_msg->remote_ip).c_str(),
	ntohs(((sockaddr_in*)&p_msg->remote_ip)->sin_port),
	p_msg->len,p_msg->buf);

    int send_err = p_msg->send(t->flags);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
	delete p_msg;
    }
    else {
        stats.inc_sent_requests(); // ?

	sip_trans* cancel_t=NULL;
	send_err = update_uac_request(bucket,cancel_t,p_msg);
	if(send_err<0){
	    DBG("Could not update state for UAC transaction\n");
	    delete p_msg;
	}
	else {
            t->canceled = true;

            if(t->logger) {
                sockaddr_storage src_ip;
                p_msg->local_socket->copy_addr_to(&src_ip);
                t->logger->log(p_msg->buf,p_msg->len,&src_ip,
                               &p_msg->remote_ip,cancel_str);

                if(!cancel_t->logger) {
                    cancel_t->logger = t->logger;
                    inc_ref(t->logger);
                }
            }
	}
    }

    bucket->unlock();
    return send_err;
}


#define DROP_MSG \
          delete msg;\
          return

void _trans_layer::received_msg(sip_msg* msg)
{
    char* err_msg=0;
    int err = parse_sip_msg(msg,err_msg);

    if(err){
	DBG("parse_sip_msg returned %i\n",err);

	if(!err_msg){
	    err_msg = (char*)"unknown parsing error";
	}

	DBG("parsing error: %s\n",err_msg);

	DBG("Message was: \"%.*s\"\n",msg->len,msg->buf);

	if((err != MALFORMED_FLINE)
	   && (msg->type == SIP_REQUEST)
	   && (msg->u.request->method != sip_request::ACK)){

	    send_sl_reply(msg,400,cstring(err_msg),
			  cstring(),cstring());
	}

	DROP_MSG;
    }

    process_rcvd_msg(msg);
}

void _trans_layer::process_rcvd_msg(sip_msg* msg)
{    
    assert(msg->callid && get_cseq(msg));
    unsigned int h = hash(msg->callid->value, get_cseq(msg)->num_str);

    trans_bucket* bucket = get_trans_bucket(h);
    sip_trans* t = NULL;

    bucket->lock();

    int err=0;
    switch(msg->type){
    case SIP_REQUEST: 
        stats.inc_received_requests();
	
	if((t = bucket->match_request(msg,TT_UAS)) != NULL){

	    if(t->logger) {
		t->logger->log(msg->buf,msg->len,&msg->remote_ip,
			       &msg->local_ip,msg->u.request->method_str);
	    }

	    if(msg->u.request->method != t->msg->u.request->method){
		
		// ACK matched INVITE transaction
		DBG("ACK matched INVITE transaction %p\n", t);
		int t_state = t->state;
		err = update_uas_request(bucket,t,msg);
		DBG("update_uas_request(bucket,t=%p,msg) = %i\n",t, err);
		if(err<0){
		    DBG("trans_layer::update_uas_trans() failed!\n");
		    // Anyway, there is nothing we can do...
		}
		else {
		
		    // do not touch the transaction anymore:
		    // it could have been deleted !!!
		       
		    // should we forward the ACK to SEMS-App upstream? Yes
		    bucket->unlock();
		    
		    if(t_state == TS_TERMINATED_200) {
			//  let's pass the request to
			//  the UA, iff it was a 200-ACK
			assert(ua);
			DBG("Passing ACK to the UA.\n");
			ua->handle_sip_request(trans_ticket(), // dummy
					       msg);
		    }
		    else {
			DBG("Absorbing non-200-ACK\n");
		    }

		    DROP_MSG;
		}
	    }
	    else {
		DBG("Found retransmission\n");
		t->retransmit(); // retransmit reply
	    }
	}
	else {
             unsigned inv_h;
             trans_bucket* inv_bucket;
             sip_trans* inv_t;
 
             switch (msg->u.request->method) {
                 case sip_request::ACK:
                     // non-2xx ACK??? drop!
                     break;
 
                 case sip_request::PRACK:
                     bucket->unlock();
                     if (! msg->rack) {
                       send_sl_reply(msg, 400, 
                           cstring("Missing valid RSeq header"),
                           cstring(),cstring());
                       DROP_MSG;
                     }
                     /* match INVITE transaction, cool off the 1xx timers */
                     inv_h = hash(msg->callid->value, get_rack(msg)->cseq_str);
                     inv_bucket = get_trans_bucket(inv_h);
                     inv_bucket->lock();
                     if((inv_t = inv_bucket->match_1xx_prack(msg)) != NULL) {
                         assert(msg->u.request->method != 
                             inv_t->msg->u.request->method);
                         err = update_uas_request(inv_bucket,inv_t,msg);
                         DBG("update_uas_request(bucket,t,msg) = %i\n",err);
                     }
                     inv_bucket->unlock();
                     bucket->lock();
                     // no break
 
                 default:
                     // New transaction
                     t = bucket->add_trans(msg, TT_UAS);
 
                     bucket->unlock();
 
                     //  let's pass the request to
                     //  the UA. 
                     assert(ua);
                     ua->handle_sip_request(trans_ticket(t,bucket),msg);
 
                     // forget the msg: it will be
                     // owned by the new transaction
                     return;
             }
	}
	break;
    
    case SIP_REPLY:
        stats.inc_received_replies();

	if((t = bucket->match_reply(msg)) != NULL){

	    // Reply matched UAC transaction
	    
	    DBG("Reply matched an existing transaction\n");

	    if(t->logger && msg->local_socket && msg->buf && msg->len) {
		t->logger->log(msg->buf,msg->len,&msg->remote_ip,
			       &msg->local_ip,get_cseq(msg)->method_str,
			       msg->u.reply->code);
	    }

	    string dialog_id(t->dialog_id.s, t->dialog_id.len);
	    int res = update_uac_reply(bucket,t,msg);
	    if(res < 0){
		ERROR("update_uac_trans() failed, so what happens now???\n");
		break;
	    }
	    // do not touch the transaction anymore:
	    // it could have been deleted !!!
	    if (res) {
		bucket->unlock();
		ua->handle_sip_reply(dialog_id, msg);
		DROP_MSG;
		//return; - part of DROP_MSG
	    }
	}
	else {
	    DBG("Reply did NOT match any existing transaction...\n");
	    DBG("reply code = %i\n",msg->u.reply->code);
	    if( (msg->u.reply->code >= 200) &&
	        (msg->u.reply->code <  300) ) {
		
		bucket->unlock();
		
		// pass to UA
		//assert(ua);
		//ua->handle_sip_reply(msg);
		
		DROP_MSG;
	    }
	}
	break;

    default:
	ERROR("Got unknown message type: Bug?\n");
	break;
    }

    // unlock_drop:
    bucket->unlock();
    DROP_MSG;
}


int _trans_layer::update_uac_reply(trans_bucket* bucket, sip_trans* t, sip_msg* msg)
{
    assert(msg->type == SIP_REPLY);

    cstring to_tag;
    int     reply_code = msg->u.reply->code;

    DBG("update_uac_reply(reply code = %i, trans=%p)\n",reply_code, t);

    if(reply_code < 200){

	// Provisional reply
	switch(t->state){

	case TS_CALLING:
	    t->reset_all_timers();
	    // fall through trap

	case TS_TRYING:
	    t->state = TS_PROCEEDING;
	    // fall through trap

	case TS_PROCEEDING:
	    if(t->msg->u.request->method != sip_request::CANCEL) {
		if(t->msg->u.request->method == sip_request::INVITE) {
		    t->reset_timer(STIMER_C, C_TIMER, bucket->get_id());
		}
		goto pass_reply;
	    }
	    else
		goto end;

	case TS_ABANDONED:
	    // disable blacklisting: remote UA did reply
	    DBG("disable blacklisting: remote UA (%s/%i) did reply",
		am_inet_ntop(&msg->remote_ip).c_str(),
		am_get_port(&msg->remote_ip));
	    t->flags |= TR_FLAG_DISABLE_BL;
	    bucket->unlock();
	    {
		// send CANCEL
		trans_ticket tt(t,bucket);
		cancel(&tt,cstring(),0,cstring());
	    
		// Now remove the transaction
		bucket->lock();
		//bucket->remove(t);
	    }
	    goto end;

	case TS_TERMINATED:
	    // disable blacklisting: remote UA did reply
	    DBG("disable blacklisting: remote UA (%s/%i) did reply",
		am_inet_ntop(&msg->remote_ip).c_str(),
		am_get_port(&msg->remote_ip));
	    t->flags |= TR_FLAG_DISABLE_BL;
	    goto end;

	case TS_COMPLETED:
	default:
	    goto end;
	}
    }
    
    to_tag = ((sip_from_to*)msg->to->p)->tag;
    // if((t->msg->u.request->method == sip_request::INVITE) &&
    //    (reply_code < 300) &&
    //    !to_tag.len){
    // 	//if (!trans_layer::accept_fr_without_totag) {
    // 	ERROR("To-tag missing in final reply to INVITE"
    // 	      //" (see sems.conf: accept_fr_without_totag?)"
    // 	      );
    // 	return -1;
    // 	//}
    // }
    
    if(t->msg->u.request->method == sip_request::INVITE){
    
	if(reply_code >= 300){
	
	    bool forget_reply = false;
	    if(reply_code == 503 &&
	       (t->state == TS_CALLING ||
		t->state == TS_PROCEEDING)) {

		if(!(t->flags & TR_FLAG_DISABLE_BL)) {
		    tr_blacklist::instance()->insert(&t->msg->remote_ip,
						     default_bl_ttl,"503");
		}

		if(msg->local_socket) { // remote reply
		    if(!try_next_ip(bucket,t,true))
			forget_reply = true;
		}
		else { // local reply
		    if(!try_next_ip(bucket,t,false))
			goto end;
		}
	    }
    
	    // Final error reply
	    switch(t->state){
		
	    case TS_CALLING:

		t->reset_all_timers();

	    case TS_PROCEEDING:
	
		t->clear_timer(STIMER_C);

		t->state = TS_COMPLETED;
		send_non_200_ack(msg,t);
		t->reset_timer(STIMER_D, D_TIMER, bucket->get_id());
		
		if(forget_reply)
		    goto end;

		goto pass_reply;
		
	    case TS_ABANDONED:
	    case TS_TERMINATED:
		// local reply: do not send an ACK in this case
		if(!msg->local_socket) {
		    t->reset_all_timers();
		    bucket->remove(t);
		    goto end;
		}

		// disable blacklisting: remote UA did reply
		DBG("disable blacklisting: remote UA (%s/%i) did reply",
		    am_inet_ntop(&msg->remote_ip).c_str(),
		    am_get_port(&msg->remote_ip));

		t->flags |= TR_FLAG_DISABLE_BL;
		// fall through trap

	    case TS_COMPLETED:
		// generate a new non-200 ACK
		send_non_200_ack(msg,t);
	    default:
		goto end;
	    }
	} 
	else {
	    
	    DBG("Positive final reply to INVITE transaction (state=%i)\n",t->state);

	    // Positive final reply to INVITE transaction
	    switch(t->state){
		
	    case TS_CALLING:
	    case TS_PROCEEDING: // first 2xx reply

		// TODO:
		//  we should take care of 200 ACK re-transmissions
		//    - on first reply:
		//      - save to-tag.
		//    - compare to-tag on subsequent replies.
		//      - (if different): 
		//        - (generate new 200 ACK based on reply).
		//        - (send BYE (check for existing UAC trans)).
		//      - else:
		//        - re-transmit ACK.

		t->state  = TS_TERMINATED_200;
		t->reset_all_timers();

		// Timer B & C share the same slot,
		// so it would pretty useless to clear
		// the same timer slot another time ;-)
		//t->clear_timer(STIMER_C);

		t->reset_timer(STIMER_L, L_TIMER, bucket->get_id());

		if (t->to_tag.len==0 && to_tag.len!=0) {
			t->to_tag.s = new char[to_tag.len];
			t->to_tag.len = to_tag.len;
			memcpy((void*)t->to_tag.s,to_tag.s,to_tag.len);
		}
		
		goto pass_reply;
		
	    case TS_TERMINATED_200: // subsequent 2xx reply (no ACK sent)
		
		if( (to_tag.len != t->to_tag.len) ||
		    (memcmp(to_tag.s,t->to_tag.s,to_tag.len) != 0) ){

		    // TODO: 
		    //   (this should be implemented in the UA)
		    //   we should also send a 200 ACK here,
		    //   but this would mean that we should
		    //   also be sending a BYE to quit
		    //   this dialog...
		    //
		    DBG("Received 200 reply with different To-tag as the previous one.\n");
		    goto end;
		}

		DBG("Received 200 reply retransmission\n");
		t->retransmit();
		goto end;

	    case TS_ABANDONED:
	    case TS_TERMINATED:
		//TODO: send ACK+BYE
		DBG("disable blacklisting: remote UA (%s/%i) did reply",
		    am_inet_ntop(&msg->remote_ip).c_str(),
		    am_get_port(&msg->remote_ip));
		t->flags |= TR_FLAG_DISABLE_BL;
		goto end;

	    default:
		goto end;
	    }
	}
    }
    else { // non-INVITE transaction

	if(reply_code == 503) {
	    if(default_bl_ttl) {
		tr_blacklist::instance()->insert(&t->msg->remote_ip,
						 default_bl_ttl,"503");
	    }
	    if(!try_next_ip(bucket,t,false))
		goto end;
	}

	// Final reply
	switch(t->state){
	    
	case TS_TRYING:
	case TS_CALLING:
	case TS_PROCEEDING:
	    
	    t->state = TS_COMPLETED;
	
	    t->clear_timer(STIMER_E);
	    t->clear_timer(STIMER_M);
	    t->clear_timer(STIMER_F);

	    {
		int t_method = t->msg->u.request->method;
		if(msg->local_socket &&
		   !msg->local_socket->is_reliable())
		    t->reset_timer(STIMER_K, K_TIMER, bucket->get_id());
		else {
		    bucket->remove(t);
		}

		// ??? we don't pass CANCEL replies to UA layer ???
		if(t_method != sip_request::CANCEL)
		    goto pass_reply;
		else
		    goto end;
	    }

	case TS_COMPLETED:
	    // Absorb reply retransmission (only if UDP)
	    goto end;
	    
	case TS_ABANDONED:
	case TS_TERMINATED:
	    //local reply
	    if(!msg->local_socket) {
		if(reply_code == 500 || reply_code == 503) {
		    // no more replies will come...
		    bucket->remove(t);
		}
		goto end;
	    }

	    INFO("disable blacklisting: remote UA (%s/%i) did reply",
		 am_inet_ntop(&msg->remote_ip).c_str(),
		 am_get_port(&msg->remote_ip));
	    t->flags |= TR_FLAG_DISABLE_BL;
	    goto end;

	default:
	    goto end;
	}
    }

 pass_reply:
    return 1;
 end:
    return 0;
}

int _trans_layer::update_uac_request(trans_bucket* bucket, sip_trans*& t,
				     sip_msg* msg)
{
    if(msg->u.request->method != sip_request::ACK){
	t = bucket->add_trans(msg,TT_UAC);

	DBG("update_uac_request(t=%p)\n", t);
    }
    else {
	// 200 ACK
	if(!msg->local_socket->is_reliable()) {
	    t = bucket->match_request(msg,TT_UAC);
	    if(t == NULL){
		WARN("While sending 200 ACK: no matching transaction\n");
		return -1;
	    }
	    DBG("update_uac_request(200 ACK, t=%p)\n", t);
	    // clear old retransmission buffer
	    delete [] t->retr_buf;
	
	    // transfer the message buffer 
	    // to the transaction (incl. ownership)
	    t->retr_buf = msg->buf;
	    t->retr_len = msg->len;
	    msg->buf = NULL;
	    msg->len = 0;
	
	    // copy destination address
	    memcpy(&t->retr_addr,&msg->remote_ip,sizeof(sockaddr_storage));
	    inc_ref(msg->local_socket);
	    if(t->retr_socket) dec_ref(t->retr_socket);
	    t->retr_socket = msg->local_socket;

	    // remove the message;
	    delete msg;
	}

	return 0;
    }

    DBG("transport = '%s'; is_reliable = %i",
	msg->local_socket->get_transport(),
	msg->local_socket->is_reliable());

    switch(msg->u.request->method){

    case sip_request::INVITE:
	if(!msg->local_socket->is_reliable()) {
	    // if transport == UDP
	    t->reset_timer(STIMER_A,A_TIMER,bucket->get_id());
	}

	// for any transport type
	t->reset_timer(STIMER_B,B_TIMER,bucket->get_id());
	break;
    
    default:
	if(!msg->local_socket->is_reliable()) {
	    // if transport == UDP
	    t->reset_timer(STIMER_E,E_TIMER,bucket->get_id());
	}

	// for any transport type
	t->reset_timer(STIMER_F,F_TIMER,bucket->get_id());
	break;
    }

    return 0;
}

int _trans_layer::update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code)
{
    DBG("update_uas_reply(t=%p)\n", t);

    t->reply_status = reply_code;
    bool reliable_trsp = t->retr_socket->is_reliable();

    if(t->reply_status >= 300){

	// error reply
	t->state = TS_COMPLETED;
	    
	if(t->msg->u.request->method == sip_request::INVITE){

	    if(!reliable_trsp)
		t->reset_timer(STIMER_G,G_TIMER,bucket->get_id());

	    t->reset_timer(STIMER_H,H_TIMER,bucket->get_id());
	}
	else if(!reliable_trsp) {
	    // 64*T1_TIMER if UDP / 0 if !UDP
	    t->reset_timer(STIMER_J,J_TIMER,bucket->get_id()); 
	}
	else {
	    bucket->remove(t);
	    return TS_REMOVED;
	}
    }
    else if(t->reply_status >= 200) {

	if(t->msg->u.request->method == sip_request::INVITE){

	    // final reply
	    //
	    // In this stack, the transaction layer
	    // takes care of re-transmiting the 200 reply
	    // in a UAS INVITE transaction. The code above
	    // is commented out and shows the behavior as
	    // required by the RFC.
	    //
	    t->state = TS_TERMINATED_200;
	    
	    if(!reliable_trsp)
		t->reset_timer(STIMER_G,G_TIMER,bucket->get_id());

	    t->reset_timer(STIMER_H,H_TIMER,bucket->get_id());
	}
	else if(!reliable_trsp) {
	    // Only for unreliable transports.
	    t->state = TS_COMPLETED;
	    t->reset_timer(STIMER_J,J_TIMER,bucket->get_id()); 
	}
	else {
	    // reliable transports
	    bucket->remove(t);
	    return TS_REMOVED;
	}
    }
    else {
	// provisional reply
        if (t->last_rseq) {
            t->state = TS_PROCEEDING_REL;
            // see above notes, for 2xx replies; the same applies for
            // 1xx/PRACK

	    if(!reliable_trsp)
		t->reset_timer(STIMER_G,G_TIMER,bucket->get_id());

	    t->reset_timer(STIMER_H,H_TIMER,bucket->get_id());
        } else {
	    t->state = TS_PROCEEDING;
        }
    }

    return t->state;
}

int _trans_layer::update_uas_request(trans_bucket* bucket, sip_trans* t, sip_msg* msg)
{
    DBG("update_uas_request(t=%p)\n", t);
    int method = msg->u.request->method;

    if(method != sip_request::ACK &&
       method != sip_request::PRACK) {

	ERROR("Bug? Recvd non PR-/ACK request for existing UAS transact.!?\n");
	return -1;
    }

    switch(t->state){

    case TS_PROCEEDING:
	// ACK or PRACK after non-reliable 1xx???
	return -1;

    case TS_PROCEEDING_REL:
	if(method == sip_request::PRACK) {
	    // stop retransmissions
	    t->clear_timer(STIMER_G);
	    t->clear_timer(STIMER_H);
	}
        return t->state;
	    
    case TS_COMPLETED: // non-2xx-ACK
	if(method != sip_request::ACK) return -1;
	t->state = TS_CONFIRMED;

	t->clear_timer(STIMER_G);
	t->clear_timer(STIMER_H);

	if(msg->local_socket->is_reliable()){
	    bucket->remove(t);
	    return TS_REMOVED;
	}

	t->reset_timer(STIMER_I,I_TIMER,bucket->get_id());
	
	// drop through
    case TS_CONFIRMED: // non-2xx-ACK re-transmission
	return t->state;
	    
    case TS_TERMINATED_200: // 2xx-ACK
	if(method != sip_request::ACK) return -1;
	// remove transaction
	bucket->remove(t);
	return TS_REMOVED;
	    
    default:
	DBG("Bug? Unknown state at this point: %i\n",t->state);
	break;
    }

    return -1;
}

void _trans_layer::send_non_200_ack(sip_msg* reply, sip_trans* t)
{
    sip_msg* inv = t->msg;
    
    cstring method("ACK",3);
    int ack_len = request_line_len(method,inv->u.request->ruri_str);
    
    ack_len += copy_hdr_len(inv->via1)
	+ copy_hdr_len(inv->from)
	+ copy_hdr_len(reply->to)
	+ copy_hdr_len(inv->callid);
    
    ack_len += cseq_len(get_cseq(inv)->num_str,method);

    if(!inv->route.empty())
 	ack_len += copy_hdrs_len(inv->route);

    cstring content_len("0");
    ack_len += content_length_len(content_len);

    ack_len += 2/* EoH CRLF */;
    
    char* ack_buf = new char [ack_len];
    char* c = ack_buf;

    request_line_wr(&c,method,inv->u.request->ruri_str);
    
    copy_hdr_wr(&c,inv->via1);

    copy_hdr_wr(&c,inv->from);
    copy_hdr_wr(&c,reply->to);
    copy_hdr_wr(&c,inv->callid);
    
    cseq_wr(&c,get_cseq(inv)->num_str,method);

    if(!inv->route.empty())
	 copy_hdrs_wr(&c,inv->route);

    content_length_wr(&c,content_len);
    
    *c++ = CR;
    *c++ = LF;

    DBG("About to send ACK\n");

    assert(inv->local_socket);
    int send_err = inv->local_socket->send(&inv->remote_ip,ack_buf,
					   ack_len,t->flags);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
    }
    else stats.inc_sent_requests();
    
    if(t->logger) {
	sockaddr_storage src_ip;
	inv->local_socket->copy_addr_to(&src_ip);
	t->logger->log(ack_buf,ack_len,&src_ip,&inv->remote_ip,method);
    }

    delete[] ack_buf;

}

void _trans_layer::timer_expired(trans_timer* t, trans_bucket* bucket,
				 sip_trans* tr)
{
    int n = t->type >> 16;
    int type = t->type & 0xFFFF;

    switch(type){

    case STIMER_A:  // Calling: (re-)send INV

	n++;
	tr->msg->send(tr->flags);
        stats.inc_sent_request_retrans();
	
	if(tr->logger) {
	    sockaddr_storage src_ip;
	    tr->msg->local_socket->copy_addr_to(&src_ip);
	    tr->logger->log(tr->msg->buf,tr->msg->len,&src_ip,&tr->msg->remote_ip,
			    tr->msg->u.request->method_str);
	}

	tr->reset_timer((n<<16) | type, A_TIMER<<n, bucket->get_id());
	break;
	
    case STIMER_B:  // Calling: -> Terminated

	tr->clear_timer(STIMER_B);
	if(tr->state == TS_CALLING) {
	    DBG("Transaction timeout!\n");
	    // unlocks the bucket
	    timeout(bucket,tr);
	    return;
	}
	else if(tr->state == TS_ABANDONED) {
	    if(tr->flags & TR_FLAG_DISABLE_BL) {
		bucket->remove(tr);
	    }
	    else {
		// set blacklist timer
		tr->reset_timer(STIMER_BL,BL_TIMER,bucket->get_id());
	    }
	}
	else {
	    DBG("Transaction timeout timer hit while state=%s (0x%x)",
		tr->state_str(), tr->state);
	    bucket->remove(tr);
	}
	break;

    case STIMER_C: // Proceeding -> Terminated
	
	// Note: remember well, we first set timer C
	//       after the first provisional reply.
	tr->clear_timer(STIMER_C);
	//if(tr->state != TS_PROCEEDING)
	//  break; // shouldn't happen
	bucket->unlock();

	{
	    // send CANCEL
	    trans_ticket tt(tr,bucket);
	    cancel(&tt,cstring(),0,cstring());
	    
	    // Now remove the transaction
	    bucket->lock();
	    if(bucket->exist(tr)) {
		// unlocks the bucket
		timeout(bucket,tr);
		return;
	    }
	}
	break;

    case STIMER_F:  // Trying/Proceeding: terminate transaction
	
	tr->clear_timer(STIMER_F);

	switch(tr->state) {

	case TS_TRYING:
	case TS_PROCEEDING:
	    DBG("Transaction timeout!\n");
	    // unlocks the bucket
	    timeout(bucket,tr);
	    return;
	case TS_ABANDONED:
	    if(tr->flags & TR_FLAG_DISABLE_BL) {
		bucket->remove(tr);
	    }
	    else {
		// set blacklist timer
		tr->reset_timer(STIMER_BL,BL_TIMER,bucket->get_id());
	    }
	    break;
	default:
	    DBG("Transaction timeout timer hit while state=%s (0x%x)",
		tr->state_str(), tr->state);
	    bucket->remove(tr);
	}
	break;

    case STIMER_H:  // PENDING_REL, Completed: -> Terminated
        if(tr->type == TT_UAS) { // TODO: can timer _H fire for TT_UAC??
          bool handled;

          switch(tr->state) {
          case TS_PROCEEDING_REL: // missing PRACK for rel-1xx
            assert(tr->retr_len);
            tr->clear_timer(type); // stop retransmissions
            //signal timeout to UA
            ua->handle_reply_timeout(AmSipTimeoutEvent::noPRACK, tr, bucket);
                //tr->msg, tr->retr_buf, tr->retr_len); //signal timeout to UA
            handled = true; // let the UA [3..6]xx, if it wishes so
            break;

          case TS_TERMINATED_200: // missing ACK for (locally replied) 200
          case TS_COMPLETED: // missing ACK for (locally replied) [3..6]xx
            tr->clear_timer(type);
            ////ua->timer_expired(tr,STIMER_H);
            ua->handle_reply_timeout(AmSipTimeoutEvent::noACK, tr);
            bucket->remove(tr);
            handled = true;
            break;

          default: 
            handled = false;
          }

          if (handled)
            break;
        }

    case STIMER_D:  // Completed: -> Terminated
    case STIMER_K:  // Completed: terminate transaction
    case STIMER_J:  // Completed: -> Terminated
    case STIMER_I:  // Confirmed: -> Terminated
    case STIMER_L:  // Terminated_200 -> Terminated
	
	// TODO:
	//  - check if the UA has sent the ACK.
	//    else, send ACK & BYE.

	tr->clear_timer(type);
	bucket->remove(tr);
	break;

    case STIMER_E:  // Trying/Proceeding: (re-)send request
    case STIMER_G:  { // Completed: (re-)send response

	n++; // re-transmission counter

	//
	// in this stack, the transaction layer
	// takes care of re-transmiting the 200 reply
	// in a UAS INVITE transaction.
	//
	if(tr->type == TT_UAS){
	    
	    // re-transmit reply to INV
	    tr->retransmit();
            stats.inc_sent_reply_retrans();
	}
	else {

	    // re-transmit request
	    tr->msg->send(tr->flags);
            stats.inc_sent_request_retrans();

	    if(tr->logger) {
		sockaddr_storage src_ip;
		tr->msg->local_socket->copy_addr_to(&src_ip);
		tr->logger->log(tr->msg->buf,tr->msg->len,
				&src_ip,&tr->msg->remote_ip,
				tr->msg->u.request->method_str);
	    }
	}

	unsigned int retr_timer = (type == STIMER_E) ?
	    E_TIMER << n : G_TIMER << n;

	if(retr_timer<<n > T2_TIMER) {
	    tr->reset_timer((n<<16) | type, T2_TIMER, bucket->get_id());
	}
	else {
	    tr->reset_timer((n<<16) | type, retr_timer, bucket->get_id());
	}
    } break;

    case STIMER_M: {
	if(!try_next_ip(bucket,tr,true)) {
	    // Abandon old transaction
	    tr->clear_timer(STIMER_A);
	    tr->state = TS_ABANDONED;
	}
    } break;

    case STIMER_BL:
	tr->clear_timer(STIMER_BL);
	if(!(tr->flags & TR_FLAG_DISABLE_BL)) {
	    // insert destination to blacklist
	    if(default_bl_ttl) {
		tr_blacklist::instance()->insert(&tr->msg->remote_ip,
						 default_bl_ttl,
						 "timeout");
	    }
	}
	bucket->remove(tr);
	break;

    default:
	ERROR("Invalid timer type %i\n",type);
	break;
    }

    bucket->unlock();
}

/**
 * Tries to find an interface suitable for
 * sending to the destination supplied.
 */
int _trans_layer::find_outbound_if(sockaddr_storage* remote_ip)
{
    if(transports.size() == 0)
	return 0;

    if(transports.size() == 1)
	return 0;
    
    int temp_sock = socket(remote_ip->ss_family, SOCK_DGRAM, 0 );
    if (temp_sock == -1) {
	ERROR( "ERROR: socket() failed: %s\n",
	       strerror(errno));
	return 0;
    }
    
    sockaddr_storage from;
    socklen_t    len=sizeof(from);
    
    if (connect(temp_sock, (sockaddr*)remote_ip, 
		remote_ip->ss_family == AF_INET ? 
		sizeof(sockaddr_in) : sizeof(sockaddr_in6))==-1) {
	
	ERROR("connect failed: %s\n",
	      strerror(errno));
	goto error;
    }
    
    if (getsockname(temp_sock, (sockaddr*)&from, &len)==-1) {
	ERROR("getsockname failed: %s\n",
	      strerror(errno));
	goto error;
    }
    close(temp_sock);
    
    // try with alternative address
    char local_ip[NI_MAXHOST];
    if(am_inet_ntop(&from,local_ip,NI_MAXHOST) != NULL) {
	map<string,unsigned short>::iterator if_it =
	    AmConfig::LocalSIPIP2If.find(local_ip);
	if(if_it == AmConfig::LocalSIPIP2If.end()){
	    ERROR("Could not find a local interface for "
		  "resolved local IP (local_ip='%s')",
		  local_ip);
	}
	else {
	    return if_it->second;
	}
    }

    // no matching interface
    return -1;

 error:
    close(temp_sock);
    return 0;
}

sip_trans* _trans_layer::copy_uac_trans(sip_trans* tr)
{
    assert(tr && (tr->type == TT_UAC));
    sip_trans* n_tr = new sip_trans();
    
    n_tr->type  = tr->type;
    n_tr->flags = tr->flags;

    if(tr->dialog_id.len) {
	n_tr->dialog_id.s = new char[tr->dialog_id.len];
	n_tr->dialog_id.len = tr->dialog_id.len;
	memcpy((void*)n_tr->dialog_id.s,tr->dialog_id.s,n_tr->dialog_id.len);
    }

    if(tr->logger) {
	n_tr->logger = tr->logger;
	inc_ref(n_tr->logger);
    }

    return n_tr;
}

int _trans_layer::try_next_ip(trans_bucket* bucket, sip_trans* tr,
			      bool use_new_trans)
{
    tr->clear_timer(STIMER_M);

    cstring next_trsp;
    sockaddr_storage sa;

 try_next_dest:
    // get the next ip
    if(!tr->targets ||
       tr->targets->get_next(&sa,next_trsp,tr->flags) < 0){
	DBG("no more destinations!");
	return -1;
    }

    if(use_new_trans) {
	string   n_uri;
	cstring  old_uri;
	unique_ptr<sip_trans> n_tr(copy_uac_trans(tr));

	// Warning: no deep copy!!!
	//  -> do not forget to release() before it's too late!
	sip_msg tmp_msg(*tr->msg);

	// remove last Via-HF
	tmp_msg.vias.pop_front();

	// copy the new address back
	memcpy(&tmp_msg.remote_ip,&sa,sizeof(sockaddr_storage));

	// backup R-URI before possible update
	old_uri = tr->msg->u.request->ruri_str;

	int out_interface = tmp_msg.local_socket->get_if();
	tmp_msg.local_socket = NULL;
	if(set_trsp_socket(&tmp_msg,next_trsp,out_interface) < 0)
	    return -1;

	if(n_tr->flags & TR_FLAG_NEXT_HOP_RURI) {
	    // patch R-URI, generate& parse new message
	    if(patch_ruri_with_remote_ip(n_uri, &tmp_msg)) {
		ERROR("could not patch R-URI with new destination");
		tmp_msg.release();
		return -1;
	    }
	}

	sip_msg* p_msg=NULL;
	if(generate_and_parse_new_msg(&tmp_msg,p_msg)) {
	    ERROR("could not generate&parse new message");
	    tmp_msg.release();
	    return -1;
	}

	tmp_msg.release();
	n_tr->msg = p_msg;

	// take over target set
	n_tr->targets = tr->targets;
	tr->targets = NULL;

	// restore old R-URI
	tr->msg->u.request->ruri_str = old_uri;

	trans_timer* t_bf = tr->get_timer(STIMER_B);
	tr = n_tr.release();

	// keep old timer B/F
	if(t_bf) {
	    t_bf = new trans_timer(*t_bf,bucket->get_id(),tr);
	    tr->reset_timer(t_bf,t_bf->type);
	}

	bucket->append(tr);	
    }
    else {
	// copy the new address back
	memcpy(&tr->msg->remote_ip,&sa,sizeof(sockaddr_storage));

	trsp_socket* old_sock = tr->msg->local_socket;
	int out_interface = old_sock->get_if();
	if(set_trsp_socket(tr->msg,next_trsp,out_interface) < 0)
	    return -1;

	if(tr->flags & TR_FLAG_NEXT_HOP_RURI) {
	    string   n_uri;
	    sip_msg* p_msg=NULL;

	    // patch R-URI, generate& parse new message
	    if(patch_ruri_with_remote_ip(n_uri, tr->msg) ||
	       generate_and_parse_new_msg(tr->msg,p_msg)) {
		ERROR("could not patch R-URI with new destination");
		return -1;
	    }

	    delete tr->msg;
	    tr->msg = p_msg;
	}
	else if(old_sock != tr->msg->local_socket) {
	    string   n_uri;
	    sip_msg* p_msg=NULL;

	    // patch R-URI, generate & parse new message
	    if(generate_and_parse_new_msg(tr->msg,p_msg)) {
		return -1;
	    }

	    delete tr->msg;
	    tr->msg = p_msg;
	}
	else {
	    // only create new branch tag
	    // -> patched directly in the msg's buffer
	    compute_branch((char*)(tr->msg->via_p1->branch.s+MAGIC_BRANCH_LEN),
			   tr->msg->callid->value,tr->msg->cseq->value);
	}
    }
   

    // and re-send
    if(tr->msg->send(tr->flags) < 0) {
	ERROR("Error from transport layer\n");

	if(default_bl_ttl) {
	    tr_blacklist::instance()->insert(&tr->msg->remote_ip,
					     default_bl_ttl,"503");
	}

	use_new_trans = false;
	goto try_next_dest;
    }

    stats.inc_sent_requests();
    
    if(tr->logger) {
	sockaddr_storage src_ip;
	tr->msg->local_socket->copy_addr_to(&src_ip);
	tr->logger->log(tr->msg->buf,tr->msg->len,
			&src_ip,&tr->msg->remote_ip,
			tr->msg->u.request->method_str);
    }

    if(tr->msg->u.request->method == sip_request::INVITE) {
	tr->state = TS_CALLING;
	if(!tr->msg->local_socket->is_reliable()) {
	    tr->reset_timer(STIMER_A,A_TIMER,bucket->get_id());
	}
	if(!tr->get_timer(STIMER_B)) {
	    tr->reset_timer(STIMER_B,B_TIMER,bucket->get_id());
	}
    }
    else {
	tr->state = TS_TRYING;
	if(!tr->msg->local_socket->is_reliable()) {
	    tr->reset_timer(STIMER_E,E_TIMER,bucket->get_id());
	}
	if(!tr->get_timer(STIMER_F)) {
	    tr->reset_timer(STIMER_F,F_TIMER,bucket->get_id());
	}
    }
    
    if(tr->targets->has_next())
	tr->reset_timer(STIMER_M,M_TIMER,bucket->get_id());

    return 0;
}

void trans_ticket::lock_bucket() const
{
    _bucket->lock();
}

void trans_ticket::unlock_bucket() const
{
    _bucket->unlock();
}

const sip_trans* trans_ticket::get_trans() const
{
    if(_bucket->exist(_t))
	return _t; 
    else 
	return NULL; 
}

void trans_ticket::remove_trans()
{
    _bucket->remove(_t);
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
