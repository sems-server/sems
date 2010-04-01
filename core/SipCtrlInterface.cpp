/*
 * $Id: SipCtrlInterface.cpp 1648 2010-03-03 19:35:22Z sayer $
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
#include "SipCtrlInterface.h"

#include "AmUtils.h"
#include "AmSipMsg.h"

#include "sip/trans_layer.h"
#include "sip/sip_parser.h"
#include "sip/parse_header.h"
#include "sip/parse_from_to.h"
#include "sip/parse_cseq.h"
#include "sip/hash_table.h"
#include "sip/sip_trans.h"
#include "sip/wheeltimer.h"
#include "sip/msg_hdrs.h"
#include "sip/udp_trsp.h"

#include "log.h"

#include <assert.h>

#include "AmApi.h"
#include "AmConfigReader.h"
#include "AmSipDispatcher.h"

#ifndef MOD_NAME
#define MOD_NAME  "sipctrl"
#endif

string SipCtrlInterface::outbound_host = "";
unsigned int SipCtrlInterface::outbound_port = 0;
bool SipCtrlInterface::accept_fr_without_totag = false;
int SipCtrlInterface::log_raw_messages = 3;
bool SipCtrlInterface::log_parsed_messages = true;

int SipCtrlInterface::load()
{
    INFO("SIP bind_addr: `%s'.\n", AmConfig::LocalSIPIP.c_str());
    INFO("SIP bind_port: `%i'.\n", AmConfig::LocalSIPPort);

    if (!AmConfig::OutboundProxy.empty()) {
	sip_uri parsed_uri;
	if (parse_uri(&parsed_uri, (char *)AmConfig::OutboundProxy.c_str(),
		      AmConfig::OutboundProxy.length()) < 0) {
	    ERROR("invalid outbound_proxy specified\n");
	    return -1;
	}
	outbound_host = c2stlstr(parsed_uri.host);
	if (parsed_uri.port) {
	    outbound_port = parsed_uri.port;
	}
    }

    AmConfigReader cfg;
    string cfgfile = AmConfig::ConfigurationFile.c_str();
    if (file_exists(cfgfile) && !cfg.loadFile(cfgfile)) {
	if (cfg.hasParameter("accept_fr_without_totag")) {
	    accept_fr_without_totag = 
		cfg.getParameter("accept_fr_without_totag") == "yes";
	}
	DBG("accept_fr_without_totag = %s\n", 
	    accept_fr_without_totag?"yes":"no");

	if (cfg.hasParameter("log_raw_messages")) {
	    string msglog = cfg.getParameter("log_raw_messages");
	    if (msglog == "no") log_raw_messages = -1;
	    else if (msglog == "error") log_raw_messages = 0;
	    else if (msglog == "warn")  log_raw_messages = 1;
	    else if (msglog == "info")  log_raw_messages = 2;
	    else if (msglog == "debug") log_raw_messages = 3;
	}
	DBG("log_raw_messages level = %d\n", 
	    log_raw_messages);

	if (cfg.hasParameter("log_parsed_messages")) {
	    log_parsed_messages = cfg.getParameter("log_parsed_messages")=="yes";
	}
	DBG("log_parsed_messages = %s\n", 
	    log_parsed_messages?"yes":"no");

    } else {
	DBG("assuming SIP default settings.\n");
    }

    return 0;
    
}

SipCtrlInterface::SipCtrlInterface()
{
    trans_layer::instance()->register_ua(this);
}

int SipCtrlInterface::cancel(const AmSipRequest& req)
{
    unsigned int  h=0;
    unsigned long t=0;

    if((sscanf(req.serKey.c_str(),"%x:%lx",&h,&t) != 2) ||
       (h >= H_TABLE_ENTRIES)){
	ERROR("Invalid transaction key: invalid bucket ID (key=%s)\n",req.serKey.c_str());
	return -1;
    }

    return trans_layer::instance()->cancel(get_trans_bucket(h),(sip_trans*)t);
}


int SipCtrlInterface::send(const AmSipRequest &req, char* serKey, unsigned int& serKeyLen)
{
    serKeyLen = 0;

    if(req.method == "CANCEL")
	return cancel(req);

    sip_msg* msg = new sip_msg();
    
    msg->type = SIP_REQUEST;
    msg->u.request = new sip_request();

    msg->u.request->method_str = stl2cstr(req.method);
    msg->u.request->ruri_str = stl2cstr(req.r_uri);

    // To
    // From
    // Call-ID
    // CSeq
    // Contact
    // Max-Forwards

    
    char* c = (char*)req.from.c_str();
    int err = parse_headers(msg,&c);

    c = (char*)req.to.c_str();
    err = err || parse_headers(msg,&c);

    if(err){
	ERROR("Malformed To or From header\n");
	delete msg;
	return -1;
    }

    string cseq = int2str(req.cseq)
	+ " " + req.method;

    msg->cseq = new sip_header(0,"CSeq",stl2cstr(cseq));
    msg->hdrs.push_back(msg->cseq);

    msg->callid = new sip_header(0,"Call-ID",stl2cstr(req.callid));
    msg->hdrs.push_back(msg->callid);


    if(!req.contact.empty()){

	c = (char*)req.contact.c_str();
	err = parse_headers(msg,&c);
	if(err){
	    ERROR("Malformed Contact header\n");
	    delete msg;
	    return -1;
	}
    }
    
    if(!req.route.empty()){
	
 	char *c = (char*)req.route.c_str();
	
 	err = parse_headers(msg,&c);
	
	if(err){
	    ERROR("Route headers parsing failed\n");
	    ERROR("Faulty headers were: <%s>\n",req.route.c_str());
	    delete msg;
	    return -1;
	}

	//
	// parse_headers() appends our route headers 
	// to msg->hdrs and msg->route. But we want
	// only msg->route(), so we just remove the
	// route headers at the end of msg->hdrs.
	//
 	for(sip_header* h_rr = msg->hdrs.back();
 	    !msg->hdrs.empty(); h_rr = msg->hdrs.back()) {
	    
 	    if(h_rr->type != sip_header::H_ROUTE){
 		break;
 	    }
	    
	    msg->hdrs.pop_back();
 	}
    }

    if(!req.hdrs.empty()) {
	
 	char *c = (char*)req.hdrs.c_str();
	
 	err = parse_headers(msg,&c);
	
	if(err){
	    ERROR("Additional headers parsing failed\n");
	    ERROR("Faulty headers were: <%s>\n",req.hdrs.c_str());
	    delete msg;
	    return -1;
	}
    }

    if(!req.body.empty()){

	if(req.content_type.empty()){
	    ERROR("Body in request without content type\n");
	}
	else {
	    msg->content_type = new sip_header(0,"Content-Type",stl2cstr(req.content_type));
	    msg->hdrs.push_back(msg->content_type);

	    msg->body = stl2cstr(req.body);
	}
    }

    string next_hop;
    unsigned int next_port_i = 0;

    if(!req.next_hop.empty()){
	string next_port;
	sip_uri parsed_uri;
	if (parse_uri(&parsed_uri, (char *)req.next_hop.c_str(),
		      req.next_hop.length()) < 0) {
	    ERROR("invalid next hop URI '%s'\n", req.next_hop.c_str());
	    ERROR("Using default outbound proxy");
	    next_hop = SipCtrlInterface::outbound_host;
	    next_port_i = SipCtrlInterface::outbound_port;
	} else {
	    next_hop = c2stlstr(parsed_uri.host);
	    if (parsed_uri.port) {
		next_port_i= parsed_uri.port;	    }
	    next_hop += *(c++);
	}
    }
    else if(!SipCtrlInterface::outbound_host.empty()){
	next_hop = SipCtrlInterface::outbound_host;
	next_port_i = SipCtrlInterface::outbound_port;
    }

    cstring c_next_hop = stl2cstr(next_hop);
    if(trans_layer::instance()->set_next_hop(msg->route,msg->u.request->ruri_str,
			c_next_hop,(unsigned short)next_port_i,
			&msg->remote_ip) < 0){

	DBG("set_next_hop failed\n");
	delete msg;
	return -1;
    }

    int res = trans_layer::instance()->send_request(msg,serKey,serKeyLen);
    delete msg;

    return res;
}

void SipCtrlInterface::run(const string& bind_addr, unsigned short bind_port)
{
    INFO("Starting SIP control interface\n");

    udp_trsp* udp_server =  new udp_trsp(trans_layer::instance());

    trans_layer::instance()->register_transport(udp_server);
    udp_server->bind(bind_addr,bind_port);
    
    wheeltimer::instance()->start();

    udp_server->start();
    udp_server->join();
}

int SipCtrlInterface::send(const AmSipReply &rep)
{
    unsigned int  h=0;
    unsigned long t=0;

    if((sscanf(rep.serKey.c_str(),"%x:%lx",&h,&t) != 2) ||
       (h >= H_TABLE_ENTRIES)){
	ERROR("Invalid transaction key: invalid bucket ID\n");
	return -1;
    }
    
    sip_msg msg;

    if(!rep.hdrs.empty()) {

	char* c = (char*)rep.hdrs.c_str();
	int err = parse_headers(&msg,&c);
	if(err){
	    ERROR("Malformed additional header\n");
	    return -1;
	}
    }

    if(!rep.contact.empty()){

	char* c = (char*)rep.contact.c_str();
	int err = parse_headers(&msg,&c);
	if(err){
	    ERROR("Malformed Contact header\n");
	    return -1;
	}
    }

    if(!rep.body.empty()) {
	if(rep.content_type.empty()){
	    ERROR("Reply does not contain a Content-Type whereby body is not empty\n");
	    return -1;
	}
    }

    
    unsigned int hdrs_len = copy_hdrs_len(msg.hdrs);

    if(!rep.body.empty()) {
	hdrs_len += content_type_len(stl2cstr(rep.content_type));
    }

    char* hdrs_buf = NULL;
    char* c = hdrs_buf;

    if (hdrs_len) {
	
	c = hdrs_buf = new char[hdrs_len];
	
	copy_hdrs_wr(&c,msg.hdrs);
		
	if(!rep.body.empty()) {
	    content_type_wr(&c,stl2cstr(rep.content_type));
	}
    }

    int ret = trans_layer::instance()->send_reply(get_trans_bucket(h),(sip_trans*)t,
			     rep.code,stl2cstr(rep.reason),
			     stl2cstr(rep.local_tag),
			     cstring(hdrs_buf,hdrs_len), stl2cstr(rep.body));

    delete [] hdrs_buf;

    return ret;
}

#define DBG_PARAM(p)\
    DBG("%s = <%s>\n",#p,p.c_str());

void SipCtrlInterface::handleSipMsg(AmSipRequest &req)
{
    DBG("Received new request\n");
    if (SipCtrlInterface::log_parsed_messages) {
	//     DBG_PARAM(req.cmd);
	DBG_PARAM(req.method);
	//     DBG_PARAM(req.user);
	//     DBG_PARAM(req.domain);
	//     DBG_PARAM(req.dstip);
	//     DBG_PARAM(req.port);
	DBG_PARAM(req.r_uri);
	DBG_PARAM(req.from_uri);
	DBG_PARAM(req.from);
	DBG_PARAM(req.to);
	DBG_PARAM(req.callid);
	DBG_PARAM(req.from_tag);
	DBG_PARAM(req.to_tag);
	DBG("cseq = <%i>\n",req.cseq);
	DBG_PARAM(req.serKey);
	DBG_PARAM(req.route);
	DBG_PARAM(req.next_hop);
	DBG("hdrs = <%s>\n",req.hdrs.c_str());
	DBG("body = <%s>\n",req.body.c_str());
    }

    AmSipDispatcher::instance()->handleSipMsg(req);
}

void SipCtrlInterface::handleSipMsg(AmSipReply &rep)
{
    DBG("Received reply: %i %s\n",rep.code,rep.reason.c_str());
    DBG_PARAM(rep.callid);
    DBG_PARAM(rep.local_tag);
    DBG_PARAM(rep.remote_tag);
    DBG("cseq = <%i>\n",rep.cseq);

    AmSipDispatcher::instance()->handleSipMsg(rep);
}

void SipCtrlInterface::handle_sip_request(const char* tid, sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipRequest req;
    
    req.cmd      = "sems";
    req.method   = c2stlstr(msg->u.request->method_str);
    req.user     = c2stlstr(msg->u.request->ruri.user);
    req.domain   = c2stlstr(msg->u.request->ruri.host);
    req.dstip    = get_addr_str(((sockaddr_in*)(&msg->local_ip))->sin_addr); //FIXME: IPv6
    req.port     = int2str(ntohs(((sockaddr_in*)(&msg->local_ip))->sin_port));
    req.r_uri    = c2stlstr(msg->u.request->ruri_str);

    if(get_contact(msg)){

	sip_nameaddr na;
	const char* c = get_contact(msg)->value.s;
	if(parse_nameaddr(&na,&c,get_contact(msg)->value.len) < 0){
	    WARN("Contact parsing failed\n");
	    WARN("\tcontact = '%.*s'\n",get_contact(msg)->value.len,get_contact(msg)->value.s);
	    WARN("\trequest = '%.*s'\n",msg->len,msg->buf);
	}
	else {
	    sip_uri u;
	    if(parse_uri(&u,na.addr.s,na.addr.len)){
		WARN("'Contact' in new request contains a malformed URI\n");
		WARN("\tcontact uri = '%.*s'\n",na.addr.len,na.addr.s);
		WARN("\trequest = '%.*s'\n",msg->len,msg->buf);
	    }

	    req.from_uri = c2stlstr(na.addr);
	    req.contact  = c2stlstr(get_contact(msg)->value);
	}
    }
    else {
	if (req.method == "INVITE") {
	    WARN("Request has no contact header\n");
	    WARN("\trequest = '%.*s'\n",msg->len,msg->buf);
	}
    }
    
    if(req.from_uri.empty()) {
	req.from_uri = c2stlstr(get_from(msg)->nameaddr.addr);
    }

    if(get_from(msg)->nameaddr.name.len){
	req.from += c2stlstr(get_from(msg)->nameaddr.name) + ' ';
    }

    req.from += '<' + c2stlstr(get_from(msg)->nameaddr.addr) + '>';

    req.to       = c2stlstr(msg->to->value);
    req.callid   = c2stlstr(msg->callid->value);
    req.from_tag = c2stlstr(((sip_from_to*)msg->from->p)->tag);
    req.to_tag   = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    req.cseq     = get_cseq(msg)->num;
    req.body     = c2stlstr(msg->body);

    if(tid != NULL)
	req.serKey = tid;

    if (msg->content_type)
 	req.content_type = c2stlstr(msg->content_type->value);

    prepare_routes_uas(msg->record_route, req.route);
	
    for (list<sip_header*>::iterator it = msg->hdrs.begin(); 
	 it != msg->hdrs.end(); ++it) {
	if((*it)->type == sip_header::H_OTHER){
	    req.hdrs += c2stlstr((*it)->name) + ": " 
		+ c2stlstr((*it)->value) + "\r\n";
	}
    }

    handleSipMsg(req);
}

void SipCtrlInterface::handle_sip_reply(sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipReply   reply;

    reply.content_type = msg->content_type ? c2stlstr(msg->content_type->value): "";

    reply.body = msg->body.len ? c2stlstr(msg->body) : "";
    reply.cseq = get_cseq(msg)->num;
    reply.method = c2stlstr(get_cseq(msg)->method_str);

    reply.code   = msg->u.reply->code;
    reply.reason = c2stlstr(msg->u.reply->reason);

    if(get_contact(msg)){

	// parse the first contact
	const char* c = get_contact(msg)->value.s;
	sip_nameaddr na;
	
	int err = parse_nameaddr(&na,&c,get_contact(msg)->value.len);
	if(err < 0) {
	    
	    ERROR("Contact nameaddr parsing failed\n");
	    return;
	}
	
	// 'Contact' header?
	reply.next_request_uri = c2stlstr(na.addr);
	
	list<sip_header*>::iterator c_it = msg->contacts.begin();
	reply.contact = c2stlstr((*c_it)->value);
	for(;c_it!=msg->contacts.end(); ++c_it){
	    reply.contact += "," + c2stlstr((*c_it)->value);
	}
    }

    reply.callid = c2stlstr(msg->callid->value);
    
    reply.remote_tag = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    reply.local_tag  = c2stlstr(((sip_from_to*)msg->from->p)->tag);

    reply.dstip = get_addr_str(((sockaddr_in*)(&msg->local_ip))->sin_addr); //FIXME: IPv6
    reply.port  = int2str(ntohs(((sockaddr_in*)(&msg->local_ip))->sin_port));

    prepare_routes_uac(msg->record_route, reply.route);

    for (list<sip_header*>::iterator it = msg->hdrs.begin(); 
	 it != msg->hdrs.end(); ++it) {
	if((*it)->type == sip_header::H_OTHER){
	    reply.hdrs += c2stlstr((*it)->name) + ": " 
		+ c2stlstr((*it)->value) + "\r\n";
	}
    }
    
    handleSipMsg(reply);
}

void SipCtrlInterface::prepare_routes_uac(const list<sip_header*>& routes, string& route_field)
{
    if(!routes.empty()){
	
	list<sip_header*>::const_reverse_iterator it = routes.rbegin();

	route_field = c2stlstr((*it)->value);
	++it;

	for(; it != routes.rend(); ++it) {
		
	    route_field += ", " + c2stlstr((*it)->value);
	}
    }
}

void SipCtrlInterface::prepare_routes_uas(const list<sip_header*>& routes, string& route_field)
{
    if(!routes.empty()){
	
	list<sip_header*>::const_iterator it = routes.begin();

	route_field = c2stlstr((*it)->value);
	++it;

	for(; it != routes.end(); ++it) {
		
	    route_field += ", " + c2stlstr((*it)->value);
	}
    }
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
