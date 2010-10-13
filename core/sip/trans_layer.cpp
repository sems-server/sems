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
#include "parse_100rel.h"
#include "sip_trans.h"
#include "msg_fline.h"
#include "msg_hdrs.h"
#include "udp_trsp.h"
#include "resolver.h"
#include "log.h"

#include "wheeltimer.h"
#include "sip_timers.h"

#include "SipCtrlInterface.h"
#include "AmUtils.h"
#include "AmSipMsg.h"
#include "AmConfig.h"
#include "AmSipEvent.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

#include <algorithm>

bool _trans_layer::accept_fr_without_totag = false;

_trans_layer::_trans_layer()
    : ua(NULL),
      transport(NULL)
{
}

_trans_layer::~_trans_layer()
{}


void _trans_layer::register_ua(sip_ua* ua)
{
    this->ua = ua;
}

void _trans_layer::register_transport(trsp_socket* trsp)
{
    transport = trsp;
}



int _trans_layer::send_reply(trans_ticket* tt,
			    int reply_code, const cstring& reason,
			    const cstring& to_tag, const cstring& hdrs, 
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

    sip_msg* req = t->msg;
    assert(req);

    bool have_to_tag = false;
    int  reply_len   = status_line_len(reason);

    // add 'received' should be added
    // check if first Via has rport parameter

    assert(req->via1);
    assert(req->via_p1);

    unsigned int new_via1_len = copy_hdr_len(req->via1);
    string remote_ip_str = get_addr_str(((sockaddr_in*)&req->remote_ip)->sin_addr).c_str();
    new_via1_len += 10/*;received=*/ + remote_ip_str.length();

    // needed if rport parameter was present but empty
    string remote_port_str;
    if(req->via_p1->has_rport) {
	if(!req->via_p1->rport.len){
	    remote_port_str = int2str(ntohs(((sockaddr_in*)&req->remote_ip)->sin_port));
	    new_via1_len += remote_port_str.length() + 1/* "=<port number>" */;
	}
    }

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
	    assert((*it)->p);
	    if(! ((sip_from_to*)(*it)->p)->tag.len ) {

		reply_len += 5/* ';tag=' */
		    + to_tag.len; 
	    }
	    else {
		// To-tag present in request
		have_to_tag = true;

		t->to_tag = ((sip_from_to*)(*it)->p)->tag;
	    }
	    reply_len += copy_hdr_len(*it);
	    break;

	case sip_header::H_FROM:
	case sip_header::H_CALL_ID:
	case sip_header::H_CSEQ:
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

    DBG("reply_len = %i\n",reply_len);

    status_line_wr(&c,reply_code,reason);

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

		memcpy(c,";received=",10);
		c += 10;

		memcpy(c,remote_ip_str.c_str(),remote_ip_str.length());
		c += remote_ip_str.length();

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

    assert(transport);
    // TODO: inspect topmost 'Via' and select proper addr (+resolve DNS names)
    // refs: RFC3261 18.2.2; RFC3581
    sockaddr_storage remote_ip;
    memcpy(&remote_ip,&req->remote_ip,sizeof(sockaddr_storage));

    if(req->via_p1->has_rport){

	if(req->via_p1->rport_i){
	    // use 'rport'
	    ((sockaddr_in*)&remote_ip)->sin_port = htons(req->via_p1->rport_i);
	}
	// else: use the source port from the replied request (from IP hdr)
    }
    else {
	
	if(req->via_p1->port_i){
	    // use port from 'sent-by' via address
	    ((sockaddr_in*)&remote_ip)->sin_port = htons(req->via_p1->port_i);
	}
	// else: use the source port from the replied request (from IP hdr)
    }

    int err = transport->send(&remote_ip,reply_buf,reply_len);
    if(err < 0){
	delete [] reply_buf;
	goto end;
    }

    err = update_uas_reply(bucket,t,reply_code);
    if(err < 0){
	
	ERROR("Invalid state change\n");
	delete [] reply_buf;
    }
    else if(err != TS_TERMINATED) {
	if (t->retr_buf) 
		delete [] t->retr_buf;

	t->retr_buf = reply_buf;
	t->retr_len = reply_len;
	memcpy(&t->retr_addr,&remote_ip,sizeof(sockaddr_storage));

	err = 0;
    }
    else {
	// Transaction has been deleted
	// -> should not happen, as we 
	//    now wait for 200 ACK
	delete [] reply_buf;
	err = 0;
    }
    
 end:
    bucket->unlock();
    return err;
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

    assert(transport);

    int err = transport->send(&req->remote_ip,reply_buf,reply_len);
    delete [] reply_buf;

    return err;
}

//
// Ref. RFC 3261 "12.2.1.1 Generating the Request"
//
int _trans_layer::set_next_hop(sip_msg* msg, 
			       cstring* next_hop,
			       unsigned short* next_port)
{
    assert(msg);

    list<sip_header*>& route_hdrs = msg->route; 
    cstring& r_uri = msg->u.request->ruri_str;

    int err=0;

    if(!route_hdrs.empty()){
	
	sip_header* fr = route_hdrs.front();
	
	sip_nameaddr na;
	const char* c = fr->value.s;
	if(parse_nameaddr(&na, &c, fr->value.len)<0) {
	    
	    DBG("Parsing name-addr failed\n");
	    return -1;
	}
	
	if(parse_uri(&na.uri,na.addr.s,na.addr.len) < 0) {
	    
	    DBG("Parsing route uri failed\n");
	    return -1;
	}

	bool is_lr = false;
	if(!na.uri.params.empty()){
	    
	    list<sip_avp*>::iterator it = na.uri.params.begin();
	    for(;it != na.uri.params.end(); it++){
		
		if( ((*it)->name.len == 2) && 
		    (!memcmp((*it)->name.s,"lr",2)) ) {

		    is_lr = true;
		    break;
		}
	    }

	}

	if (next_hop->len == 0) {
	    *next_hop  = na.uri.host;
	    if(na.uri.port_str.len)
		*next_port = na.uri.port;
	}	    

	if(!is_lr){
	    
	    // detect beginning of next route

	    enum {
		RR_PARAMS=0,
		RR_QUOTED,
		RR_SEP_SWS,  // space(s) after ','
		RR_NXT_ROUTE
	    };

	    int st = RR_PARAMS;
	    const char* end = fr->value.s + fr->value.len;
	    for(;c<end;c++){
		
		switch(st){
		case RR_PARAMS:
		    switch(*c){
		    case SP:
		    case HTAB:
		    case CR:
		    case LF:
			break;
		    case COMMA:
			st = RR_SEP_SWS;
			break;
		    case DQUOTE:
			st = RR_QUOTED;
			break;
		    }
		    break;
		case RR_QUOTED:
		    switch(*c){
		    case BACKSLASH:
			c++;
			break;
		    case DQUOTE:
			st = RR_PARAMS;
			break;
		    }
		    break;
		case RR_SEP_SWS:
		    switch(*c){
		    case SP:
		    case HTAB:
		    case CR:
		    case LF:
			break;
		    default:
			st = RR_NXT_ROUTE;
			goto nxt_route;
		    }
		    break;
		}
	    }

	nxt_route:
	    
	    switch(st){
	    case RR_QUOTED:
	    case RR_SEP_SWS:
		DBG("Malformed first route header\n");
	    case RR_PARAMS:
		// remove current route header from message
		route_hdrs.pop_front();
		DBG("route_hdrs.length() = %i\n",(int)route_hdrs.size());
		{
		    list<sip_header*>::iterator h_it = std::find(msg->hdrs.begin(),msg->hdrs.end(),fr);
		    if(h_it != msg->hdrs.end()) msg->hdrs.erase(h_it);
		}
		DBG("delete (fr=0x%p)\n",fr);
		delete fr;
		break;
		
	    case RR_NXT_ROUTE:
		// remove current route from this header
		fr->value.s   = c;
		fr->value.len = end-c;
		break;
	    }

	    
	    // copy r_uri at the end of 
	    // the route set.
	    msg->hdrs.push_back(new sip_header(0,"Route",r_uri));

	    r_uri = na.addr;
	}
	
    }
    else {

	sip_uri parsed_r_uri;
	err = parse_uri(&parsed_r_uri,r_uri.s,r_uri.len);
	if(err < 0){
	    ERROR("Invalid Request URI\n");
	    return -1;
	}
	*next_hop  = parsed_r_uri.host;
	if(parsed_r_uri.port_str.len)
	    *next_port = parsed_r_uri.port;
    }

    DBG("next_hop:next_port is <%.*s:%u>\n", next_hop->len, next_hop->s, *next_port);
    
    return 0;
}


int _trans_layer::set_destination_ip(sip_msg* msg, cstring* next_hop, unsigned short next_port)
{
    string nh = c2stlstr(*next_hop);
    
    if(!next_port){
	// no explicit port specified,
	// try SRV first

	string srv_name = "_sip._udp." + nh;
	if(!resolver::instance()->resolve_name(srv_name.c_str(),
					       &(msg->h_dns),
					       &(msg->remote_ip),IPv4)){
	    return 0;
	}

	DBG("no SRV record for %s",srv_name.c_str());
    }

    memset(&(msg->remote_ip),0,sizeof(sockaddr_storage));
    int err = resolver::instance()->resolve_name(nh.c_str(),
						 &(msg->h_dns),
						 &(msg->remote_ip),IPv4);
    if(err < 0){
	ERROR("Unresolvable Request URI domain\n");
	return -1;
    }

    if(!((sockaddr_in*)&(msg->remote_ip))->sin_port) {
	if(!next_port)
	    next_port = 5060;
	((sockaddr_in*)&(msg->remote_ip))->sin_port = htons(next_port);
    }
 
    return 0;
}

void _trans_layer::timeout(trans_bucket* bucket, sip_trans* t)
{
    t->reset_all_timers();
    t->state = TS_TERMINATED;

    // send 408 to 'ua'
    sip_msg  msg;
    sip_msg* req = t->msg;

    msg.type = SIP_REPLY;
    msg.u.reply = new sip_reply();

    msg.u.reply->code = 408;
    msg.u.reply->reason = cstring("Timeout");

    msg.from = req->from;
    msg.to = req->to;
    msg.cseq = req->cseq;
    msg.callid = req->callid;

    ua->handle_sip_reply(&msg);

    bucket->remove(t);
}

int _trans_layer::send_request(sip_msg* msg, trans_ticket* tt)
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

    assert(transport);
    assert(msg);
    assert(tt);

    cstring next_hop;
    unsigned short next_port=0;

    tt->_bucket = 0;
    tt->_t = 0;

    if(!msg->u.request->ruri_str.len ||
       !msg->u.request->method_str.len) {
	
	ERROR("empty method name or R-URI");
	return -1;
    }
    else {
	DBG("send_request to <%.*s>",msg->u.request->ruri_str.len,msg->u.request->ruri_str.s);
    }

    int request_len = request_line_len(msg->u.request->method_str,
				       msg->u.request->ruri_str);

    char branch_buf[BRANCH_BUF_LEN];
    compute_branch(branch_buf,msg->callid->value,msg->cseq->value);
    cstring branch(branch_buf,BRANCH_BUF_LEN);
    
    string via(transport->get_ip());
    if(transport->get_port() != 5060)
	via += ":" + int2str(transport->get_port());

    // add 'rport' parameter defaultwise? yes, for now
    request_len += via_len(stl2cstr(via),branch,true);

    request_len += copy_hdrs_len(msg->hdrs);

    string content_len = int2str(msg->body.len);

    request_len += content_length_len(stl2cstr(content_len));
    request_len += 2/* CRLF end-of-headers*/;

    if(msg->body.len){
	request_len += msg->body.len;
    }

    // Allocate new message
    sip_msg* p_msg = new sip_msg();
    p_msg->buf = new char[request_len+1];
    p_msg->len = request_len;

    // generate it
    char* c = p_msg->buf;
    request_line_wr(&c,msg->u.request->method_str,
		    msg->u.request->ruri_str);

    via_wr(&c,stl2cstr(via),branch,true);
    copy_hdrs_wr(&c,msg->hdrs);

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
	return MALFORMED_SIP_MSG;
    }

    if(set_next_hop(msg,&next_hop,&next_port) < 0){
	DBG("set_next_hop failed\n");
	delete p_msg;
	return -1;
    }

    if(set_destination_ip(p_msg,&next_hop,next_port) < 0){
     	DBG("set_destination_ip failed\n");
	delete p_msg;
     	return -1;
    }

    DBG("Sending to %s:%i <%.*s...>\n",
	get_addr_str(((sockaddr_in*)&p_msg->remote_ip)->sin_addr).c_str(),
	ntohs(((sockaddr_in*)&p_msg->remote_ip)->sin_port),
	50 /* preview - instead of p_msg->len */,p_msg->buf);

    tt->_bucket = get_trans_bucket(p_msg->callid->value,
					    get_cseq(p_msg)->num_str);
    tt->_bucket->lock();
    
    int send_err = transport->send(&p_msg->remote_ip,p_msg->buf,p_msg->len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
	delete p_msg;
    }
    else {

	send_err = update_uac_request(tt->_bucket,tt->_t,p_msg);
	if(send_err < 0){
	    ERROR("Could not update UAC state for request.\n");
	    delete p_msg;
	}
    }

    tt->_bucket->unlock();
    
    return send_err;
}

int _trans_layer::cancel(trans_ticket* tt)
{
    assert(tt);
    assert(tt->_bucket && tt->_t);

    trans_bucket* bucket = tt->_bucket;
    sip_trans*    t = tt->_t;

    bucket->lock();
    if(!bucket->exist(t)){
	DBG("No transaction to cancel: wrong key or finally replied\n");
	bucket->unlock();
	return 0;
    }

    sip_msg* req = t->msg;
    
    // RFC 3261 says: SHOULD NOT be sent for other request
    // than INVITE.
    if(req->u.request->method != sip_request::INVITE){
	bucket->unlock();
	ERROR("Trying to cancel a non-INVITE request (we SHOULD NOT do that)\n");
	return -1;
    }
    
    switch(t->state){
    case TS_CALLING:
	bucket->unlock();
	ERROR("Trying to cancel a request while in TS_CALLING state.\n");
	return -1;

    case TS_COMPLETED:
	bucket->unlock();
	ERROR("Trying to cancel a request while in TS_COMPLETED state\n");
	return -1;
	
    case TS_PROCEEDING:
	// continue with CANCEL request
	break;

    default:
	assert(0);
	break;
    }

    cstring cancel_str("CANCEL");

    int request_len = request_line_len(cancel_str,
				       req->u.request->ruri_str);

    char branch_buf[BRANCH_BUF_LEN];
    compute_branch(branch_buf,req->callid->value,get_cseq(req)->num_str);
    cstring branch(branch_buf,BRANCH_BUF_LEN);
    
    string via(transport->get_ip());
    if(transport->get_port() != 5060)
	via += ":" + int2str(transport->get_port());

    //TODO: add 'rport' parameter by default?

    request_len += copy_hdr_len(req->via1);

    request_len += copy_hdr_len(req->to)
	+ copy_hdr_len(req->from)
	+ copy_hdr_len(req->callid)
	+ cseq_len(get_cseq(req)->num_str,cancel_str)
	+ copy_hdrs_len(req->route)
	+ copy_hdrs_len(req->contacts);

    request_len += 2/* CRLF end-of-headers*/;

    // Allocate new message
    sip_msg* p_msg = new sip_msg();
    p_msg->buf = new char[request_len];
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

    *c++ = CR;
    *c++ = LF;

    // and parse it
    char* err_msg=0;
    if(parse_sip_msg(p_msg,err_msg)){
	ERROR("Parser failed on generated request\n");
	ERROR("Message was: <%.*s>\n",p_msg->len,p_msg->buf);
	delete p_msg;
	return MALFORMED_SIP_MSG;
    }

    memcpy(&p_msg->remote_ip,&req->remote_ip,sizeof(sockaddr_storage));

    DBG("Sending to %s:%i:\n<%.*s>\n",
	get_addr_str(((sockaddr_in*)&p_msg->remote_ip)->sin_addr).c_str(),
	ntohs(((sockaddr_in*)&p_msg->remote_ip)->sin_port),
	p_msg->len,p_msg->buf);

    trans_bucket* n_bucket = get_trans_bucket(p_msg->callid->value,
					      get_cseq(p_msg)->num_str);
    
    if(bucket != n_bucket)
	n_bucket->lock();

    int send_err = transport->send(&p_msg->remote_ip,p_msg->buf,p_msg->len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
	delete p_msg;
    }
    else {

	sip_trans* t=NULL;
	send_err = update_uac_request(bucket,t,p_msg);
	if(send_err<0){
	    ERROR("Could not update state for UAC transaction\n");
	    delete p_msg;
	}
    }

    if(bucket != n_bucket)
	n_bucket->unlock();
    
    bucket->unlock();
    return send_err;
}


void _trans_layer::received_msg(sip_msg* msg)
{
#define DROP_MSG \
          delete msg;\
          return

    char* err_msg=0;
    int err = parse_sip_msg(msg,err_msg);

    if(err){
	DBG("parse_sip_msg returned %i\n",err);

	if(!err_msg){

	    err_msg = (char*)"unknown parsing error";
	    DBG("parsing error: %s\n",err_msg);
	}

	DBG("Message was: \"%.*s\"\n",msg->len,msg->buf);

	if((err != MALFORMED_FLINE)
	   && (msg->type == SIP_REQUEST)
	   && (msg->u.request->method != sip_request::ACK)){

	    send_sl_reply(msg,400,cstring(err_msg),
			  cstring(),cstring());
	}

	DROP_MSG;
    }
    
    assert(msg->callid && get_cseq(msg));
#if 0
    unsigned int  h;
    if (msg->rack) { /* it's a PRACK */
        h = hash(msg->callid->value, get_rack(msg)->cseq_str);
        DBG("### RACK: <%.*s> <%.*s>\n", 
            msg->callid->value.len, msg->callid->value.s,
            get_rack(msg)->cseq_str.len, get_rack(msg)->cseq_str.s);
    } else {
        h = hash(msg->callid->value, get_cseq(msg)->num_str);
        DBG("### XXXX: <%.*s> <%.*s>\n", 
            msg->callid->value.len, msg->callid->value.s,
            get_cseq(msg)->num_str.len, get_cseq(msg)->num_str.s);
    }
#else
    unsigned int h = hash(msg->callid->value, get_cseq(msg)->num_str);
#endif
    trans_bucket* bucket = get_trans_bucket(h);
    sip_trans* t = NULL;

    bucket->lock();

    switch(msg->type){
    case SIP_REQUEST: 
	
	if((t = bucket->match_request(msg)) != NULL){
	    if(msg->u.request->method != t->msg->u.request->method){
		
		// ACK matched INVITE transaction
		DBG("ACK matched INVITE transaction\n");
		
		err = update_uas_request(bucket,t,msg);
		DBG("update_uas_request(bucket,t,msg) = %i\n",err);
		if(err<0){
		    DBG("trans_layer::update_uas_trans() failed!\n");
		    // Anyway, there is nothing we can do...
		}
		else if((err == TS_TERMINATED) ||
			(err == TS_REMOVED)){
		
		    // do not touch the transaction anymore:
		    // it could have been deleted !!!
		       
		    // should we forward the ACK to SEMS-App upstream? Yes
		    bucket->unlock();
		    
		    //  let's pass the request to
		    //  the UA. 
		    assert(ua);
		    DBG("Passing ACK to the UA.\n");
		    ua->handle_sip_request(trans_ticket(t,bucket),msg);
		    
		    DROP_MSG;
		}
	    }
	    else {
		DBG("Found retransmission\n");
		retransmit(t); // retransmit reply
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
                     /* match INVITE transaction, cool off the 1xx timers */
                     inv_h = hash(msg->callid->value, get_rack(msg)->cseq_str);
                     inv_bucket = get_trans_bucket(inv_h);
                     inv_bucket->lock();
                     if((inv_t = inv_bucket->match_request(msg)) != NULL) {
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

	if((t = bucket->match_reply(msg)) != NULL){

	    // Reply matched UAC transaction
	    
	    DBG("Reply matched an existing transaction\n");
	    if(update_uac_reply(bucket,t,msg) < 0){
		ERROR("update_uac_trans() failed, so what happens now???\n");
		break;
	    }
	    // do not touch the transaction anymore:
	    // it could have been deleted !!!
	}
	else {
	    DBG("Reply did NOT match any existing transaction...\n");
	    DBG("reply code = %i\n",msg->u.reply->code);
	    if( (msg->u.reply->code >= 200) &&
	        (msg->u.reply->code <  300) ) {
		
		bucket->unlock();
		
		// pass to UA
		assert(ua);
		ua->handle_sip_reply(msg);
		
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

    DBG("reply code = %i\n",msg->u.reply->code);

    if(reply_code < 200){

	// Provisional reply
	switch(t->state){

	case TS_CALLING:
	    t->clear_timer(STIMER_A);
	    t->clear_timer(STIMER_M);
	    t->clear_timer(STIMER_B);
	    // fall through trap

	case TS_TRYING:
	    t->state = TS_PROCEEDING;
	    // fall through trap

	case TS_PROCEEDING:
	    goto pass_reply;

	case TS_COMPLETED:
	default:
	    goto end;
	}
    }
    
    to_tag = ((sip_from_to*)msg->to->p)->tag;
    if((t->msg->u.request->method != sip_request::CANCEL) && !to_tag.len){
	DBG("To-tag missing in final reply (see sipctrl.conf?)\n");
	if (!trans_layer::accept_fr_without_totag)
	    return -1;
    }
    
    if(t->msg->u.request->method == sip_request::INVITE){
    
	if(reply_code >= 300){
	    
	    // Final error reply
	    switch(t->state){
		
	    case TS_CALLING:

		t->clear_timer(STIMER_A);
		t->clear_timer(STIMER_M);
		t->clear_timer(STIMER_B);

	    case TS_PROCEEDING:
		
		t->state = TS_COMPLETED;
		send_non_200_ack(msg,t);
		t->reset_timer(STIMER_D, D_TIMER, bucket->get_id());
		
		goto pass_reply;
		
	    case TS_COMPLETED:
		// retransmit non-200 ACK
		retransmit(t);
	    default:
		goto end;
	    }
	} 
	else {
	    
	    DBG("Positive final reply to INVITE transaction (state=%i)\n",t->state);

	    // Positive final reply to INVITE transaction
	    switch(t->state){
		
	    case TS_CALLING:
	    case TS_PROCEEDING:

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
		t->clear_timer(STIMER_A);
		t->clear_timer(STIMER_M);
		t->clear_timer(STIMER_B);

		t->reset_timer(STIMER_L, L_TIMER, bucket->get_id());

		if (t->to_tag.len==0) {
			t->to_tag.s = new char[to_tag.len];
			t->to_tag.len = to_tag.len;
			memcpy((void*)t->to_tag.s,to_tag.s,to_tag.len);
		}
		
		goto pass_reply;
		
	    case TS_TERMINATED_200:
		
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
		retransmit(t);
		goto end;

	    default:
		goto end;
	    }
	}
    }
    else { // non-INVITE transaction

	// Final reply
	switch(t->state){
	    
	case TS_TRYING:
	case TS_CALLING:
	case TS_PROCEEDING:
	    
	    t->state = TS_COMPLETED;
	
	    t->clear_timer(STIMER_E);
	    t->clear_timer(STIMER_M);
	    t->clear_timer(STIMER_F);
	    t->reset_timer(STIMER_K, K_TIMER, bucket->get_id());
	    
	    if(t->msg->u.request->method != sip_request::CANCEL)
		goto pass_reply;
	    else
		goto end;

	case TS_COMPLETED:
	    // Absorb reply retransmission (only if UDP)
	    goto end;
	    
	default:
	    goto end;
	}
    }

 pass_reply:
    assert(ua);
    ua->handle_sip_reply(msg);
 end:
    return 0;
}

int _trans_layer::update_uac_request(trans_bucket* bucket, sip_trans*& t, sip_msg* msg)
{
    if(msg->u.request->method != sip_request::ACK){
	t = bucket->add_trans(msg,TT_UAC);
    }
    else {
	// 200 ACK
	t = bucket->match_request(msg);
	if(t == NULL){
	    DBG("While sending 200 ACK: no matching transaction\n");
	    return -1;
	}

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

	// remove the message;
	delete msg;

	return 0;
    }

    switch(msg->u.request->method){

    case sip_request::INVITE:
	// if transport == UDP
	t->reset_timer(STIMER_A,A_TIMER,bucket->get_id());
	// for any transport type
	t->reset_timer(STIMER_B,B_TIMER,bucket->get_id());
	break;
    
    default:
	// if transport == UDP
	t->reset_timer(STIMER_E,E_TIMER,bucket->get_id());
	// for any transport type
	t->reset_timer(STIMER_F,F_TIMER,bucket->get_id());
	break;
    }

    if(!msg->h_dns.eoip()){ // if transport == UDP
	t->reset_timer(STIMER_M,M_TIMER,bucket->get_id());
    }

    return 0;
}

int _trans_layer::update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code)
{
    if(t->reply_status >= 200){
	ERROR("Transaction has already been closed with a final reply\n");
	return -1;
    }

    t->reply_status = reply_code;

    if(t->reply_status >= 300){

	// error reply
	t->state = TS_COMPLETED;
	    
	if(t->msg->u.request->method == sip_request::INVITE){
	    t->reset_timer(STIMER_G,G_TIMER,bucket->get_id());
	    t->reset_timer(STIMER_H,H_TIMER,bucket->get_id());
	}
	else {
	    // 64*T1_TIMER if UDP / 0 if !UDP
	    t->reset_timer(STIMER_J,J_TIMER,bucket->get_id()); 
	}
    }
    else if(t->reply_status >= 200) {

	if(t->msg->u.request->method == sip_request::INVITE){

	    // final reply

	    //bucket->remove_trans(t);
	    //return TS_TERMINATED;

	    //
	    // In this stack, the transaction layer
	    // takes care of re-transmiting the 200 reply
	    // in a UAS INVITE transaction. The code above
	    // is commented out and shows the behavior as
	    // required by the RFC.
	    //
	    t->state = TS_TERMINATED_200;
	    t->reset_timer(STIMER_G,G_TIMER,bucket->get_id());
	    t->reset_timer(STIMER_H,H_TIMER,bucket->get_id());

	}
	else {
	    t->state = TS_COMPLETED;
	    // Only for unreliable transports.
	    t->reset_timer(STIMER_J,J_TIMER,bucket->get_id()); 
	}
    }
    else {
	// provisional reply
        if (t->last_rseq) {
            t->state = TS_PROCEEDING_REL;
            // see above notes, for 2xx replies; the same applies for
            // 1xx/PRACK
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
    if(msg->u.request->method != sip_request::ACK &&
            msg->u.request->method != sip_request::PRACK){
	ERROR("Bug? Recvd non PR-/ACK request for existing UAS transact.!?\n");
	return -1;
    }
	
    switch(t->state){

    case TS_PROCEEDING_REL:
        // stop retransmissions
	t->clear_timer(STIMER_G);
	t->clear_timer(STIMER_H);
        return t->state;
	    
    case TS_COMPLETED:
	t->state = TS_CONFIRMED;

	t->clear_timer(STIMER_G);
	t->clear_timer(STIMER_H);

	t->reset_timer(STIMER_I,I_TIMER,bucket->get_id());
	
	// drop through
    case TS_CONFIRMED:
	return t->state;
	    
    case TS_TERMINATED_200:
	// remove transaction
	bucket->remove(t);
	return TS_REMOVED;
	    
    default:
	DBG("Bug? Unknown state at this point: %i\n",t->state);
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
    ack_len += 2/* EoH CRLF */;

    if(!inv->route.empty())
 	ack_len += copy_hdrs_len(inv->route);
    
    char* ack_buf = new char [ack_len];
    char* c = ack_buf;

    request_line_wr(&c,method,inv->u.request->ruri_str);
    
    copy_hdr_wr(&c,inv->via1);

    if(!inv->route.empty())
	 copy_hdrs_wr(&c,inv->route);

    copy_hdr_wr(&c,inv->from);
    copy_hdr_wr(&c,reply->to);
    copy_hdr_wr(&c,inv->callid);
    
    cseq_wr(&c,get_cseq(inv)->num_str,method);
    
    *c++ = CR;
    *c++ = LF;

    DBG("About to send ACK\n");

    assert(transport);
    int send_err = transport->send(&inv->remote_ip,ack_buf,ack_len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
	delete ack_buf;
    }
    else {
	delete [] t->retr_buf;
	t->retr_buf = ack_buf;
	t->retr_len = ack_len;
	memcpy(&t->retr_addr,&inv->remote_ip,sizeof(sockaddr_storage));
    }
}

void _trans_layer::retransmit(sip_trans* t)
{
    assert(transport);
    if(!t->retr_buf || !t->retr_len){
	// there is nothing to re-transmit yet!!!
	return;
    }

    int send_err = transport->send(&t->retr_addr,t->retr_buf,t->retr_len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
    }
}

void _trans_layer::retransmit(sip_msg* msg)
{
    assert(transport);
    int send_err = transport->send(&msg->remote_ip,msg->buf,msg->len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
    }
}

void _trans_layer::timer_expired(timer* t, trans_bucket* bucket, sip_trans* tr)
{
    int n = t->type >> 16;
    int type = t->type & 0xFFFF;

    switch(type){

    case STIMER_A:  // Calling: (re-)send INV

	n++;
	retransmit(tr->msg);
	tr->reset_timer((n<<16) | type, T1_TIMER<<n, bucket->get_id());
	break;
	
    case STIMER_B:  // Calling: -> Terminated

	tr->clear_timer(STIMER_B);
	if(tr->state == TS_CALLING) {
	    DBG("Transaction timeout!\n");
	    timeout(bucket,tr);
	}
	else {
	    DBG("Transaction timeout timer hit while state=0x%x",tr->state);
	}
	break;

    case STIMER_F:  // Trying/Proceeding: terminate transaction
	
	tr->clear_timer(STIMER_F);

	switch(tr->state) {

	case TS_TRYING:
	case TS_PROCEEDING:
	    DBG("Transaction timeout!\n");
	    timeout(bucket,tr);
	    break;
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
    case STIMER_G:  // Completed: (re-)send response

	n++; // re-transmission counter

	//
	// in this stack, the transaction layer
	// takes care of re-transmiting the 200 reply
	// in a UAS INVITE transaction.
	//
	if(tr->type == TT_UAS){
	    
	    // re-transmit reply to INV
	    retransmit(tr);
	}
	else {

	    // re-transmit request
	    retransmit(tr->msg);
	}

	if(T1_TIMER<<n > T2_TIMER) {
	    tr->reset_timer((n<<16) | type, T2_TIMER, bucket->get_id());
	}
	else {
	    tr->reset_timer((n<<16) | type, T1_TIMER<<n, bucket->get_id());
	}
	break;

    case STIMER_M:
	{
	    sockaddr_storage sa;
	    memset(&sa,0,sizeof(sockaddr_storage));

	    // get the next ip
	    if(tr->msg->h_dns.next_ip(&sa) < 0){
		tr->clear_timer(STIMER_M);
		return;
	    }

	    //If a SRV record is involved, the port number
	    // should have been set by h_dns.next_ip(...).
	    if(!((sockaddr_in*)&sa)->sin_port){
		//Else, we copy the old port number
		((sockaddr_in*)&sa)->sin_port = ((sockaddr_in*)&tr->msg->remote_ip)->sin_port;
	    }

	    // copy the new address back
	    memcpy(&tr->msg->remote_ip,&sa,sizeof(sockaddr_storage));

	    // create new branch tag
	    compute_branch((char*)(tr->msg->via_p1->branch.s+MAGIC_BRANCH_LEN),
			   tr->msg->callid->value,tr->msg->cseq->value);

	    // and re-send
	    retransmit(tr->msg);

	    // reset counter for timer A & E
	    timer* A_E_timer = tr->get_timer(STIMER_A);
	    tr->reset_timer(A_E_timer->type & 0xFFFF,A_TIMER,bucket->get_id());

	    if(!tr->msg->h_dns.eoip())
		tr->reset_timer(STIMER_M,M_TIMER,bucket->get_id());
	    else
		tr->clear_timer(STIMER_M);
	}
	break;

    default:
	ERROR("Invalid timer type %i\n",t->type);
	break;
    }
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
