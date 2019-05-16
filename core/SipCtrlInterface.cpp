/*
 * $Id: SipCtrlInterface.cpp 1648 2010-03-03 19:35:22Z sayer $
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
#include "SipCtrlInterface.h"

#include "AmUtils.h"
#include "AmSipMsg.h"
#include "AmMimeBody.h"
#include "AmSipHeaders.h"

#include "sip/trans_layer.h"
#include "sip/sip_parser.h"
#include "sip/parse_header.h"
#include "sip/parse_from_to.h"
#include "sip/parse_cseq.h"
#include "sip/parse_100rel.h"
#include "sip/parse_route.h"
#include "sip/trans_table.h"
#include "sip/sip_trans.h"
#include "sip/wheeltimer.h"
#include "sip/msg_hdrs.h"
#include "sip/udp_trsp.h"
#include "sip/ip_util.h"
#include "sip/tcp_trsp.h"

#include "log.h"

#include <assert.h>

#include "AmApi.h"
#include "AmConfigReader.h"
#include "AmSipDispatcher.h"
#include "AmEventDispatcher.h"
#include "AmSipEvent.h"

bool _SipCtrlInterface::log_parsed_messages = true;
int _SipCtrlInterface::udp_rcvbuf = -1;

int _SipCtrlInterface::alloc_udp_structs()
{
    udp_sockets = new udp_trsp_socket*[ AmConfig::SIP_Ifs.size() ];
    udp_servers = new udp_trsp* [ AmConfig::SIPServerThreads
				  * AmConfig::SIP_Ifs.size() ];

    if(udp_sockets && udp_servers)
	return 0;

    return -1;
}

int _SipCtrlInterface::init_udp_servers(int if_num)
{
    udp_trsp_socket* udp_socket = 
	new udp_trsp_socket(if_num,AmConfig::SIP_Ifs[if_num].SigSockOpts
			    | (AmConfig::ForceOutboundIf ? 
			       trsp_socket::force_outbound_if : 0)
			    | (AmConfig::UseRawSockets ?
			       trsp_socket::use_raw_sockets : 0),
			    AmConfig::SIP_Ifs[if_num].NetIfIdx);
	
    if(!AmConfig::SIP_Ifs[if_num].PublicIP.empty()) {
	udp_socket->set_public_ip(AmConfig::SIP_Ifs[if_num].PublicIP);
    }

    if(udp_socket->bind(AmConfig::SIP_Ifs[if_num].LocalIP,
			AmConfig::SIP_Ifs[if_num].LocalPort) < 0){

	ERROR("Could not bind SIP/UDP socket to %s:%i",
	      AmConfig::SIP_Ifs[if_num].LocalIP.c_str(),
	      AmConfig::SIP_Ifs[if_num].LocalPort);

	delete udp_socket;
	return -1;
    }

    if(udp_rcvbuf > 0) {
	udp_socket->set_recvbuf_size(udp_rcvbuf);
    }

    trans_layer::instance()->register_transport(udp_socket);
    udp_sockets[if_num] = udp_socket;
    inc_ref(udp_socket);
    nr_udp_sockets++;

    for(int j=0; j<AmConfig::SIPServerThreads;j++){
	udp_servers[if_num * AmConfig::SIPServerThreads + j] = 
	    new udp_trsp(udp_socket);
	nr_udp_servers++;
    }

    return 0;
}

int _SipCtrlInterface::alloc_tcp_structs()
{
    tcp_sockets = new tcp_server_socket*[ AmConfig::SIP_Ifs.size() ];
    tcp_servers = new tcp_trsp* [ AmConfig::SIP_Ifs.size() ];

    if(tcp_sockets && tcp_servers)
	return 0;

    return -1;
}

int _SipCtrlInterface::init_tcp_servers(int if_num)
{
    tcp_server_socket* tcp_socket = new tcp_server_socket(if_num);

    if(!AmConfig::SIP_Ifs[if_num].PublicIP.empty()) {
     	tcp_socket->set_public_ip(AmConfig::SIP_Ifs[if_num].PublicIP);
    }

    tcp_socket->set_connect_timeout(AmConfig::SIP_Ifs[if_num].tcp_connect_timeout);
    tcp_socket->set_idle_timeout(AmConfig::SIP_Ifs[if_num].tcp_idle_timeout);

    if(tcp_socket->bind(AmConfig::SIP_Ifs[if_num].LocalIP,
			AmConfig::SIP_Ifs[if_num].LocalPort) < 0){

	ERROR("Could not bind SIP/TCP socket to %s:%i",
	      AmConfig::SIP_Ifs[if_num].LocalIP.c_str(),
	      AmConfig::SIP_Ifs[if_num].LocalPort);

	delete tcp_socket;
	return -1;
    }

    //TODO: add some more threads
    tcp_socket->add_threads(AmConfig::SIPServerThreads);

    trans_layer::instance()->register_transport(tcp_socket);
    tcp_sockets[if_num] = tcp_socket;
    inc_ref(tcp_socket);
    nr_tcp_sockets++;

    tcp_servers[if_num] = new tcp_trsp(tcp_socket);
    nr_tcp_servers++;

    return 0;
}

int _SipCtrlInterface::load()
{
    if (!AmConfig::OutboundProxy.empty()) {
	sip_uri parsed_uri;
	if (parse_uri(&parsed_uri, (char *)AmConfig::OutboundProxy.c_str(),
		      AmConfig::OutboundProxy.length()) < 0) {
	    ERROR("invalid outbound_proxy specified\n");
	    return -1;
	}
    }

    AmConfigReader cfg;
    string cfgfile = AmConfig::ConfigurationFile.c_str();
    if (file_exists(cfgfile) && !cfg.loadFile(cfgfile)) {
	if (cfg.hasParameter("accept_fr_without_totag")) {
	    trans_layer::accept_fr_without_totag = 
		cfg.getParameter("accept_fr_without_totag") == "yes";
	}
	DBG("accept_fr_without_totag = %s\n", 
	    trans_layer::accept_fr_without_totag?"yes":"no");

	if (cfg.hasParameter("default_bl_ttl")) {
	    trans_layer::default_bl_ttl = 
		cfg.getParameterInt("default_bl_ttl",
				    trans_layer::default_bl_ttl);
	}
	DBG("default_bl_ttl = %u\n",trans_layer::default_bl_ttl);

	if (cfg.hasParameter("log_raw_messages")) {
	    string msglog = cfg.getParameter("log_raw_messages");
	    if (msglog == "no") trsp_socket::log_level_raw_msgs = -1;
	    else if (msglog == "error") trsp_socket::log_level_raw_msgs = L_ERR;
	    else if (msglog == "warn")  trsp_socket::log_level_raw_msgs = L_WARN;
	    else if (msglog == "info")  trsp_socket::log_level_raw_msgs = L_INFO;
	    else if (msglog == "debug") trsp_socket::log_level_raw_msgs = L_DBG;
	}
	DBG("log_raw_messages level = %d\n", 
	    trsp_socket::log_level_raw_msgs);

	if (cfg.hasParameter("log_parsed_messages")) {
	    log_parsed_messages = cfg.getParameter("log_parsed_messages")=="yes";
	}
	DBG("log_parsed_messages = %s\n", 
	    log_parsed_messages?"yes":"no");

	if (cfg.hasParameter("udp_rcvbuf")) {
	    unsigned int config_udp_rcvbuf = -1;
	    if (str2i(cfg.getParameter("udp_rcvbuf"), config_udp_rcvbuf)) {
		ERROR("invalid value specified for udp_rcvbuf\n");
		return false;
	    }
	    udp_rcvbuf = config_udp_rcvbuf;
	    DBG("udp_rcvbuf = %d\n", udp_rcvbuf);
	}

    } else {
	DBG("assuming SIP default settings.\n");
    }

    if(alloc_udp_structs() < 0) {
	ERROR("no enough memory to alloc UDP structs");
	return -1;
    }

    // Init UDP transport instances
    for(unsigned int i=0; i<AmConfig::SIP_Ifs.size();i++) {
	if(init_udp_servers(i) < 0) {
	    return -1;
	}
    }

    if(alloc_tcp_structs() < 0) {
	ERROR("no enough memory to alloc TCP structs");
	return -1;
    }

    // Init TCP transport instances
    for(unsigned int i=0; i<AmConfig::SIP_Ifs.size();i++) {
	if(init_tcp_servers(i) < 0) {
	    return -1;
	}
    }

    return 0;    
}

_SipCtrlInterface::_SipCtrlInterface()
    : stopped(false),
      nr_udp_sockets(0), udp_sockets(NULL),
      nr_udp_servers(0), udp_servers(NULL),
      nr_tcp_sockets(0), tcp_sockets(NULL),
      nr_tcp_servers(0), tcp_servers(NULL)
{
    trans_layer::instance()->register_ua(this);
}

int _SipCtrlInterface::cancel(trans_ticket* tt, const string& dialog_id,
			      unsigned int inv_cseq, const string& hdrs)
{
    return trans_layer::instance()->cancel(tt,stl2cstr(dialog_id),
					   inv_cseq,stl2cstr(hdrs));
}

int _SipCtrlInterface::send(AmSipRequest &req, const string& dialog_id,
			    const string& next_hop, int out_interface,
			    unsigned int flags, msg_logger* logger)
{
    if(req.method == "CANCEL")
	return cancel(&req.tt, dialog_id, req.cseq, req.hdrs);

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
    int err = parse_headers(msg,&c,c+req.from.length());

    c = (char*)req.to.c_str();
    err = err || parse_headers(msg,&c,c+req.to.length());

    if(err){
	ERROR("Malformed To or From header\n");
	delete msg;
	return -1;
    }

    string cseq = int2str(req.cseq)
	+ " " + req.method;

    msg->cseq = new sip_header(0,SIP_HDR_CSEQ,stl2cstr(cseq));
    msg->hdrs.push_back(msg->cseq);

    msg->callid = new sip_header(0,SIP_HDR_CALL_ID,stl2cstr(req.callid));
    msg->hdrs.push_back(msg->callid);

    if(!req.contact.empty()){

	c = (char*)req.contact.c_str();
	err = parse_headers(msg,&c,c+req.contact.length());
	if(err){
	    ERROR("Malformed Contact header\n");
	    delete msg;
	    return -1;
	}
    }
    
    if(!req.route.empty()){
	
 	c = (char*)req.route.c_str();
	err = parse_headers(msg,&c,c+req.route.length());
	
	if(err){
	    ERROR("Route headers parsing failed\n");
	    ERROR("Faulty headers were: <%s>\n",req.route.c_str());
	    delete msg;
	    return -1;
	}
    }

    if(req.max_forwards < 0) {
	req.max_forwards = AmConfig::MaxForwards;
    }

    string mf = int2str(req.max_forwards);
    msg->hdrs.push_back(new sip_header(0,SIP_HDR_MAX_FORWARDS,stl2cstr(mf)));

    if(!req.hdrs.empty()) {
	
 	c = (char*)req.hdrs.c_str();
	
 	err = parse_headers(msg,&c,c+req.hdrs.length());
	
	if(err){
	    ERROR("Additional headers parsing failed\n");
	    ERROR("Faulty headers were: <%s>\n",req.hdrs.c_str());
	    delete msg;
	    return -1;
	}
    }

    string body;
    string content_type;

    if(!req.body.empty()){
	content_type = req.body.getCTHdr();
	msg->content_type = new sip_header(0,SIP_HDR_CONTENT_TYPE,
					   stl2cstr(content_type));
	msg->hdrs.push_back(msg->content_type);
	req.body.print(body);
	msg->body = stl2cstr(body);
    }

    int res = trans_layer::instance()->send_request(msg,&req.tt,
						    stl2cstr(dialog_id),
						    stl2cstr(next_hop),
						    out_interface,
						    flags,logger);
    delete msg;

    return res;
}

int _SipCtrlInterface::run()
{
    DBG("Starting SIP control interface\n");
    wheeltimer::instance()->start();

    if (NULL != udp_servers) {
	for(int i=0; i<nr_udp_servers;i++){
	    udp_servers[i]->start();
	}
    }

    if (NULL != tcp_servers) {
	for(int i=0; i<nr_tcp_servers;i++){
	    tcp_servers[i]->start();
	}
    }

    while (!stopped.get()) {
        stopped.wait_for();
    }

    DBG("SIP control interface ending\n");
    return 0;
}

void _SipCtrlInterface::stop()
{
    stopped.set(true);
}

void _SipCtrlInterface::cleanup()
{
    DBG("Stopping SIP control interface threads\n");

    if (NULL != udp_servers) {
	for(int i=0; i<nr_udp_servers;i++){
	    udp_servers[i]->stop();
	    udp_servers[i]->join();
	    delete udp_servers[i];
	}

	delete [] udp_servers;
	udp_servers = NULL;
	nr_udp_servers = 0;
    }

    if (NULL != tcp_servers) {
	for(int i=0; i<nr_tcp_servers;i++){
	    tcp_servers[i]->stop();
	    tcp_servers[i]->join();
	    delete tcp_servers[i];
	}

	delete [] tcp_servers;
	tcp_servers = NULL;
	nr_tcp_servers = 0;
    }

    trans_layer::instance()->clear_transports();

    if (NULL != udp_sockets) {
	for(int i=0; i<nr_udp_sockets;i++){
	    DBG("dec_ref(%p)",udp_sockets[i]);
	    dec_ref(udp_sockets[i]);
	}

	delete [] udp_sockets;
	udp_sockets = NULL;
	nr_udp_sockets = 0;
    }

    if (NULL != tcp_sockets) {
	for(int i=0; i<nr_tcp_sockets;i++){
	    DBG("dec_ref(%p)",tcp_sockets[i]);
	    dec_ref(tcp_sockets[i]);
	}

	delete [] tcp_sockets;
	tcp_sockets = NULL;
	nr_tcp_sockets = 0;
    }
}

int _SipCtrlInterface::send(const AmSipReply &rep, const string& dialog_id,
			   msg_logger* logger)
{
    sip_msg msg;

    if(!rep.hdrs.empty()) {

	char* c = (char*)rep.hdrs.c_str();
	int err = parse_headers(&msg,&c,c+rep.hdrs.length());
	if(err){
	    ERROR("Malformed additional header\n");
	    return -1;
	}
    }

    if(!rep.contact.empty()){

	char* c = (char*)rep.contact.c_str();
	int err = parse_headers(&msg,&c,c+rep.contact.length());
	if(err){
	    ERROR("Malformed Contact header\n");
	    return -1;
	}
    }

    string body;
    string content_type;
    if(!rep.body.empty()) {
	content_type = rep.body.getCTHdr();
	rep.body.print(body);
    	if(content_type.empty()){
    	    ERROR("Reply does not contain a Content-Type whereby body is not empty\n");
    	    return -1;
    	}
	msg.body = stl2cstr(body);
	msg.hdrs.push_back(new sip_header(sip_header::H_CONTENT_TYPE,
					  SIP_HDR_CONTENT_TYPE,
					  stl2cstr(content_type)));
    }

    msg.type = SIP_REPLY;
    msg.u.reply = new sip_reply(rep.code,stl2cstr(rep.reason));

    return
	trans_layer::instance()->send_reply(&msg,(trans_ticket*)&rep.tt,
					    stl2cstr(dialog_id),
					    stl2cstr(rep.to_tag),logger);
}


inline bool _SipCtrlInterface::sip_msg2am_request(const sip_msg *msg, 
						 const trans_ticket& tt,
						 AmSipRequest &req)
{
    assert(msg);
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    req.method   = c2stlstr(msg->u.request->method_str);
    req.user     = c2stlstr(msg->u.request->ruri.user);
    req.domain   = c2stlstr(msg->u.request->ruri.host);
    req.r_uri    = c2stlstr(msg->u.request->ruri_str);
    req.tt       = tt;

    if(get_contact(msg) && get_contact(msg)->value.len){

	sip_nameaddr na;
	cstring contact = get_contact(msg)->value;
	if(parse_first_nameaddr(&na,contact.s,contact.len) < 0) {
	    WARN("Contact parsing failed\n");
	    WARN("\tcontact = '%.*s'\n",contact.len,contact.s);
	    WARN("\trequest = '%.*s'\n",msg->len,msg->buf);

	    trans_layer::instance()->send_sf_error_reply(&tt, msg, 400, 
							 "Bad Contact");
	    return false;
	}

	const char* c = na.addr.s;
	if((na.addr.len != 1) || (*c != '*')) {
	    sip_uri u;
	    if(parse_uri(&u,na.addr.s,na.addr.len)){
		DBG("'Contact' in new request contains a malformed URI\n");
		DBG("\tcontact uri = '%.*s'\n",na.addr.len,na.addr.s);
		DBG("\trequest = '%.*s'\n",msg->len,msg->buf);

		trans_layer::instance()->
		    send_sf_error_reply(&tt, msg, 400, "Malformed Contact URI");
		return false;
	    }

	    req.from_uri = c2stlstr(na.addr);
	}

	list<sip_header*>::const_iterator c_it = msg->contacts.begin();
	req.contact = c2stlstr((*c_it)->value);
	++c_it;

	for(;c_it!=msg->contacts.end(); ++c_it){
	    req.contact += ", " + c2stlstr((*c_it)->value);
	}
    }
    else {
	if (req.method == SIP_METH_INVITE) {
	    DBG("Request has no contact header\n");
	    DBG("\trequest = '%.*s'\n",msg->len,msg->buf);
	    trans_layer::instance()->
		send_sf_error_reply(&tt, msg, 400, "Missing Contact-HF");
	    return false;
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
    req.cseq_method = c2stlstr(get_cseq(msg)->method_str);
    req.via_branch = c2stlstr(msg->via_p1->branch);

    if (msg->rack) {
        req.rseq = get_rack(msg)->rseq;
	req.rack_method = c2stlstr(get_rack(msg)->method_str);
	req.rack_cseq = get_rack(msg)->cseq;
    }

    if (msg->content_type) {

	if(req.body.parse(c2stlstr(msg->content_type->value),
			  (unsigned char*)msg->body.s,
			  msg->body.len) < 0) {
	    DBG("could not parse MIME body\n");
	}
	else {
	    DBG("MIME body successfully parsed\n");
	    // some debug infos?
	}
    }

    prepare_routes_uas(msg->record_route, req.route);
	
    for (list<sip_header *>::const_iterator it = msg->hdrs.begin(); 
	 it != msg->hdrs.end(); ++it) {

	switch((*it)->type) {
	case sip_header::H_OTHER:
	case sip_header::H_REQUIRE:
	    req.hdrs += c2stlstr((*it)->name) + ": " 
		+ c2stlstr((*it)->value) + CRLF;
	    break;
	case sip_header::H_VIA:
	    req.vias += c2stlstr((*it)->name) + ": " 
		+ c2stlstr((*it)->value) + CRLF;
	    break;
	case sip_header::H_MAX_FORWARDS:
	    if(!str2int(c2stlstr((*it)->value),req.max_forwards) ||
	       (req.max_forwards < 0) ||
	       (req.max_forwards > 255)) {
		trans_layer::instance()->
		    send_sf_error_reply(&tt, msg, 400, "Incorrect Max-Forwards");
		return false;
	    }
	    break;
	}
    }

    if(req.max_forwards < 0)
	req.max_forwards = AmConfig::MaxForwards;

    req.remote_ip = get_addr_str(&msg->remote_ip);
    req.remote_port = am_get_port(&msg->remote_ip);

    req.local_ip = get_addr_str(&msg->local_ip);
    req.local_port = am_get_port(&msg->local_ip);

    req.trsp = msg->local_socket->get_transport();
    req.local_if = msg->local_socket->get_if();

    req.via1 = c2stlstr(msg->via1->value);
    if(msg->vias.size() > 1) {
	req.first_hop = false;
    } 
    else {
	sip_via* via1 = (sip_via*)msg->via1->p;
	assert(via1); // gets parsed in parse_sip_msg()
	req.first_hop = (via1->parms.size() == 1);
    }

    return true;
}

inline bool _SipCtrlInterface::sip_msg2am_reply(sip_msg *msg, AmSipReply &reply)
{
    if (msg->content_type) {

	if(reply.body.parse(c2stlstr(msg->content_type->value),
			    (unsigned char*)msg->body.s,
			    msg->body.len) < 0) {
	    DBG("could not parse MIME body\n");
	}
	else {
	    DBG("MIME body successfully parsed\n");
	    // some debug infos?
	}
    }

    reply.cseq = get_cseq(msg)->num;
    reply.cseq_method = c2stlstr(get_cseq(msg)->method_str);

    reply.code   = msg->u.reply->code;
    reply.reason = c2stlstr(msg->u.reply->reason);

    if(get_contact(msg) && get_contact(msg)->value.len){

	// parse the first contact
	sip_nameaddr na;
	cstring contact = get_contact(msg)->value;
	if(parse_first_nameaddr(&na,contact.s,contact.len) < 0) {
	    
	    ERROR("Contact nameaddr parsing failed ('%.*s')\n",
		  contact.len,contact.s);
	}
	else {
	    reply.to_uri = c2stlstr(na.addr);
	}

	list<sip_header*>::iterator c_it = msg->contacts.begin();
	reply.contact = c2stlstr((*c_it)->value);
	++c_it;

	for(;c_it!=msg->contacts.end(); ++c_it){
	    reply.contact += "," + c2stlstr((*c_it)->value);
	}
    }

    reply.callid = c2stlstr(msg->callid->value);
    
    reply.to_tag = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    reply.from_tag  = c2stlstr(((sip_from_to*)msg->from->p)->tag);


    prepare_routes_uac(msg->record_route, reply.route);

    unsigned rseq;
    for (list<sip_header*>::iterator it = msg->hdrs.begin(); 
	 it != msg->hdrs.end(); ++it) {
#ifdef PROPAGATE_UNPARSED_REPLY_HEADERS
        reply.unparsed_headers.push_back(AmSipHeader((*it)->name, (*it)->value));
#endif
        switch ((*it)->type) {
          case sip_header::H_OTHER:
          case sip_header::H_REQUIRE:
	      reply.hdrs += c2stlstr((*it)->name) + ": " 
                  + c2stlstr((*it)->value) + CRLF;
              break;
          case sip_header::H_RSEQ:
              if (! parse_rseq(&rseq, (*it)->value.s, (*it)->value.len)) {
                  ERROR("failed to parse (rcvd) '" SIP_HDR_RSEQ "' hdr.\n");
              } else {
                  reply.rseq = rseq;
              }
              break;
        }
    }

    reply.remote_ip = get_addr_str(&msg->remote_ip);
    reply.remote_port = am_get_port(&msg->remote_ip);

    reply.local_ip = get_addr_str(&msg->local_ip);
    reply.local_port = am_get_port(&msg->local_ip);

    if(msg->local_socket)
	reply.trsp = msg->local_socket->get_transport();

    return true;
}


#define DBG_PARAM(p)\
    DBG("%s = <%s>\n",#p,p.c_str());

void _SipCtrlInterface::handle_sip_request(const trans_ticket& tt, sip_msg* msg)
{
    assert(msg);
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipRequest req;

    if(!sip_msg2am_request(msg, tt, req))
	return;

    DBG("Received new request from <%s:%i/%s> on intf #%i\n",
	req.remote_ip.c_str(),req.remote_port,req.trsp.c_str(),req.local_if);

    if (_SipCtrlInterface::log_parsed_messages) {
	//     DBG_PARAM(req.cmd);
	DBG_PARAM(req.method);
	//     DBG_PARAM(req.user);
	//     DBG_PARAM(req.domain);
	DBG_PARAM(req.r_uri);
	DBG_PARAM(req.from_uri);
	DBG_PARAM(req.from);
	DBG_PARAM(req.to);
	DBG_PARAM(req.callid);
	DBG_PARAM(req.from_tag);
	DBG_PARAM(req.to_tag);
	DBG("cseq = <%i>\n",req.cseq);
	DBG("max_forwards = <%i>\n",req.max_forwards);
	DBG_PARAM(req.route);
	DBG("hdrs = <%s>\n",req.hdrs.c_str());
	DBG("body-ct = <%s>\n",req.body.getCTStr().c_str());
    }

    AmSipDispatcher::instance()->handleSipMsg(req);

    DBG("^^ M [%s|%s] Ru SIP request %s handled ^^\n",
	req.callid.c_str(), req.to_tag.c_str(), req.method.c_str());
}

void _SipCtrlInterface::handle_sip_reply(const string& dialog_id, sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipReply   reply;


    if (! sip_msg2am_reply(msg, reply)) {
      ERROR("failed to convert sip_msg to AmSipReply\n");
      // trans_bucket::match_reply only uses via_branch & cseq
      reply.cseq = get_cseq(msg)->num;
      reply.cseq_method = c2stlstr(get_cseq(msg)->method_str);
      reply.code = 500;
      reply.reason = "Internal Server Error";
      reply.callid = c2stlstr(msg->callid->value);
      reply.to_tag = c2stlstr(((sip_from_to*)msg->to->p)->tag);
      reply.from_tag  = c2stlstr(((sip_from_to*)msg->from->p)->tag);
      AmSipDispatcher::instance()->handleSipMsg(dialog_id, reply);
      return;
    }
    
    DBG("Received reply: %i %s\n",reply.code,reply.reason.c_str());
    DBG_PARAM(reply.callid);
    DBG_PARAM(reply.from_tag);
    DBG_PARAM(reply.to_tag);
    DBG_PARAM(reply.contact);
    DBG_PARAM(reply.to_uri);
    DBG("cseq = <%i>\n",reply.cseq);
    DBG_PARAM(reply.route);
    DBG("hdrs = <%s>\n",reply.hdrs.c_str());
    DBG("body-ct = <%s>\n",reply.body.getCTStr().c_str());

    AmSipDispatcher::instance()->handleSipMsg(dialog_id, reply);

    DBG("^^ M [%s|%s] ru SIP reply %u %s handled ^^\n",
	reply.callid.c_str(), reply.from_tag.c_str(),
	reply.code, reply.reason.c_str());
}

void _SipCtrlInterface::handle_reply_timeout(AmSipTimeoutEvent::EvType evt,
    sip_trans *tr, trans_bucket *buk)
{
  AmSipTimeoutEvent *tmo_evt;
  
  switch (evt) {
  case AmSipTimeoutEvent::noACK: {
      sip_cseq* cseq = dynamic_cast<sip_cseq*>(tr->msg->cseq->p);

      if(!cseq){
          ERROR("missing CSeq\n");
          return;
      }
    tmo_evt = new AmSipTimeoutEvent(evt, cseq->num);
    }
    break;

  case AmSipTimeoutEvent::noPRACK: {
      sip_msg msg(tr->retr_buf, tr->retr_len);

      char* err_msg=0;
      int err = parse_sip_msg(&msg, err_msg);
      if (err) {
          ERROR("failed to parse (own) reply[%d]: %s.\n", err, 
              err_msg ? err_msg : "???");
          return;
      }

      AmSipReply reply;
      if (! sip_msg2am_reply(&msg, reply)) {
          ERROR("failed to convert sip_msg to AmSipReply.\n");
          return;
      }

      AmSipRequest request;
      sip_msg2am_request(tr->msg, trans_ticket(tr, buk), request);

      DBG("Reply timed out: %i %s\n",reply.code,reply.reason.c_str());
      DBG_PARAM(reply.callid);
      DBG_PARAM(reply.to_tag);
      DBG_PARAM(reply.from_tag);
      DBG("cseq = <%i>\n",reply.cseq);

      tmo_evt = new AmSipTimeoutEvent(evt, request, reply);
    }
    break;

  default:
    ERROR("BUG: unexpected timout event type '%d'.\n", evt);
    return;
  }

  cstring dlg_id = tr->to_tag;
  if(tr->dialog_id.len) {
      dlg_id = tr->dialog_id;
  }

  if(!AmEventDispatcher::instance()->post(c2stlstr(dlg_id), tmo_evt)){
      DBG("Could not post timeout event (sess. id: %.*s)\n",dlg_id.len,dlg_id.s);
      delete tmo_evt;
  }
}

#undef DBG_PARAM

void _SipCtrlInterface::prepare_routes_uac(const list<sip_header*>& routes, string& route_field)
{
    if(routes.empty())
	return;
	
    list<sip_header*>::const_reverse_iterator it_rh = routes.rbegin();
    if(parse_route(*it_rh) < 0){
	DBG("Could not parse route header [%.*s]\n",
	    (*it_rh)->value.len,(*it_rh)->value.s);
	return;
    }
    sip_route* route = (sip_route*)(*it_rh)->p;

    list<route_elmt*>::const_reverse_iterator it_re = route->elmts.rbegin();
    route_field = c2stlstr((*it_re)->route);
    
    while(true) {
	
	if(++it_re == route->elmts.rend()) {
	    if(++it_rh == routes.rend()){
		DBG("route_field = [%s]\n",route_field.c_str());
		return;
	    }

	    if(parse_route(*it_rh) < 0){
		DBG("Could not parse route header [%.*s]\n",
		    (*it_rh)->value.len,(*it_rh)->value.s);
		return;
	    }
	    route = (sip_route*)(*it_rh)->p;
	    if(route->elmts.empty())
		return;
	    it_re = route->elmts.rbegin();
	}
	
	route_field += ", " + c2stlstr((*it_re)->route);
    }

}

void _SipCtrlInterface::prepare_routes_uas(const list<sip_header*>& routes, string& route_field)
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
