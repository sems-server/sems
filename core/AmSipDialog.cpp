/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmSipDialog.h"
#include "AmConfig.h"
#include "AmSession.h"
#include "AmUtils.h"
#include "AmSipHeaders.h"
#include "SipCtrlInterface.h"
#include "sems.h"

#include "sip/parse_route.h"
#include "sip/parse_uri.h"

const char* AmSipDialog::status2str[4]  = { 	
  "Disconnected",
  "Pending",
  "Connected",
  "Disconnecting" };


AmSipDialog::AmSipDialog(AmSipDialogEventHandler* h)
  : status(Disconnected),cseq(10),r_cseq_i(false),hdl(h),pending_invites(0),
    outbound_proxy(AmConfig::OutboundProxy),
    force_outbound_proxy(AmConfig::ForceOutboundProxy),
    reliable_1xx(AmConfig::rel100),
    rseq(0), rseq_1st(0), rseq_confirmed(false),
    next_hop_port(AmConfig::NextHopPort),
    next_hop_ip(AmConfig::NextHopIP),
    next_hop_for_replies(AmConfig::NextHopForReplies),
    outbound_interface(-1), out_intf_for_replies(false)
{
}

AmSipDialog::~AmSipDialog()
{
  DBG("callid = %s\n",callid.c_str());
  DBG("local_tag = %s\n",local_tag.c_str());
  DBG("uac_trans.size() = %u\n",(unsigned int)uac_trans.size());
  if(uac_trans.size()){
    for(TransMap::iterator it = uac_trans.begin();
	it != uac_trans.end(); it++){
	    
      DBG("    cseq = %i; method = %s\n",it->first,it->second.method.c_str());
    }
  }
  DBG("uas_trans.size() = %u\n",(unsigned int)uas_trans.size());
  if(uas_trans.size()){
    for(TransMap::iterator it = uas_trans.begin();
	it != uas_trans.end(); it++){
	    
      DBG("    cseq = %i; method = %s\n",it->first,it->second.method.c_str());
    }
  }
}

void AmSipDialog::setStatus(int new_status) {
  DBG("setting  SIP dialog status: %s->%s\n",
      status2str[status], status2str[new_status]);

  status = new_status;
}

void AmSipDialog::updateStatus(const AmSipRequest& req)
{
  DBG("AmSipDialog::updateStatus(req = %s)\n", req.method.c_str());

  if (req.method == "ACK") {
    if(hdl)
      hdl->onSipRequest(req);
    return;
  }

  if (req.method == "CANCEL") {

    TransMap::iterator t_it = uas_trans.find(req.cseq);
    if(t_it == uas_trans.end()){
      reply_error(req,481,SIP_REPLY_NOT_EXIST);
      return;
    }

    if(hdl)
      hdl->onSipRequest(req);
    return;
  }

  // Sanity checks
  if (r_cseq_i && req.cseq <= r_cseq){
    INFO("remote cseq lower than previous ones - refusing request\n");
    // see 12.2.2
    reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR, "",
		next_hop_for_replies ? next_hop_ip : "",
		next_hop_for_replies ? next_hop_port : 0);
    return;
  }

  if (req.method == "INVITE") {
    if (pending_invites) {
      reply_error(req,500, SIP_REPLY_SERVER_INTERNAL_ERROR,
		  "Retry-After: " + int2str(get_random() % 10) + CRLF,
		  next_hop_for_replies ? next_hop_ip : "",
		  next_hop_for_replies ? next_hop_port : 0);
      return;
    }

    pending_invites++;
  }

  r_cseq = req.cseq;
  r_cseq_i = true;
  uas_trans[req.cseq] = AmSipTransaction(req.method,req.cseq,req.tt);

  // target refresh requests
  if (req.from_uri.length() &&
      ((req.method.length()==6 &&
	((req.method == "INVITE") ||
	 (req.method == "UPDATE") ||
	 (req.method == "NOTIFY"))) ||
       (req.method == "SUBSCRIBE")))
    {

    remote_uri = req.from_uri;
  }

  if(callid.empty()){
    callid       = req.callid;
    remote_tag   = req.from_tag;
    user         = req.user;
    domain       = req.domain;
    local_uri    = req.r_uri;
    remote_party = req.from;
    local_party  = req.to;
    route        = req.route;
  }

  int cont = rel100OnRequestIn(req);

  if(cont && hdl)
      hdl->onSipRequest(req);
}

int AmSipDialog::rel100OnRequestIn(const AmSipRequest& req)
{
  if (reliable_1xx == REL100_IGNORED)
    return 1;

  /* activate the 100rel, if needed */
  if (req.method == SIP_METH_INVITE) {
    switch(reliable_1xx) {
      case REL100_SUPPORTED: /* if support is on, enforce if asked by UAC */
        if (key_in_list(getHeader(req.hdrs, SIP_HDR_SUPPORTED), 
              SIP_EXT_100REL) ||
            key_in_list(getHeader(req.hdrs, SIP_HDR_REQUIRE), 
              SIP_EXT_100REL)) {
          reliable_1xx = REL100_REQUIRE;
          DBG(SIP_EXT_100REL " now active.\n");
        }
        break;

      case REL100_REQUIRE: /* if support is required, reject if UAC doesn't */
        if (! (key_in_list(getHeader(req.hdrs,SIP_HDR_SUPPORTED), 
              SIP_EXT_100REL) ||
            key_in_list(getHeader(req.hdrs, SIP_HDR_REQUIRE), 
              SIP_EXT_100REL))) {
          ERROR("'" SIP_EXT_100REL "' extension required, but not advertised"
            " by peer.\n");
          if (hdl) hdl->onFailure(FAIL_REL100_421, &req, 0);
          return 0; // has been replied
        }
        break; // 100rel required

      case REL100_DISABLED:
        // TODO: shouldn't this be part of a more general check in SEMS?
        if (key_in_list(getHeader(req.hdrs,SIP_HDR_REQUIRE),SIP_EXT_100REL)) {
          if (hdl) hdl->onFailure(FAIL_REL100_420, &req, 0);
          return 0; // has been replied
        }
        break;

      default:
        ERROR("BUG: unexpected value `%d' for '" SIP_EXT_100REL "' switch.", 
          reliable_1xx);
#ifndef NDEBUG
        abort();
#endif
    } // switch reliable_1xx
  } else if (req.method == SIP_METH_PRACK) {
    if (reliable_1xx != REL100_REQUIRE) {
      WARN("unexpected PRACK received while " SIP_EXT_100REL " not active.\n");
      // let if float up
    } else if (rseq_1st<=req.rseq && req.rseq<=rseq) {
      if (req.rseq == rseq) {
        rseq_confirmed = true; // confirmed
      }
      // else: confirmation for one of the pending 1xx
      DBG("%sRSeq (%u) confirmed.\n", (req.rseq==rseq) ? "latest " : "", rseq);
    }
  }

  return 1;
}

/**
 *
 * update dialog status from UAC Request that we send (e.g. INVITE)
 */
void AmSipDialog::updateStatusFromLocalRequest(const AmSipRequest& req)
{
  if (req.r_uri.length())
    remote_uri = req.r_uri;

  if(callid.empty()){
    DBG("dialog callid is empty, updating from UACRequest\n");
    callid       = req.callid;
    local_tag    = req.from_tag;
    DBG("local_tag = %s\n",local_tag.c_str());
    user         = req.user;
    domain       = req.domain;
    local_uri    = req.from_uri;
    remote_party = req.to;
    local_party  = req.from;
  }
}

int AmSipDialog::updateStatusReply(const AmSipRequest& req, unsigned int code)
{
  TransMap::iterator t_it = uas_trans.find(req.cseq);
  if(t_it == uas_trans.end()){
    ERROR("could not find any transaction matching request\n");
    ERROR("reply code=%i; method=%s; callid=%s; local_tag=%s; "
	  "remote_tag=%s; cseq=%i\n",
	  code,req.method.c_str(),callid.c_str(),local_tag.c_str(),
	  remote_tag.c_str(),req.cseq);
    return -1;
  }
  DBG("reply: transaction found!\n");
    
  AmSipTransaction& t = t_it->second;
  switch(status){

  case Disconnected:
  case Pending:
    if(t.method == "INVITE"){
	
      if(req.method == "CANCEL"){
		
	// wait for somebody
	// to answer 487
	return 0;
      }

      if(code < 200)
	status = Pending;
      else if(code < 300)
	status = Connected;
      else
	status = Disconnected;
    }
	
    break;
  case Connected:
  case Disconnecting:
    if(t.method == "BYE"){
	    
	if(code >= 200)
	    status = Disconnected;
    }
    break;
  }

  if(code >= 200){
    DBG("req.method = %s; t.method = %s\n",
	req.method.c_str(),t.method.c_str());

    if(t.method == "INVITE")
	pending_invites--;

    uas_trans.erase(t_it);
  }

  return 0;
}

void AmSipDialog::updateStatus(const AmSipReply& reply)
{
  TransMap::iterator t_it = uac_trans.find(reply.cseq);
  if(t_it == uac_trans.end()){
    ERROR("could not find any transaction matching reply: %s\n", 
        ((AmSipReply)reply).print().c_str());
    return;
  }
  DBG("updateStatus(rep = %u %s): transaction found!\n",
      reply.code, reply.reason.c_str());

  AmSipTransaction& t = t_it->second;
  int old_dlg_status = status;
  string trans_method = t.method;

  // rfc3261 12.1
  // Dialog established only by 101-199 or 2xx 
  // responses to INVITE

  if ( (reply.code > 100) 
       && (reply.code < 300) ) {

    if(!reply.remote_tag.empty() 
       && (remote_tag.empty() ||
	   ((status < Connected) && (reply.code >= 200))) ) {  

      remote_tag = reply.remote_tag;
    }

    // allow route overwriting
    if ((status < Connected) && !reply.route.empty()) {
      route = reply.route;
    }

    if (reply.next_request_uri.length())
      remote_uri = reply.next_request_uri;
  }

  switch(status){
  case Disconnecting:
    if (trans_method == SIP_METH_INVITE) {
      // ignore provisional reply in canceled INVITE
      if (reply.code < 200)
	break;

      if(reply.code >= 400){
	// CANCEL accepted
	DBG("CANCEL accepted, status -> Disconnected\n");
	status = Disconnected;
      }
      else if(reply.code < 300){
	// CANCEL rejected
	DBG("CANCEL rejected/too late - bye()\n");
	bye();
	// if BYE could not be sent,
	// there is nothing we can do anymore...
      }
    }

    if((trans_method == "BYE") && (reply.code >= 200)){
      // final reply to BYE: Disconnecting -> Disconnected
      status = Disconnected;
    }
    break;

  case Pending:
  case Disconnected:
    // only change status of dialog if reply 
    // to INVITE received
    if (trans_method == SIP_METH_INVITE) { 
      if(reply.code < 200)
	status = Pending;
      else if(reply.code >= 300)
	status = Disconnected;
      else
	status = Connected;
    }
    break;
  default:
    break;
  }

  int cont = rel100OnReplyIn(reply);

  // TODO: remove the transaction only after the dedicated timer has hit
  //       this would help taking care of multiple 2xx replies.
  if(reply.code >= 200){
    // TODO: 
    // - place this somewhere else.
    //   (probably in AmSession...)
    if((reply.code < 300) && (trans_method == SIP_METH_INVITE)) {

	if(hdl) {
	    hdl->onInvite2xx(reply);
	}
	else {
	    send_200_ack(t);
	}
    }
    else {
	uac_trans.erase(t_it);
    }
  }

  if(cont && hdl)
    hdl->onSipReply(reply, old_dlg_status, trans_method);
}


int AmSipDialog::rel100OnReplyIn(const AmSipReply &reply)
{
  if (reliable_1xx == REL100_IGNORED)
    return 1;

  if (status!=Pending && status!=Connected)
    return 1;

  if (100<reply.code && reply.code<200 && reply.method==SIP_METH_INVITE) {
    switch (reliable_1xx) {
    case REL100_SUPPORTED:
      if (key_in_list(getHeader(reply.hdrs, SIP_HDR_REQUIRE), 
          SIP_EXT_100REL))
        reliable_1xx = REL100_REQUIRE;
        // no break!
      else
        break;

    case REL100_REQUIRE:
      if (!key_in_list(getHeader(reply.hdrs,SIP_HDR_REQUIRE),SIP_EXT_100REL) ||
          !reply.rseq) {
        ERROR(SIP_EXT_100REL " not supported or no positive RSeq value in "
            "(reliable) 1xx.\n");
        if (hdl) hdl->onFailure(FAIL_REL100_421, 0, &reply);
      } else {
        DBG(SIP_EXT_100REL " now active.\n");
        if (hdl) hdl->onInvite1xxRel(reply);
      }
      break;

    case REL100_DISABLED:
      // 100rel support disabled
      break;
    default:
      ERROR("BUG: unexpected value `%d' for " SIP_EXT_100REL " switch.", 
          reliable_1xx);
#ifndef NDEBUG
      abort();
#endif
    } // switch reliable 1xx
  } else if (reliable_1xx && reply.method==SIP_METH_PRACK) {
    if (300 <= reply.code) {
      // if PRACK fails, tear down session
      if (hdl) hdl->onFailure(FAIL_REL100_421, 0, &reply);
    } else if (200 <= reply.code) {
      if (hdl) hdl->onPrack2xx(reply);
    } else {
      WARN("received '%d' for " SIP_METH_PRACK " method.\n", reply.code);
    }
    // absorbe the replys for the prack (they've been dispatched through 
    // onPrack2xx, if necessary)
    return 0;
  }
  return 1;
}

void AmSipDialog::uasTimeout(AmSipTimeoutEvent* to_ev)
{
  assert(to_ev);

  switch(to_ev->type){
  case AmSipTimeoutEvent::noACK:
    DBG("Timeout: missing ACK\n");
    if(hdl) hdl->onNoAck(to_ev->cseq);
    break;

  case AmSipTimeoutEvent::noPRACK:
    DBG("Timeout: missing PRACK\n");
    rel100OnTimeout(to_ev->req, to_ev->rpl);
    break;

  case AmSipTimeoutEvent::_noEv:
  default:
    break;
  };
  
  to_ev->processed = true;
}

void AmSipDialog::rel100OnTimeout(const AmSipRequest &req, 
    const AmSipReply &rpl)
{
  if (reliable_1xx == REL100_IGNORED)
    return;

  INFO("reply <%s> timed out (not PRACKed).\n", rpl.print().c_str());
  if (100 < rpl.code && rpl.code < 200 && reliable_1xx == REL100_REQUIRE &&
      rseq == rpl.rseq && rpl.method == SIP_METH_INVITE) {
    INFO("reliable %d reply timed out; rejecting request.\n", rpl.code);
    if(hdl) hdl->onNoPrack(req, rpl);
  } else {
    WARN("reply timed-out, but not reliable.\n"); // debugging
  }
}


bool AmSipDialog::getUACTransPending() {
  return !uac_trans.empty();
}

bool AmSipDialog::getUACInvTransPending() {
  for (TransMap::iterator it=uac_trans.begin();
       it != uac_trans.end(); it++) {
    if (it->second.method == "INVITE")
      return true;
  }
  return false;
}

string AmSipDialog::getContactHdr()
{
  if(contact_uri.empty()) {

    contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) "<sip:";

    if(!user.empty()) {
      contact_uri += user + "@";
    }
    
    int oif = getOutboundIf();
    assert(oif >= 0);
    assert(oif < (int)AmConfig::Ifs.size());

    contact_uri += (AmConfig::Ifs[oif].PublicIP.empty() ? 
		    AmConfig::Ifs[oif].LocalSIPIP : AmConfig::Ifs[oif].PublicIP ) 
      + ":";
    contact_uri += int2str(AmConfig::Ifs[oif].LocalSIPPort);
    contact_uri += ">";

    contact_uri += CRLF;
    
  }

  return contact_uri;
}

/** 
 * Computes, set and return the outbound interface
 * based on remote_uri, next_hop_ip, outbound_proxy, route.
 */
int AmSipDialog::getOutboundIf()
{
  if (outbound_interface >= 0)
    return outbound_interface;

  if(AmConfig::Ifs.size() == 1){
    return (outbound_interface = 0);
  }

  // Destination priority:
  // 1. next_hop_ip
  // 2. outbound_proxy (if 1st req or force_outbound_proxy)
  // 3. first route
  // 4. remote URI
  
  string dest_uri;
  string dest_ip;
  string local_ip;
  multimap<string,unsigned short>::iterator if_it;

  if(!next_hop_ip.empty()) {
    dest_ip = next_hop_ip;
  }
  else if(!outbound_proxy.empty() &&
	  (remote_tag.empty() || force_outbound_proxy)) {
    dest_uri = outbound_proxy;
  }
  else if(!route.empty()){
    // parse first route
    sip_header fr;
    fr.value = stl2cstr(route);
    sip_uri* route_uri = get_first_route_uri(&fr);
    if(!route_uri){
      ERROR("Could not parse route (local_tag='%s';route='%s')",
	    local_tag.c_str(),route.c_str());
      goto error;
    }

    dest_ip = c2stlstr(route_uri->host);
  }
  else {
    dest_uri = remote_uri;
  }

  if(dest_uri.empty() && dest_ip.empty()) {
    ERROR("No destination found (local_tag='%s')",local_tag.c_str());
    goto error;
  }
  
  if(!dest_uri.empty()){
    sip_uri d_uri;
    if(parse_uri(&d_uri,dest_uri.c_str(),dest_uri.length()) < 0){
      ERROR("Could not parse destination URI (local_tag='%s';dest_uri='%s')",
	    local_tag.c_str(),dest_uri.c_str());
      goto error;
    }

    dest_ip = c2stlstr(d_uri.host);
  }

  if(get_local_addr_for_dest(dest_ip,local_ip) < 0){
    ERROR("No local address for dest '%s' (local_tag='%s')",dest_ip.c_str(),local_tag.c_str());
    goto error;
  }

  if_it = AmConfig::LocalSIPIP2If.find(local_ip);
  if(if_it == AmConfig::LocalSIPIP2If.end()){
    ERROR("Could not find a local interface for resolved local IP (local_tag='%s';local_ip='%s')",
	  local_tag.c_str(), local_ip.c_str());
    goto error;
  }

  outbound_interface = if_it->second;
  return outbound_interface;

 error:
  WARN("Error while computing outbound interface: default interface will be used instead.");
  outbound_interface = 0;
  return outbound_interface;
}

void AmSipDialog::resetOutboundIf()
{
  outbound_interface = -1;
}

string AmSipDialog::getRoute() 
{
  string res;

  if(!outbound_proxy.empty() && (force_outbound_proxy || remote_tag.empty())){
    res += "<" + outbound_proxy + ";lr>";

    if(!route.empty()) {
      res += ",";
    }
  }

  res += route;

  if(!res.empty()) {
    res = SIP_HDR_COLSP(SIP_HDR_ROUTE) + res + CRLF;
  }

  return res;
}


int AmSipDialog::reply(const AmSipRequest& req,
		       unsigned int  code,
		       const string& reason,
		       const string& content_type,
		       const string& body,
		       const string& hdrs,
		       int flags)
{
  string m_hdrs = hdrs;

  if(hdl)
    hdl->onSendReply(req,code,reason,
		     content_type,body,m_hdrs,flags);

  rel100OnReplyOut(req, code, m_hdrs);

  AmSipReply reply;

  reply.method = req.method;
  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  reply.local_tag = local_tag;
  reply.hdrs = m_hdrs;

  if (!flags&SIP_FLAGS_VERBATIM) {
    // add Signature
    if (AmConfig::Signature.length())
      reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;
  }

  if (code < 300 && req.method != "CANCEL" && req.method != "BYE")
    /* if 300<=code<400, explicit contact setting should be done */
    reply.contact = getContactHdr();

  reply.content_type = content_type;
  reply.body = body;

  if(updateStatusReply(req,code))
    return -1;

  int ret = SipCtrlInterface::send(reply, next_hop_for_replies ? next_hop_ip : "",
				   next_hop_for_replies ? next_hop_port : 0,
				   out_intf_for_replies ? outbound_interface : -1 );
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),req.method.c_str(),req.callid.c_str(),req.cseq);
  }
  return ret;
}


void AmSipDialog::rel100OnReplyOut(const AmSipRequest &req, unsigned int code, 
    string &hdrs)
{
  if (reliable_1xx == REL100_IGNORED)
    return;

  if (req.method == SIP_METH_INVITE) {
    if (100 < code && code < 200) {
      switch (reliable_1xx) {
        case REL100_SUPPORTED:
          if (! key_in_list(getHeader(hdrs, SIP_HDR_REQUIRE), SIP_EXT_100REL))
            hdrs += SIP_HDR_COLSP(SIP_HDR_SUPPORTED) SIP_EXT_100REL CRLF;
          break;
        case REL100_REQUIRE:
          // add Require HF
          if (! key_in_list(getHeader(hdrs, SIP_HDR_REQUIRE), SIP_EXT_100REL))
            hdrs += SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
          // add RSeq HF
          if (getHeader(hdrs, SIP_HDR_RSEQ).length())
            // already added (by app?)
            break;
          if (! rseq) { // only init rseq if 1xx is used
            rseq = (get_random() & 0x3ff) + 1; // start small (<1024) and non-0
            rseq_confirmed = false;
            rseq_1st = rseq;
          } else {
            if ((! rseq_confirmed) && (rseq_1st == rseq))
              // refuse subsequent 1xx if first isn't yet PRACKed
              throw AmSession::Exception(491, "first reliable 1xx not yet "
                  "PRACKed");
            rseq ++;
          }
          hdrs += SIP_HDR_COLSP(SIP_HDR_RSEQ) + int2str(rseq) + CRLF;
          break;
        default:
          break;
      }
    } else if (code < 300 && reliable_1xx == REL100_REQUIRE) { //code = 2xx
      if (rseq && !rseq_confirmed) 
        // reliable 1xx is pending, 2xx'ing not allowed yet
        throw AmSession::Exception(491, "last reliable 1xx not yet PRACKed");
    }
  }
}

/* static */
int AmSipDialog::reply_error(const AmSipRequest& req, unsigned int code, 
			     const string& reason, const string& hdrs,
			     const string& next_hop_ip,
			     unsigned short next_hop_port,
			     int outbound_interface)
{
  AmSipReply reply;

  reply.method = req.method;
  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  reply.hdrs = hdrs;
  reply.local_tag = AmSession::getNewId();

  if (AmConfig::Signature.length())
    reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;

  int ret = SipCtrlInterface::send(reply, next_hop_ip, next_hop_port, outbound_interface);
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),req.method.c_str(),req.callid.c_str(),req.cseq);
  }
  return ret;
}


int AmSipDialog::bye(const string& hdrs, int flags)
{
    switch(status){

    case Disconnecting:
    case Connected:
        for (TransMap::iterator it=uac_trans.begin();
	     it != uac_trans.end(); it++) {
	  if (it->second.method == "INVITE"){
	    // finish any UAC transaction before sending BYE
	    send_200_ack(it->second);
	  }
	}
	if (AmConfig::WaitForByeTransaction)
	  status = Disconnecting;
	else
	  status = Disconnected;
	return sendRequest("BYE", "", "", hdrs, flags);

    case Pending:
	status = Disconnecting;
	if(getUACInvTransPending())
	    return cancel();
	else {
	    // missing AmSipRequest to be able
	    // to send the reply on behalf of the app.
	    DBG("ignoring bye() in Pending state: "
		"no UAC transaction to cancel.\n");
	    status = Disconnected;
	}
	return 0;
    default:
	if(getUACInvTransPending())
	    return cancel();
	else {
	    DBG("bye(): we are not connected "
		"(status=%i). do nothing!\n",status);
	}
	return 0;
    }	
}

int AmSipDialog::reinvite(const string& hdrs,  
			  const string& content_type,
			  const string& body,
			  int flags)
{
  switch(status){
  case Connected:
    return sendRequest("INVITE", content_type, body, hdrs, flags);
  case Disconnecting:
  case Pending:
    DBG("reinvite(): we are not yet connected."
	"(status=%i). do nothing!\n",status);

    return 0;
  default:
    DBG("reinvite(): we are not connected "
	"(status=%i). do nothing!\n",status);
    return 0;
  }	
}

int AmSipDialog::invite(const string& hdrs,  
			const string& content_type,
			const string& body)
{
  switch(status){
  case Disconnected: {
    int res = sendRequest("INVITE", content_type, body, hdrs);
    status = Pending;
    return res;
  }; break;

  case Disconnecting:
  case Connected:
  case Pending:
  default:
    DBG("invite(): we are already connected."
	"(status=%i). do nothing!\n",status);

    return 0;
  }	
}

int AmSipDialog::update(const string &cont_type, 
                        const string &body, 
                        const string &hdrs)
{
  switch(status){
  case Connected:
  case Pending:
    return sendRequest(SIP_METH_UPDATE, cont_type, body, hdrs);

  default:
    DBG("update(): dialog not connected (status=%i). do nothing!\n",status);
  }	
  return -1;
}

int AmSipDialog::refer(const string& refer_to,
		       int expires)
{
  switch(status){
  case Connected: {
    string hdrs = SIP_HDR_COLSP(SIP_HDR_REFER_TO) + refer_to + CRLF;
    if (expires>=0) 
      hdrs+= SIP_HDR_COLSP(SIP_HDR_EXPIRES) + int2str(expires) + CRLF;
    return sendRequest("REFER", "", "", hdrs);
  }
  case Disconnecting:
  case Pending:
    DBG("refer(): we are not yet connected."
	"(status=%i). do nothing!\n",status);

    return 0;
  default:
    DBG("refer(): we are not connected "
	"(status=%i). do nothing!\n",status);
    return 0;
  }	

}

int AmSipDialog::transfer(const string& target)
{
  if(status == Connected){

    status = Disconnecting;
		
    string      hdrs = "";
    AmSipDialog tmp_d(*this);
		
    tmp_d.route = "";
    tmp_d.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) 
      "<" + tmp_d.remote_uri + ">" CRLF;
    tmp_d.remote_uri = target;
		
    string r_set;
    if(!route.empty()){
			
      hdrs = PARAM_HDR ": " "Transfer-RR=\"" + route + "\""+CRLF;
    }
				
    int ret = tmp_d.sendRequest("REFER","","",hdrs);
    if(!ret){
      uac_trans.insert(tmp_d.uac_trans.begin(),
		       tmp_d.uac_trans.end());
      cseq = tmp_d.cseq;
    }
		
    return ret;
  }
	
  DBG("transfer(): we are not connected "
      "(status=%i). do nothing!\n",status);
    
  return 0;
}

int AmSipDialog::prack(const AmSipReply &reply1xx,
                       const string &cont_type, 
                       const string &body, 
                       const string &hdrs)
{
  switch(status) {
    case Pending:
    case Connected:
      break;
    case Disconnected:
    case Disconnecting:
      ERROR("can not send PRACK while dialog is in state '%d'.\n", status);
      return -1;
    default:
      ERROR("BUG: unexpected dialog state '%d'.\n", status);
      return -1;
  }
  string h = hdrs +
          SIP_HDR_COLSP(SIP_HDR_RACK) + 
          int2str(reply1xx.rseq) + " " + 
          int2str(reply1xx.cseq) + " " + 
          reply1xx.method + CRLF;
  return sendRequest(SIP_METH_PRACK, cont_type, body, h);
}

int AmSipDialog::cancel()
{
    for(TransMap::reverse_iterator t = uac_trans.rbegin();
	t != uac_trans.rend(); t++) {
	
	if(t->second.method == "INVITE"){
	  
	  return SipCtrlInterface::cancel(&t->second.tt);
	}
    }
    
    ERROR("could not find INVITE transaction to cancel\n");
    return -1;
}

int AmSipDialog::sendRequest(const string& method, 
			     const string& content_type,
			     const string& body,
			     const string& hdrs,
			     int flags)
{
  string msg,ser_cmd;
  string m_hdrs = hdrs;

  if(hdl)
    hdl->onSendRequest(method,content_type,body,m_hdrs,flags,cseq);

  rel100OnRequestOut(method, m_hdrs);

  AmSipRequest req;

  req.method = method;
  req.r_uri = remote_uri;

  req.from = SIP_HDR_COLSP(SIP_HDR_FROM) + local_party;
  if(!local_tag.empty())
    req.from += ";tag=" + local_tag;
    
  req.to = SIP_HDR_COLSP(SIP_HDR_TO) + remote_party;
  if(!remote_tag.empty()) 
    req.to += ";tag=" + remote_tag;
    
  req.cseq = cseq;
  req.callid = callid;
    
  if((method!="BYE")&&(method!="CANCEL"))
    req.contact = getContactHdr();
    
  if(!m_hdrs.empty())
    req.hdrs = m_hdrs;
  
  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + int2str(AmConfig::MaxForwards) + CRLF;

  }

  req.route = getRoute();

  if(!body.empty()) {
    req.content_type = content_type;
    req.body = body;
  }

  if (SipCtrlInterface::send(req, next_hop_ip, next_hop_port,outbound_interface))
    return -1;
 
  uac_trans[cseq] = AmSipTransaction(method,cseq,req.tt);

  // increment for next request
  cseq++;

  return 0;
}

void AmSipDialog::rel100OnRequestOut(const string &method, string &hdrs)
{
  if (reliable_1xx == REL100_IGNORED || method!=SIP_METH_INVITE) // && method!=SIP_METH_OPTIONS)
    return;

  switch(reliable_1xx) {
    case REL100_SUPPORTED:
      if (! key_in_list(getHeader(hdrs, SIP_HDR_REQUIRE), SIP_EXT_100REL))
        hdrs += SIP_HDR_COLSP(SIP_HDR_SUPPORTED) SIP_EXT_100REL CRLF;
      break;
    case REL100_REQUIRE:
      if (! key_in_list(getHeader(hdrs, SIP_HDR_REQUIRE), SIP_EXT_100REL))
        hdrs += SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
      break;
    default:
      ERROR("BUG: unexpected reliability switch value of '%d'.\n",
          reliable_1xx);
    case 0:
      break;
  }
}


string AmSipDialog::get_uac_trans_method(unsigned int cseq)
{
  TransMap::iterator t = uac_trans.find(cseq);

  if (t != uac_trans.end())
    return t->second.method;

  return "";
}

AmSipTransaction* AmSipDialog::get_uac_trans(unsigned int cseq)
{
    TransMap::iterator t = uac_trans.find(cseq);
    
    if (t != uac_trans.end())
	return &(t->second);
    
    return NULL;
}

int AmSipDialog::drop()
{	
  status = Disconnected;
  return 1;
}

int AmSipDialog::send_200_ack(const AmSipTransaction& t,
			      const string& content_type,
			      const string& body,
			      const string& hdrs,
			      int flags)
{
  // TODO: implement missing pieces from RFC 3261:
  // "The ACK MUST contain the same credentials as the INVITE.  If
  // the 2xx contains an offer (based on the rules above), the ACK MUST
  // carry an answer in its body.  If the offer in the 2xx response is not
  // acceptable, the UAC core MUST generate a valid answer in the ACK and
  // then send a BYE immediately."

  string m_hdrs = hdrs;

  if(hdl)
    hdl->onSendRequest("ACK",content_type,body,m_hdrs,flags,t.cseq);

  AmSipRequest req;

  req.method = "ACK";
  req.r_uri = remote_uri;

  req.from = SIP_HDR_COLSP(SIP_HDR_FROM) + local_party;
  if(!local_tag.empty())
    req.from += ";tag=" + local_tag;
    
  req.to = SIP_HDR_COLSP(SIP_HDR_TO) + remote_party;
  if(!remote_tag.empty()) 
    req.to += ";tag=" + remote_tag;
    
  req.cseq = t.cseq;// should be the same as the INVITE
  req.callid = callid;
  req.contact = getContactHdr();
    
  if(!m_hdrs.empty())
    req.hdrs = m_hdrs;
  
  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + int2str(AmConfig::MaxForwards) + CRLF;
  }

  req.route = getRoute();

  if(!body.empty()) {
    req.content_type = content_type;
    req.body = body;
  }

  if (SipCtrlInterface::send(req, next_hop_ip, next_hop_port, outbound_interface))
    return -1;

  uac_trans.erase(t.cseq);

  return 0;
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
