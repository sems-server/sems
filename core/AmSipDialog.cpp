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
#include "sip/parse_next_hop.h"

#include "AmB2BMedia.h" // just because of statistics

static void addTranscoderStats(string &hdrs)
{
  // add transcoder statistics into request/reply headers
  if (!AmConfig::TranscoderOutStatsHdr.empty()) {
    string usage;
    B2BMediaStatistics::instance()->reportCodecWriteUsage(usage);

    hdrs += AmConfig::TranscoderOutStatsHdr + ": ";
    hdrs += usage;
    hdrs += CRLF;
  }
  if (!AmConfig::TranscoderInStatsHdr.empty()) {
    string usage;
    B2BMediaStatistics::instance()->reportCodecReadUsage(usage);

    hdrs += AmConfig::TranscoderInStatsHdr + ": ";
    hdrs += usage;
    hdrs += CRLF;
  }
}

const char* __dlg_status2str[AmSipDialog::__max_Status]  = {
  "Disconnected",
  "Trying",
  "Proceeding",
  "Cancelling",
  "Early",
  "Connected",
  "Disconnecting"
};

const char* dlgStatusStr(AmSipDialog::Status st)
{
  if((st < 0) || (st >= AmSipDialog::__max_Status))
    return "Invalid";
  else
    return __dlg_status2str[st];
}

const char* AmSipDialog::getStatusStr()
{
  return dlgStatusStr(status);
}

AmSipDialog::AmSipDialog(AmSipDialogEventHandler* h)
  : status(Disconnected),oa(this),rel100(this,h),
    offeranswer_enabled(true),
    early_session_started(false),session_started(false),
    cseq(10),r_cseq_i(false),hdl(h),
    pending_invites(0),cancel_pending(false),
    outbound_proxy(AmConfig::OutboundProxy),
    force_outbound_proxy(AmConfig::ForceOutboundProxy),
    next_hop(AmConfig::NextHop),
    outbound_interface(-1),
    sdp_local(), sdp_remote()
{
  assert(h);
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

void AmSipDialog::setStatus(Status new_status) {
  DBG("setting SIP dialog status: %s->%s\n",
      getStatusStr(), dlgStatusStr(new_status));

  status = new_status;
}

void AmSipDialog::onRxRequest(const AmSipRequest& req)
{
  DBG("AmSipDialog::onRxRequest(req = %s)\n", req.method.c_str());

  if ((req.method != SIP_METH_ACK) && (req.method != SIP_METH_CANCEL)) {

    // Sanity checks
    if (r_cseq_i && req.cseq <= r_cseq){
      string hdrs; bool i = false;
      if (req.method == SIP_METH_NOTIFY) {
	if (AmConfig::IgnoreNotifyLowerCSeq)
	  i = true;
	else
	  // clever trick to not break subscription dialog usage
	  // for implementations which follow 3265 instead of 5057
	  hdrs = SIP_HDR_COLSP(SIP_HDR_RETRY_AFTER)  "0"  CRLF;
      }

      if (!i) {
	INFO("remote cseq lower than previous ones - refusing request\n");
	// see 12.2.2
	reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR, hdrs);
	return;
      }
    }

    if (req.method == SIP_METH_INVITE) {
      bool pending = pending_invites;
      if (offeranswer_enabled) {
	  // not sure this is needed here: could be in AmOfferAnswer as well
	pending |= ((oa.getState() != AmOfferAnswer::OA_None) &&
		    (oa.getState() != AmOfferAnswer::OA_Completed));
      }

      if (pending) {
	reply_error(req, 491, SIP_REPLY_PENDING,
		    SIP_HDR_COLSP(SIP_HDR_RETRY_AFTER) 
		    + int2str(get_random() % 10) + CRLF);
	return;
      }

      pending_invites++;
    }
    
    r_cseq = req.cseq;
    r_cseq_i = true;
    uas_trans[req.cseq] = AmSipTransaction(req.method,req.cseq,req.tt);
    
    // target refresh requests
    if (req.from_uri.length() && 
	(req.method == SIP_METH_INVITE || 
	 req.method == SIP_METH_UPDATE ||
	 req.method == SIP_METH_SUBSCRIBE ||
	 req.method == SIP_METH_NOTIFY)) {

      // refresh the target
      remote_uri = req.from_uri;
    }

    // Dlg not yet initialized?
    if(callid.empty()){
      callid       = req.callid;
      remote_tag   = req.from_tag;
      user         = req.user;
      domain       = req.domain;
      local_uri    = req.r_uri;
      remote_party = req.from;
      local_party  = req.to;
      route        = req.route;
      outbound_interface = req.local_if;
    }
  }

  switch(status){
  case Disconnected:
    if(req.method == SIP_METH_INVITE)
      status = Trying;
    break;
  case Connected:
    if(req.method == SIP_METH_BYE)
      status = Disconnecting;
    break;

  case Trying:
  case Proceeding:
  case Early:
    if(req.method == SIP_METH_BYE)
      status = Disconnecting;
    else if(req.method == SIP_METH_CANCEL){
      status = Cancelling;
      reply(req,200,"OK");
    }
    break;

  default: break;
  }

  if (offeranswer_enabled) {
    oa.onRequestIn(req);
  }

  if(rel100.onRequestIn(req) && hdl)
    hdl->onSipRequest(req);
}

int AmSipDialog::onSdpCompleted()
{
  int ret = hdl->onSdpCompleted(oa.getLocalSdp(), oa.getRemoteSdp());
  if(!ret) {
    sdp_local = oa.getLocalSdp();
    sdp_remote = oa.getRemoteSdp();

    if((getStatus() == Early) && !early_session_started) {
      hdl->onEarlySessionStart();
      early_session_started = true;
    }

    if((getStatus() == Connected) && !session_started) {
      hdl->onSessionStart();
      session_started = true;
    }
  }
  else {
    oa.clear();
  }

  return ret;
}

bool AmSipDialog::getSdpOffer(AmSdp& offer)
{
  return hdl->getSdpOffer(offer);
}

bool AmSipDialog::getSdpAnswer(const AmSdp& offer, AmSdp& answer)
{
  return hdl->getSdpAnswer(offer,answer);
}

AmOfferAnswer::OAState AmSipDialog::getOAState() {
  return oa.getState();
}

void AmSipDialog::setOAState(AmOfferAnswer::OAState n_st) {
  oa.setState(n_st);
}

void AmSipDialog::setRel100State(Am100rel::State rel100_state) {
  DBG("setting 100rel state for '%s' to %i\n", local_tag.c_str(), rel100_state);
  rel100.setState(rel100_state);
}

void AmSipDialog::setOAEnabled(bool oa_enabled) {
  DBG("%sabling offer_answer on SIP dialog '%s'\n",
      oa_enabled?"en":"dis", local_tag.c_str());
  offeranswer_enabled = oa_enabled;
}

/**
 * Update dialog status from UAC Request that we send (e.g. INVITE)
 * (called only from AmSessionContainer)
 */
void AmSipDialog::initFromLocalRequest(const AmSipRequest& req)
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

// UAC behavior for locally sent requests
// (called from AmSipDialog::sendRequest())
int AmSipDialog::onTxRequest(AmSipRequest& req)
{
  if((req.method == SIP_METH_INVITE) && (status == Disconnected)){
    status = Trying;
  }
  else if((req.method == SIP_METH_BYE) && (status != Disconnecting)){
    status = Disconnecting;
  }

  return 0;
}

// UAS behavior for locally sent replies
int AmSipDialog::onTxReply(AmSipReply& reply)
{
  // update Dialog status
  switch(status){

  case Connected:
  case Disconnected:
    break;

  case Cancelling:
    if( (reply.cseq_method == "INVITE") &&
	(reply.code < 200) ) {
      // refuse local provisional replies 
      // when state is Cancelling
      ERROR("refuse local provisional replies when state is Cancelling\n");
      return -1;
    }
    // else continue with final
    // reply processing
  case Proceeding:
  case Trying:
  case Early:
    if(reply.cseq_method == "INVITE"){
      if(reply.code < 200) {
	status = Early;
      }
      else if(reply.code < 300)
	status = Connected;
      else
	status = Disconnected;
    }
    break;

  case Disconnecting:
    if(reply.cseq_method == "BYE"){

      // Only reason for refusing a BYE: 
      //  authentication (NYI at this place)
      // Also: we should not send provisionnal replies to a BYE
      if(reply.code >= 200)
	status = Disconnected;
    }
    break;

  default:
    assert(0);
    break;
  }

  return 0;
}
  
void AmSipDialog::onRxReply(const AmSipReply& reply)
{
  TransMap::iterator t_it = uac_trans.find(reply.cseq);
  if(t_it == uac_trans.end()){
    ERROR("could not find any transaction matching reply: %s\n", 
        ((AmSipReply)reply).print().c_str());
    return;
  }

  DBG("onRxReply(rep = %u %s): transaction found!\n",
      reply.code, reply.reason.c_str());

  string trans_method = t_it->second.method;

  Status saved_status = status;

  // rfc3261 12.1
  // Dialog established only by 101-199 or 2xx 
  // responses to INVITE

  if(reply.cseq_method == SIP_METH_INVITE) {

    switch(status){

    case Trying:
    case Proceeding:
      if(reply.code < 200){
	if(reply.to_tag.empty())
	  status = Proceeding;
	else {
	  status = Early;
	  remote_tag = reply.to_tag;
	  route = reply.route;
	  if(!reply.to_uri.empty())
	    remote_uri = reply.to_uri;
	}
      }
      else if(reply.code < 300){
	status = Connected;
	route = reply.route;
	remote_uri = reply.to_uri;

	if(reply.to_tag.empty()){
	  DBG("received 2xx reply without to-tag "
	      "(callid=%s): sending BYE\n",reply.callid.c_str());

	  sendRequest(SIP_METH_BYE);
	}
	else {
	  remote_tag = reply.to_tag;
	}
      }

      if(reply.code >= 300) {// error reply
	status = Disconnected;
      }
      else if(cancel_pending){
	cancel_pending = false;
	bye();
      }
      break;

    case Early:
      if(reply.code < 200){
      }
      else if(reply.code < 300){
	status = Connected;
	route = reply.route;
	remote_uri = reply.to_uri;

	if(reply.to_tag.empty()){
	  DBG("received 2xx reply without to-tag "
	      "(callid=%s): sending BYE\n",reply.callid.c_str());

	  sendRequest(SIP_METH_BYE);
	}
	else {
	  remote_tag = reply.to_tag;
	}
      }
      else { // error reply
	status = Disconnected;
      }
      break;

    case Cancelling:
      if(reply.code >= 300){
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
      break;

    default:
      break;
    }
  }

  if(status == Disconnecting){
    DBG("?Disconnecting?: cseq_method = %s; code = %i\n",reply.cseq_method.c_str(), reply.code);
    if((reply.cseq_method == "BYE") && (reply.code >= 200)){
      //TODO: support the auth case here (401/403)
      status = Disconnected;
    }
  }

  if (offeranswer_enabled) {
    oa.onReplyIn(reply);
  }

  if(rel100.onReplyIn(reply) && hdl)
    hdl->onSipReply(reply, saved_status);

  if(reply.code >= 200){
    if((reply.code < 300) && (trans_method == SIP_METH_INVITE)) {

	if(hdl) {
	    hdl->onInvite2xx(reply);
	}
	else {
	    send_200_ack(reply.cseq);
	}
    } else {
      // error reply or method != INVITE
      uac_trans.erase(t_it);

      if ((reply.code == 408 || reply.code == 481) && (status == Connected)) {
	hdl->onRemoteDisappeared(reply);
      }
    }
  }
}


void AmSipDialog::uasTimeout(AmSipTimeoutEvent* to_ev)
{
  assert(to_ev);

  switch(to_ev->type){
  case AmSipTimeoutEvent::noACK:
    DBG("Timeout: missing ACK\n");
    if (offeranswer_enabled) {
      oa.onNoAck(to_ev->cseq);
    }
    if(hdl) hdl->onNoAck(to_ev->cseq);
    break;

  case AmSipTimeoutEvent::noPRACK:
    DBG("Timeout: missing PRACK\n");
    rel100.onTimeout(to_ev->req, to_ev->rpl);
    break;

  case AmSipTimeoutEvent::_noEv:
  default:
    break;
  };
  
  to_ev->processed = true;
}

AmSipTransaction* AmSipDialog::getUACTrans(unsigned int t_cseq)
{
  TransMap::iterator it = uac_trans.find(t_cseq);
  if(it == uac_trans.end())
    return NULL;
  
  return &(it->second);
}

bool AmSipDialog::getUACTransPending() {
  return !uac_trans.empty();
}

bool AmSipDialog::getUACInvTransPending() {
  for (TransMap::iterator it=uac_trans.begin();
       it != uac_trans.end(); it++) {
    if (it->second.method == SIP_METH_INVITE)
      return true;
  }
  return false;
}

AmSipTransaction* AmSipDialog::getUASTrans(unsigned int t_cseq)
{
  TransMap::iterator it = uas_trans.find(t_cseq);
  if(it == uas_trans.end())
    return NULL;
  
  return &(it->second);
}

AmSipTransaction* AmSipDialog::getPendingUASInv()
{
  for (TransMap::iterator it=uas_trans.begin();
       it != uas_trans.end(); it++) {
    if (it->second.method == SIP_METH_INVITE)
      return &(it->second);
  }
  return NULL;
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

    contact_uri += AmConfig::Ifs[oif].PublicIP.empty() ? 
		   AmConfig::Ifs[oif].LocalSIPIP : AmConfig::Ifs[oif].PublicIP;
    contact_uri += ":" + int2str(AmConfig::Ifs[oif].LocalSIPPort);
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
  // 1. next_hop
  // 2. outbound_proxy (if 1st req or force_outbound_proxy)
  // 3. first route
  // 4. remote URI
  
  string dest_uri;
  string dest_ip;
  string local_ip;
  multimap<string,unsigned short>::iterator if_it;

  list<host_port> ip_list;
  if(!next_hop.empty() && 
     !parse_next_hop(stl2cstr(next_hop),ip_list) &&
     !ip_list.empty()) {

    dest_ip = c2stlstr(ip_list.front().host);
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
		       const AmMimeBody* body,
		       const string& hdrs,
		       int flags)
{
  return reply(AmSipTransaction(req.method,req.cseq,req.tt),
	       code,reason,body,hdrs,flags);
}

int AmSipDialog::reply(const AmSipTransaction& t,
		       unsigned int  code,
		       const string& reason,
		       const AmMimeBody* body,
		       const string& hdrs,
		       int flags)
{
  TransMap::const_iterator t_it = uas_trans.find(t.cseq);
  if(t_it == uas_trans.end()){
    ERROR("could not find any transaction matching request cseq\n");
    ERROR("request cseq=%i; reply code=%i; callid=%s; local_tag=%s; "
	  "remote_tag=%s\n",
	  t.cseq,code,callid.c_str(),
	  local_tag.c_str(),remote_tag.c_str());
    return -1;
  }
  DBG("reply: transaction found!\n");
    
  string m_hdrs = hdrs;
  AmSipReply reply;

  reply.code = code;
  reply.reason = reason;
  reply.tt = t.tt;
  reply.to_tag = local_tag;
  reply.hdrs = m_hdrs;
  reply.cseq = t.cseq;
  reply.cseq_method = t.method;

  if(body != NULL)
    reply.body = *body;

  if (!flags&SIP_FLAGS_VERBATIM) {
    // add Signature
    if (AmConfig::Signature.length())
      reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;
  }

  addTranscoderStats(reply.hdrs); // add transcoder statistics into reply headers

  if (code < 300 && t.method != "CANCEL" && t.method != "BYE"){
    /* if 300<=code<400, explicit contact setting should be done */
    reply.contact = getContactHdr();
  }

  if (offeranswer_enabled) {
    oa.onReplyOut(reply);
  }

  rel100.onReplyOut(reply);
  hdl->onSendReply(reply,flags);

  if(onTxReply(reply)){
    DBG("onTxReply failed\n");
    return -1;
  }

  int ret = SipCtrlInterface::send(reply);
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),reply.cseq_method.c_str(),
	  reply.callid.c_str(),reply.cseq);

    return ret;
  }

  if ((reply.code >= 200) && 
      (reply.cseq_method != SIP_METH_CANCEL)) {
    
    if(reply.cseq_method == SIP_METH_INVITE) 
      pending_invites--;
    
    uas_trans.erase(reply.cseq);
  }

  if (offeranswer_enabled) {
    return oa.onReplySent(reply);
  }

  return ret;
}


/* static */
int AmSipDialog::reply_error(const AmSipRequest& req, unsigned int code, 
			     const string& reason, const string& hdrs)
{
  AmSipReply reply;

  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  reply.hdrs = hdrs;
  reply.to_tag = AmSession::getNewId();

  if (AmConfig::Signature.length())
    reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;

  addTranscoderStats(reply.hdrs); // add transcoder statistics into reply headers

  int ret = SipCtrlInterface::send(reply);
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
    case Connected: {
      // collect INVITE UAC transactions
      vector<unsigned int> ack_trans;
      for (TransMap::iterator it=uac_trans.begin(); it != uac_trans.end(); it++) {
	if (it->second.method == "INVITE"){
	  ack_trans.push_back(it->second.cseq);
	}
      }
      // finish any UAC transaction before sending BYE
      for (vector<unsigned int>::iterator it=
	     ack_trans.begin(); it != ack_trans.end(); it++) {
	send_200_ack(*it);
      }

      if (status != Disconnecting) {
	status = Disconnected;
	return sendRequest("BYE", NULL, hdrs, flags);
      } else {
	return 0;
      }
    }

    case Trying:
    case Proceeding:
    case Early:
    case Cancelling:
	if(getUACInvTransPending())
	    return cancel();
	else {  
	    for (TransMap::iterator it=uas_trans.begin();
		 it != uas_trans.end(); it++) {
	      if (it->second.method == "INVITE"){
		// let quit this call by sending final reply
		return reply(it->second,
			     487,"Request terminated");
	      }
	    }

	    // missing AmSipRequest to be able
	    // to send the reply on behalf of the app.
	    ERROR("ignoring bye() in %s state: "
		  "no UAC transaction to cancel.\n",
		  getStatusStr());
	    status = Disconnected;
	}
	return 0;

    default:
        DBG("bye(): we are not connected "
	    "(status=%s). do nothing!\n",getStatusStr());
	return 0;
    }	
}

int AmSipDialog::reinvite(const string& hdrs,  
			  const AmMimeBody* body,
			  int flags)
{
  if(status == Connected) {
    return sendRequest("INVITE", body, hdrs, flags);
  }
  else {
    DBG("reinvite(): we are not connected "
	"(status=%s). do nothing!\n",getStatusStr());
  }

  return -1;
}

int AmSipDialog::invite(const string& hdrs,  
			const AmMimeBody* body)
{
  if(status == Disconnected) {
    int res = sendRequest("INVITE", body, hdrs);
    DBG("TODO: is status already 'trying'? status=%s\n",getStatusStr());
    //status = Trying;
    return res;
  }
  else {
    DBG("invite(): we are already connected "
	"(status=%s). do nothing!\n",getStatusStr());
  }

  return -1;
}

int AmSipDialog::update(const AmMimeBody* body, 
                        const string &hdrs)
{
  switch(status){
  case Connected://if Connected, we should send a re-INVITE instead...
    DBG("re-INVITE should be used instead (see RFC3311, section 5.1)\n");
  case Trying:
  case Proceeding:
  case Early:
    return sendRequest(SIP_METH_UPDATE, body, hdrs);

  default:
  case Cancelling:
  case Disconnected:
  case Disconnecting:
    DBG("update(): dialog not connected "
	"(status=%s). do nothing!\n",getStatusStr());
  }

  return -1;
}

int AmSipDialog::refer(const string& refer_to,
		       int expires)
{
  if(status == Connected) {
    string hdrs = SIP_HDR_COLSP(SIP_HDR_REFER_TO) + refer_to + CRLF;
    if (expires>=0) 
      hdrs+= SIP_HDR_COLSP(SIP_HDR_EXPIRES) + int2str(expires) + CRLF;
    return sendRequest("REFER", NULL, hdrs);
  }
  else {
    DBG("refer(): we are not Connected."
	"(status=%s). do nothing!\n",getStatusStr());

    return 0;
  }	
}

// proprietary
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
				
    int ret = tmp_d.sendRequest("REFER",NULL,hdrs);
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
                       const AmMimeBody* body, 
                       const string &hdrs)
{
  switch(status) {
  case Trying:
  case Proceeding:
  case Cancelling:
  case Early:
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
          reply1xx.cseq_method + CRLF;
  return sendRequest(SIP_METH_PRACK, body, h);
}

int AmSipDialog::cancel()
{
    for(TransMap::reverse_iterator t = uac_trans.rbegin();
	t != uac_trans.rend(); t++) {
	
	if(t->second.method == SIP_METH_INVITE){
	  
	  if(status == Trying){
	    cancel_pending=true;
	    return 0;
	  }
	  else if(status != Cancelling){
	    status = Cancelling;
	    return SipCtrlInterface::cancel(&t->second.tt);
	  }
	  else {
	    ERROR("INVITE transaction has already been cancelled\n");
	    return -1;
	  }
	}
    }
    
    ERROR("could not find INVITE transaction to cancel\n");
    return -1;
}

int AmSipDialog::sendRequest(const string& method, 
			     const AmMimeBody* body,
			     const string& hdrs,
			     int flags)
{
  int ret = sendRequest(method,body,hdrs,flags,cseq);
  if (ret < 0)
    return ret;

  cseq++;
  return 0;
}




int AmSipDialog::sendRequest(const string& method, 
			     const AmMimeBody* body,
			     const string& hdrs,
			     int flags,
			     unsigned int req_cseq)
{
  string msg,ser_cmd;
  AmSipRequest req;

  req.method = method;
  req.r_uri = remote_uri;

  req.from = SIP_HDR_COLSP(SIP_HDR_FROM) + local_party;
  if(!local_tag.empty())
    req.from += ";tag=" + local_tag;
    
  req.to = SIP_HDR_COLSP(SIP_HDR_TO) + remote_party;
  if(!remote_tag.empty()) 
    req.to += ";tag=" + remote_tag;
    
  req.cseq = req_cseq;
  req.callid = callid;
    
  if((method!=SIP_METH_BYE)&&(method!=SIP_METH_CANCEL))
    req.contact = getContactHdr();
    
  req.hdrs = hdrs;

  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + int2str(AmConfig::MaxForwards) + CRLF;

  }

  addTranscoderStats(req.hdrs); // add transcoder statistics into request headers

  req.route = getRoute();

  if(body != NULL) {
    req.body = *body;
    //DBG("req.body = '%s'\n", req.body.c_str());
  }

  if (offeranswer_enabled && oa.onRequestOut(req))
    return -1;

  rel100.onRequestOut(req);
  if(hdl)
    hdl->onSendRequest(req,flags);

  onTxRequest(req);
  int res = SipCtrlInterface::send(req, next_hop, outbound_interface);
  if(res) {
    ERROR("Could not send request: method=%s; call-id=%s; cseq=%i\n",
	  req.method.c_str(),req.callid.c_str(),req.cseq);
    return res;
  }
 
  if(method != SIP_METH_ACK) {
    uac_trans[req_cseq] = AmSipTransaction(method,req_cseq,req.tt);
  }
  else {
    // probably never executed, as send_200_ack is used instead of sendRequest
    // note: non-200 ACKs are sent from the transaction layer
    uac_trans.erase(req_cseq);
  }

  if (offeranswer_enabled) {
    return oa.onRequestSent(req);
  }

  return 0;
}

int AmSipDialog::drop()
{	
  status = Disconnected;
  return 1;
}

int AmSipDialog::send_200_ack(unsigned int inv_cseq,
			      const AmMimeBody* body,
			      const string& hdrs,
			      int flags)
{
  // TODO: implement missing pieces from RFC 3261:
  // "The ACK MUST contain the same credentials as the INVITE.  If
  // the 2xx contains an offer (based on the rules above), the ACK MUST
  // carry an answer in its body.  If the offer in the 2xx response is not
  // acceptable, the UAC core MUST generate a valid answer in the ACK and
  // then send a BYE immediately."

  if (uac_trans.find(inv_cseq) == uac_trans.end()) {
    ERROR("trying to ACK a non-existing transaction (cseq=%i;local_tag=%s)\n",inv_cseq,local_tag.c_str());
    return -1;
  }

  AmSipRequest req;

  req.method = SIP_METH_ACK;
  req.r_uri = remote_uri;

  req.from = SIP_HDR_COLSP(SIP_HDR_FROM) + local_party;
  if(!local_tag.empty())
    req.from += ";tag=" + local_tag;
    
  req.to = SIP_HDR_COLSP(SIP_HDR_TO) + remote_party;
  if(!remote_tag.empty()) 
    req.to += ";tag=" + remote_tag;
    
  req.cseq = inv_cseq;// should be the same as the INVITE
  req.callid = callid;
  req.contact = getContactHdr();
    
  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + int2str(AmConfig::MaxForwards) + CRLF;
  }

  req.route = getRoute();

  if(body != NULL)
    req.body = *body;

  if (offeranswer_enabled && oa.onRequestOut(req))
    return -1;

  if(hdl)
    hdl->onSendRequest(req,flags);

  //onTxRequest(req); // not needed right now in the ACK case
  int res = SipCtrlInterface::send(req, next_hop, outbound_interface);
  if (res)
    return res;

  uac_trans.erase(inv_cseq);
  if (offeranswer_enabled) {
    return oa.onRequestSent(req);
  }

  return 0;
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
