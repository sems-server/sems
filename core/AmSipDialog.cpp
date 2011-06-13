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

const char* __dlg_oa_status2str[AmSipDialog::__max_OA]  = {
    "None",
    "OfferRecved",
    "OfferSent",
    "Completed"
};

const char* getOAStatusStr(AmSipDialog::OAState st) {
  if((st < 0) || (st >= AmSipDialog::__max_OA))
    return "Invalid";
  else
    return __dlg_oa_status2str[st];
}

AmSipDialog::AmSipDialog(AmSipDialogEventHandler* h)
  : status(Disconnected),oa_state(OA_None),
    cseq(10),r_cseq_i(false),hdl(h),
    pending_invites(0),cancel_pending(false),
    outbound_proxy(AmConfig::OutboundProxy),
    force_outbound_proxy(AmConfig::ForceOutboundProxy),
    reliable_1xx(AmConfig::rel100),
    rseq(0), rseq_1st(0), rseq_confirmed(false),
    next_hop_port(AmConfig::NextHopPort),
    next_hop_ip(AmConfig::NextHopIP),
    next_hop_for_replies(AmConfig::NextHopForReplies),
    outbound_interface(-1), out_intf_for_replies(false)
{
  assert(h);
  if (reliable_1xx)
    rseq = 0;
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

AmSipDialog::OAState AmSipDialog::get_OA_state() {
  return oa_state;
}

void AmSipDialog::set_OA_state(OAState new_oa_state) {
  DBG("setting SIP dialog O/A status: %s->%s\n",
      getOAStatusStr(oa_state), getOAStatusStr(new_oa_state));
  oa_state = new_oa_state;
}

void AmSipDialog::onRxRequest(const AmSipRequest& req)
{
  DBG("AmSipDialog::onRxRequest(req = %s)\n", req.method.c_str());

  if ((req.method != SIP_METH_ACK) && (req.method != SIP_METH_CANCEL)) {

    // Sanity checks
    if (r_cseq_i && req.cseq <= r_cseq){
      INFO("remote cseq lower than previous ones - refusing request\n");
      // see 12.2.2
      reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR, "",
		  next_hop_for_replies ? next_hop_ip : "",
		  next_hop_for_replies ? next_hop_port : 0);
      return;
    }

    if (req.method == SIP_METH_INVITE) {
      if(pending_invites || ((oa_state != OA_None) && (oa_state != OA_Completed))) {      
	reply_error(req,500, SIP_REPLY_SERVER_INTERNAL_ERROR,
		    "Retry-After: " + int2str(get_random() % 10) + CRLF,
		    next_hop_for_replies ? next_hop_ip : "",
		    next_hop_for_replies ? next_hop_port : 0);
	return;
      }
      pending_invites++;

      // Reset Offer/Answer state
      oa_state = OA_None;
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

  if((req.method == SIP_METH_INVITE || 
      req.method == SIP_METH_UPDATE || 
      req.method == SIP_METH_ACK ||
      req.method == SIP_METH_PRACK) &&
     !req.body.empty() && 
     (req.content_type == SIP_APPLICATION_SDP)) {

    const char* err_txt=NULL;
    int err_code = onRxSdp(req.body,&err_txt);
    if(err_code){
      if( req.method != SIP_METH_ACK ){ // INVITE || UPDATE || PRACK
	reply(req,err_code,err_txt);
      }
      else { // ACK
	// TODO: only if reply to initial INVITE (if re-INV, app should decide)
	DBG("error %i with SDP received in ACK request: sending BYE\n",err_code);
	bye();
      }
    }
  }

  if(rel100OnRequestIn(req) && hdl)
    hdl->onSipRequest(req);
}

int AmSipDialog::onRxSdp(const string& body, const char** err_txt)
{
  DBG("entering onRxSdp()\n");

  int err_code = 0;
  assert(err_txt);

  if(sdp_remote.parse(body.c_str())){
    err_code = 400;
    *err_txt = "session description parsing failed";
  }
  else if(sdp_remote.media.empty()){
    err_code = 400;
    *err_txt = "no media line found in SDP message";
  }

  if(err_code == 0) {

    switch(oa_state) {
    case OA_None:
    case OA_Completed:
      oa_state = OA_OfferRecved;
      break;
      
    case OA_OfferSent:
      oa_state = OA_Completed;
      if(hdl->onSdpCompleted(sdp_local, sdp_remote)){
	err_code = 500;
	*err_txt = "internal error";
      }
      break;

    case OA_OfferRecved:
      err_code = 400;// TODO: check correct error code
      *err_txt = "pending SDP offer";
      break;

    default:
      assert(0);
      break;
    }
  }

  if(err_code != 0) {
    oa_state = OA_None;
  }

  return err_code;
}

int AmSipDialog::onTxSdp(const string& body)
{
  // assume that the payload is ok if it is not empty.
  // (do not parse again self-generated SDP)
  if(body.empty()){
    return -1;
  }

  switch(oa_state) {

  case OA_None:
  case OA_Completed:
    oa_state = OA_OfferSent;
    break;

  case OA_OfferRecved:
    oa_state = OA_Completed;
    break;

  case OA_OfferSent:
    // There is already a pending offer!!!
    DBG("There is already a pending offer, onTxSdp fails\n");
    return -1;

  default:
    break;
  }

  return 0;
}

int AmSipDialog::getSdpBody(string& sdp_body)
{
    switch(oa_state){
    case OA_None:
    case OA_Completed:
      if(hdl->getSdpOffer(sdp_local)){
	sdp_local.print(sdp_body);
      }
      else {
	DBG("No SDP Offer.\n");
	return -1;
      }
      break;
    case OA_OfferRecved:
      if(hdl->getSdpAnswer(sdp_remote,sdp_local)){
	sdp_local.print(sdp_body);
      }
      else {
	DBG("No SDP Answer.\n");
	return -1;
      }
      break;
      
    case OA_OfferSent:
      DBG("Still waiting for a reply\n");
      return -1;

    default: 
      break;
    }

    return 0;
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

  bool generate_sdp = req.body.empty() 
    && (req.content_type == SIP_APPLICATION_SDP);

  bool has_sdp = !req.body.empty() 
    && (req.content_type == SIP_APPLICATION_SDP);

  if (!generate_sdp && !has_sdp && 
      ((req.method == SIP_METH_PRACK) ||
       (req.method == SIP_METH_ACK))) {
    generate_sdp = (oa_state == OA_OfferRecved);
  }

  if (generate_sdp) {
    if (!getSdpBody(req.body)){
      req.content_type = SIP_APPLICATION_SDP;
      has_sdp = true;
    }
  }

  if(has_sdp && (onTxSdp(req.body) != 0)){
    DBG("onTxSdp() failed\n");
    return -1;
  }

  return 0;
}

// UAS behavior for locally sent replies
int AmSipDialog::onTxReply(AmSipReply& reply)
{
  // update Dialog status
  Status status_backup = status;
  switch(status){

  case Connected:
  case Disconnected:
    break;

  case Cancelling:
    if( (reply.cseq_method == "INVITE") &&
	(reply.code < 200) ) {
      // refuse local provisional replies 
      // when state is Cancelling
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

  bool generate_sdp = reply.body.empty() 
    && (reply.content_type == SIP_APPLICATION_SDP);

  bool has_sdp = !reply.body.empty() 
    && (reply.content_type == SIP_APPLICATION_SDP);

  if (!has_sdp && !generate_sdp) {
    // let's see whether we should force SDP or not.

    if (reply.cseq_method == SIP_METH_INVITE){
      
      if ((reply.code == 183) 
	  || ((reply.code >= 200) && (reply.code < 300))) {
	
	// either offer received or no offer at all:
	//  -> force SDP
	generate_sdp = (oa_state == OA_OfferRecved) || (oa_state == OA_None);
      }
    }
    else if (reply.cseq_method == SIP_METH_UPDATE) {

      if ((reply.code >= 200) &&
	  (reply.code < 300)) {
	
	// offer received:
	//  -> force SDP
	generate_sdp = (oa_state == OA_OfferRecved);
      }
    }
  }
  
  if (generate_sdp) {
    if(!getSdpBody(reply.body)) {
      reply.content_type = SIP_APPLICATION_SDP;
      has_sdp = true;
    }
  }

  if (has_sdp && (onTxSdp(reply.body) != 0)) {
    
    DBG("onTxSdp() failed\n");
    status = status_backup;
    return -1;
  }

  if ((reply.code >= 200) && 
      ((reply.cseq_method != "CANCEL"))) {
    
    if(reply.cseq_method == "INVITE") 
      pending_invites--;
    
    uas_trans.erase(reply.cseq);
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

  AmSipDialog::Status old_dlg_status = status;
  string trans_method = t_it->second.method;

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

  if((reply.cseq_method == SIP_METH_INVITE || 
      reply.cseq_method == SIP_METH_UPDATE || 
      reply.cseq_method == SIP_METH_PRACK) &&
     !reply.body.empty() && 
     (reply.content_type == SIP_APPLICATION_SDP)) {

    const char* err_txt=NULL;
    int err_code = onRxSdp(reply.body,&err_txt);
    if(err_code){
      // TODO: only if initial INVITE (if re-INV, app should decide)
      DBG("error %i with SDP received in %i reply: sending ACK+BYE\n",err_code,reply.code);
      bye();
    }
  }

  int cont = rel100OnReplyIn(reply);
  if(reply.code >= 200){
    if((reply.code < 300) && (trans_method == SIP_METH_INVITE)) {

	if(hdl) {
	    hdl->onInvite2xx(reply);
	}
	else {
	    send_200_ack(reply.cseq);
	}
    }
    else {
      uac_trans.erase(t_it);
    }
  }

  if(cont && hdl)
    hdl->onSipReply(reply, old_dlg_status);
}


int AmSipDialog::rel100OnReplyIn(const AmSipReply &reply)
{
  if (reliable_1xx == REL100_IGNORED)
    return 1;

  if (status!=Trying && status!=Proceeding && status!=Early && status!=Connected)
    return 1;

  if (100<reply.code && reply.code<200 && reply.cseq_method==SIP_METH_INVITE) {
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
  } else if (reliable_1xx && reply.cseq_method==SIP_METH_PRACK) {
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
      rseq == rpl.rseq && rpl.cseq_method == SIP_METH_INVITE) {
    INFO("reliable %d reply timed out; rejecting request.\n", rpl.code);
    if(hdl) hdl->onNoPrack(req, rpl);
  } else {
    WARN("reply timed-out, but not reliable.\n"); // debugging
  }
}

AmSipTransaction* AmSipDialog::getUACTrans(unsigned int cseq)
{
  TransMap::iterator it = uac_trans.find(cseq);
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
  return reply(AmSipTransaction(req.method,req.cseq,req.tt),
	       code,reason,content_type,body,hdrs,flags);
}

int AmSipDialog::reply(const AmSipTransaction& t,
		       unsigned int  code,
		       const string& reason,
		       const string& content_type,
		       const string& body,
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
  //const AmSipTransaction& t = t_it->second;
  AmSipReply reply;

  reply.code = code;
  reply.reason = reason;
  reply.tt = t.tt;
  reply.to_tag = local_tag;
  reply.hdrs = m_hdrs;
  reply.cseq = t.cseq;
  reply.cseq_method = t.method;
  reply.content_type = content_type;
  reply.body = body;

  hdl->onSendReply(reply,flags);
  rel100OnReplyOut(reply);

  if (!flags&SIP_FLAGS_VERBATIM) {
    // add Signature
    if (AmConfig::Signature.length())
      reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;
  }

  if (code < 300 && t.method != "CANCEL" && t.method != "BYE"){
    /* if 300<=code<400, explicit contact setting should be done */
    reply.contact = getContactHdr();
  }


  OAState old_oa_state = oa_state;
  if(onTxReply(reply)){
    DBG("onTxReply failed\n");
    return -1;
  }

  int ret = SipCtrlInterface::send(reply, next_hop_for_replies ? next_hop_ip : "",
				   next_hop_for_replies ? next_hop_port : 0,
				   out_intf_for_replies ? outbound_interface : -1 );

  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),reply.cseq_method.c_str(),
	  reply.callid.c_str(),reply.cseq);
  }
  else {
    if((old_oa_state != oa_state) &&
       (oa_state == OA_Completed)) {
      return hdl->onSdpCompleted(sdp_local, sdp_remote);
    }
  }

  return ret;
}


void AmSipDialog::rel100OnReplyOut(AmSipReply& reply)
{
  if (reliable_1xx == REL100_IGNORED)
    return;

  if (reply.cseq_method == SIP_METH_INVITE) {
    if (100 < reply.code && reply.code < 200) {
      switch (reliable_1xx) {
        case REL100_SUPPORTED:
          if (! key_in_list(getHeader(reply.hdrs, SIP_HDR_REQUIRE), 
			    SIP_EXT_100REL))
            reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SUPPORTED) SIP_EXT_100REL CRLF;
          break;
        case REL100_REQUIRE:
          // add Require HF
          if (! key_in_list(getHeader(reply.hdrs, SIP_HDR_REQUIRE), 
			    SIP_EXT_100REL))
            reply.hdrs += SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
          // add RSeq HF
          if (getHeader(reply.hdrs, SIP_HDR_RSEQ).length())
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
          reply.hdrs += SIP_HDR_COLSP(SIP_HDR_RSEQ) + int2str(rseq) + CRLF;
          break;
        default:
          break;
      }
    } else if (reply.code < 300 && reliable_1xx == REL100_REQUIRE) { //code = 2xx
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

  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  reply.hdrs = hdrs;
  reply.to_tag = AmSession::getNewId();

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
	    send_200_ack(it->second.cseq);
	  }
	}
	status = Disconnected;
	return sendRequest("BYE", "", "", hdrs, flags);

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
	    "(status=%i). do nothing!\n",status);
	return 0;
    }	
}

int AmSipDialog::reinvite(const string& hdrs,  
			  const string& content_type,
			  const string& body,
			  int flags)
{
  if(status == Connected) {
    return sendRequest("INVITE", content_type, body, hdrs, flags);
  }
  else {
    DBG("reinvite(): we are not connected "
	"(status=%s). do nothing!\n",getStatusStr());
  }

  return -1;
}

int AmSipDialog::invite(const string& hdrs,  
			const string& content_type,
			const string& body)
{
  if(status == Disconnected) {
    int res = sendRequest("INVITE", content_type, body, hdrs);
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

int AmSipDialog::update(const string &cont_type, 
                        const string &body, 
                        const string &hdrs)
{
  switch(status){
  case Connected://if Connected, we should send a re-INVITE instead...
    DBG("re-INVITE should be used instead (see RFC3311, section 5.1)\n");
  case Trying:
  case Proceeding:
  case Early:
    return sendRequest(SIP_METH_UPDATE, cont_type, body, hdrs);

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
    return sendRequest("REFER", "", "", hdrs);
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
  case Trying:
  case Proceeding:
  case Cancelling:
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
  return sendRequest(SIP_METH_PRACK, cont_type, body, h);
}

int AmSipDialog::cancel()
{
    for(TransMap::reverse_iterator t = uac_trans.rbegin();
	t != uac_trans.rend(); t++) {
	
	if(t->second.method == "INVITE"){
	  
	  if(status == Trying){
	    cancel_pending=true;
	    return 0;
	  }
	  else {
	    status = Cancelling;
	    return SipCtrlInterface::cancel(&t->second.tt);
	  }
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
  int ret = sendRequest(method,content_type,body,hdrs,flags,cseq);
  if (ret < 0)
    return ret;

  cseq++;
  return 0;
}




int AmSipDialog::sendRequest(const string& method, 
			     const string& content_type,
			     const string& body,
			     const string& hdrs,
			     int flags,
			     unsigned int req_cseq)
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
    
  req.cseq = req_cseq;
  req.callid = callid;
    
  if((method!=SIP_METH_BYE)&&(method!=SIP_METH_CANCEL))
    req.contact = getContactHdr();
    
  if(!m_hdrs.empty())
    req.hdrs = m_hdrs;
  
  // if((method == "INVITE") && reliable_1xx){
  //   req.hdrs += get_100rel_hdr(reliable_1xx);
  // }

  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + int2str(AmConfig::MaxForwards) + CRLF;

  }

  req.route = getRoute();

  req.content_type = content_type;
  req.body = body;
  DBG("req.body = '%s'\n", req.body.c_str());
  OAState old_oa_state = oa_state;
  if(onTxRequest(req))
    return -1;

  if(SipCtrlInterface::send(req, next_hop_ip, next_hop_port,outbound_interface)) {
    ERROR("Could not send request: method=%s; call-id=%s; cseq=%i\n",
	  req.method.c_str(),req.callid.c_str(),req.cseq);
    return -1;
  }
 
  if(method != SIP_METH_ACK) {
    uac_trans[req_cseq] = AmSipTransaction(method,req_cseq,req.tt);
  }
  else {
    uac_trans.erase(req_cseq);
  }

  if((old_oa_state != oa_state) &&
     (oa_state == OA_Completed)) {
    return hdl->onSdpCompleted(sdp_local, sdp_remote);
  }

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


string AmSipDialog::get_uac_trans_method(unsigned int t_cseq)
{
  TransMap::iterator t = uac_trans.find(t_cseq);

  if (t != uac_trans.end())
    return t->second.method;

  return "";
}

AmSipTransaction* AmSipDialog::get_uac_trans(unsigned int t_cseq)
{
    TransMap::iterator t = uac_trans.find(t_cseq);
    
    if (t != uac_trans.end())
	return &(t->second);
    
    return NULL;
}

int AmSipDialog::drop()
{	
  status = Disconnected;
  return 1;
}

int AmSipDialog::send_200_ack(unsigned int inv_cseq,
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

  if (uac_trans.find(inv_cseq) == uac_trans.end()) {
    ERROR("trying to ACK a non-existing transaction (cseq=%i;local_tag=%s)\n",inv_cseq,local_tag.c_str());
    return -1;
  }

  string m_hdrs = hdrs;

  if(hdl)
    hdl->onSendRequest("ACK",content_type,body,m_hdrs,flags,inv_cseq);

  AmSipRequest req;

  req.method = "ACK";
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
    
  if(!m_hdrs.empty())
    req.hdrs = m_hdrs;
  
  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + int2str(AmConfig::MaxForwards) + CRLF;
  }

  req.route = getRoute();

  req.content_type = content_type;
  req.body = body;
  
  if(onTxRequest(req))
    return -1;

  if (SipCtrlInterface::send(req, next_hop_ip, next_hop_port, outbound_interface))
    return -1;

  uac_trans.erase(inv_cseq);

  return 0;
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
