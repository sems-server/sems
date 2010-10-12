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
#include "AmSipHeaders.h"

#include "sip/trans_layer.h"
#include "sip/sip_parser.h"
#include "sip/parse_header.h"
#include "sip/parse_from_to.h"
#include "sip/parse_cseq.h"
#include "sip/parse_extensions.h"
#include "sip/parse_100rel.h"
#include "sip/parse_route.h"
#include "sip/trans_table.h"
#include "sip/sip_trans.h"
#include "sip/wheeltimer.h"
#include "sip/msg_hdrs.h"
#include "sip/udp_trsp.h"

#include "log.h"

#include <assert.h>

#include "AmApi.h"
#include "AmConfigReader.h"
#include "AmSipDispatcher.h"
#include "AmEventDispatcher.h"
#include "AmSipEvent.h"

int SipCtrlInterface::log_raw_messages = 3;
bool SipCtrlInterface::log_parsed_messages = true;
int SipCtrlInterface::udp_rcvbuf = -1;

int SipCtrlInterface::load()
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

    return 0;
    
}

SipCtrlInterface::SipCtrlInterface()
    : stopped(false), udp_servers(NULL), udp_sockets(NULL),
      nr_udp_sockets(0), nr_udp_servers(0)
{
    trans_layer::instance()->register_ua(this);
}

int SipCtrlInterface::cancel(trans_ticket* tt)
{
    return trans_layer::instance()->cancel(tt);
}

int SipCtrlInterface::send(AmSipRequest &req,
			   const string& next_hop_ip, unsigned short next_hop_port,
			   int out_interface)
{
    if(req.method == "CANCEL")
	return cancel(&req.tt);

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
	
 	c = (char*)req.route.c_str();
	err = parse_headers(msg,&c);
	
	if(err){
	    ERROR("Route headers parsing failed\n");
	    ERROR("Faulty headers were: <%s>\n",req.route.c_str());
	    delete msg;
	    return -1;
	}
    }

    if(!req.hdrs.empty()) {
	
 	c = (char*)req.hdrs.c_str();
	
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

    int res = trans_layer::instance()->send_request(msg,&req.tt,
						    stl2cstr(next_hop_ip),next_hop_port,
						    out_interface);
    delete msg;

    return res;
}

int SipCtrlInterface::run()
{
    DBG("Starting SIP control interface\n");

    udp_sockets = new udp_trsp_socket*[AmConfig::Ifs.size()];
    udp_servers = new udp_trsp*[AmConfig::SIPServerThreads * AmConfig::Ifs.size()];

    wheeltimer::instance()->start();

    // Init transport instances
    for(unsigned int i=0; i<AmConfig::Ifs.size();i++) {

	udp_trsp_socket* udp_socket = new udp_trsp_socket;

	if(udp_socket->bind(AmConfig::Ifs[i].LocalSIPIP,
			    AmConfig::Ifs[i].LocalSIPPort) < 0){

	    ERROR("Could not bind SIP/UDP socket to %s:%i",
		  AmConfig::Ifs[i].LocalSIPIP.c_str(),
		  AmConfig::Ifs[i].LocalSIPPort);

	    delete udp_socket;
	    return -1;
	}

	trans_layer::instance()->register_transport(udp_socket);
	udp_sockets[i] = udp_socket;
	nr_udp_sockets++;

	for(int j=0; j<AmConfig::SIPServerThreads;j++){
	    udp_servers[i*AmConfig::SIPServerThreads + j] = new udp_trsp(udp_socket);
	    udp_servers[i*AmConfig::SIPServerThreads + j]->start();
	    nr_udp_servers++;
	}
    }

    while (!stopped.get()) {
        stopped.wait_for();
    }

    DBG("SIP control interface ending\n");    
    return 0;
}

void SipCtrlInterface::stop()
{
    stopped.set(true);
}

void SipCtrlInterface::cleanup()
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

    trans_layer::instance()->clear_transports();

    if (NULL != udp_sockets) {
	for(int i=0; i<nr_udp_sockets;i++){
	    delete udp_sockets[i];
	}

	delete [] udp_sockets;
	udp_sockets = NULL;
	nr_udp_sockets = 0;
    }
}

int SipCtrlInterface::send(const AmSipReply &rep,
			   const string& next_hop_ip, unsigned short next_hop_port,
			   int out_interface)
{
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


    /* check if we need to store RSeq. */
    // FIXME: shouldn't the global "100rel==on?" check be present here??
    if(100 < rep.code && rep.code < 200 && rep.method == SIP_METH_INVITE) {
        unsigned ext = 0;
        unsigned rseq = 0;
        for(list<sip_header*>::iterator it = msg.hdrs.begin();
                !(ext && rseq) && it != msg.hdrs.end(); ++it) {
            // check if there's a Require field containing 100rel; if there
            // is, look for RSeq and store it's value in transaction;
            DBG("HT:%d, ext:%d, rseq:%d.\n", (*it)->type, ext, rseq);
            switch ((*it)->type) {
            case sip_header::H_REQUIRE:
                if (ext)
                    // there was alrady a(nother?) Require HF
                    continue;
                if(!parse_extensions(&ext, (*it)->value.s, (*it)->value.len)) {
                    ERROR("failed to parse(own?)'" SIP_HDR_REQUIRE "' hdr.\n");
                    continue;
                }
                if (rseq) { // our RSeq's are never 0
                    rep.tt._t->last_rseq = rseq;
                    continue; // the end.
                }
                break;

            case sip_header::H_RSEQ:
                if (rseq) {
                    ERROR("multiple '" SIP_HDR_RSEQ "' headers in reply.\n");
                    continue;
                }
                if (! parse_rseq(&rseq, (*it)->value.s, (*it)->value.len)) {
                    ERROR("failed to parse (own?) '" SIP_HDR_RSEQ "' hdr.\n");
                    continue;
                }
                if (ext) {
                    rep.tt._t->last_rseq = rseq;
                    continue; // the end.
                }
            }
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

    int ret =
	trans_layer::instance()->send_reply((trans_ticket*)&rep.tt,
					    rep.code,stl2cstr(rep.reason),
					    stl2cstr(rep.local_tag),
					    cstring(hdrs_buf,hdrs_len), stl2cstr(rep.body),
					    stl2cstr(next_hop_ip),next_hop_port,
					    out_interface);

    delete [] hdrs_buf;

    return ret;
}


inline void SipCtrlInterface::sip_msg2am_request(const sip_msg *msg, 
    AmSipRequest &req)
{
    req.cmd      = "sems";
    req.method   = c2stlstr(msg->u.request->method_str);
    req.user     = c2stlstr(msg->u.request->ruri.user);
    req.domain   = c2stlstr(msg->u.request->ruri.host);
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
	if (req.method == SIP_METH_INVITE) {
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
    req.via_branch = c2stlstr(msg->via_p1->branch);
    req.body     = c2stlstr(msg->body);

    if (msg->rack) {
        req.rseq = get_rack(msg)->rseq;
	req.rack_method = c2stlstr(get_rack(msg)->method_str);
	req.rack_cseq = get_rack(msg)->cseq;
    }

    if (msg->content_type)
 	req.content_type = c2stlstr(msg->content_type->value);

    prepare_routes_uas(msg->record_route, req.route);
	
    for (list<sip_header *>::const_iterator it = msg->hdrs.begin(); 
	 it != msg->hdrs.end(); ++it) {
	if((*it)->type == sip_header::H_OTHER || 
                (*it)->type == sip_header::H_REQUIRE){
	    req.hdrs += c2stlstr((*it)->name) + ": " 
		+ c2stlstr((*it)->value) + CRLF;
	}
    }

    req.via1 = c2stlstr(msg->via1->value);

    req.remote_ip = get_addr_str(((sockaddr_in*)&msg->remote_ip)->sin_addr).c_str();
    req.remote_port = htons(((sockaddr_in*)&msg->remote_ip)->sin_port);
    req.local_ip = get_addr_str(((sockaddr_in*)&msg->local_ip)->sin_addr).c_str();
    req.local_port = htons(((sockaddr_in*)&msg->local_ip)->sin_port);
}

inline bool SipCtrlInterface::sip_msg2am_reply(sip_msg *msg, AmSipReply &reply)
{
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
	    return false;
	}
	
	// 'Contact' header?
	reply.next_request_uri = c2stlstr(na.addr);
	
	list<sip_header*>::iterator c_it = msg->contacts.begin();
	reply.contact = c2stlstr((*c_it)->value);
	++c_it;

	for(;c_it!=msg->contacts.end(); ++c_it){
	    reply.contact += "," + c2stlstr((*c_it)->value);
	}
    }

    reply.callid = c2stlstr(msg->callid->value);
    
    reply.remote_tag = c2stlstr(((sip_from_to*)msg->to->p)->tag);
    reply.local_tag  = c2stlstr(((sip_from_to*)msg->from->p)->tag);


    prepare_routes_uac(msg->record_route, reply.route);

    unsigned rseq;
    for (list<sip_header*>::iterator it = msg->hdrs.begin(); 
	 it != msg->hdrs.end(); ++it) {
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

    reply.remote_ip = get_addr_str(((sockaddr_in*)&msg->remote_ip)->sin_addr).c_str();
    reply.remote_port = htons(((sockaddr_in*)&msg->remote_ip)->sin_port);
    reply.local_ip = get_addr_str(((sockaddr_in*)&msg->local_ip)->sin_addr).c_str();
    reply.local_port = htons(((sockaddr_in*)&msg->local_ip)->sin_port);

    return true;
}


#define DBG_PARAM(p)\
    DBG("%s = <%s>\n",#p,p.c_str());

void SipCtrlInterface::handle_sip_request(const trans_ticket& tt, sip_msg* msg)
{
    assert(msg);
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipRequest req;

    sip_msg2am_request(msg, req);

    req.tt = tt;

    DBG("Received new request\n");
    if (SipCtrlInterface::log_parsed_messages) {
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
	DBG_PARAM(req.route);
	DBG("hdrs = <%s>\n",req.hdrs.c_str());
	DBG("body = <%s>\n",req.body.c_str());
    }

    AmSipDispatcher::instance()->handleSipMsg(req);

    DBG("^^ M [%s|%s] Ru SIP request %s handled ^^\n",
	req.callid.c_str(), req.to_tag.c_str(), req.method.c_str());
}

void SipCtrlInterface::handle_sip_reply(sip_msg* msg)
{
    assert(msg->from && msg->from->p);
    assert(msg->to && msg->to->p);
    
    AmSipReply   reply;

    if (! sip_msg2am_reply(msg, reply))
      return;
    
    DBG("Received reply: %i %s\n",reply.code,reply.reason.c_str());
    DBG_PARAM(reply.callid);
    DBG_PARAM(reply.local_tag);
    DBG_PARAM(reply.remote_tag);
    DBG("cseq = <%i>\n",reply.cseq);

    AmSipDispatcher::instance()->handleSipMsg(reply);

    DBG("^^ M [%s|%s] ru SIP reply %u %s handled ^^\n",
	reply.callid.c_str(), reply.local_tag.c_str(),
	reply.code, reply.reason.c_str());
}

void SipCtrlInterface::handle_reply_timeout(AmSipTimeoutEvent::EvType evt,
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
      sip_msg2am_request(tr->msg, request);
      request.tt = trans_ticket(tr, buk);

      DBG("Reply timed out: %i %s\n",reply.code,reply.reason.c_str());
      DBG_PARAM(reply.callid);
      DBG_PARAM(reply.local_tag);
      DBG_PARAM(reply.remote_tag);
      DBG("cseq = <%i>\n",reply.cseq);

      tmo_evt = new AmSipTimeoutEvent(evt, request, reply);
    }
    break;

  default:
    ERROR("BUG: unexpected timout event type '%d'.\n", evt);
    return;
  }

  if(!AmEventDispatcher::instance()->post(c2stlstr(tr->to_tag), tmo_evt)){
      ERROR("Could not post timeout event (sess. id: %.*s)\n",tr->to_tag.len,tr->to_tag.s);
      delete tmo_evt;
  }
}

#undef DBG_PARAM

void SipCtrlInterface::prepare_routes_uac(const list<sip_header*>& routes, string& route_field)
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
	
	if(++it_re == route->elmts.rend()){
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
