/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmSipDialog.h"
#include "AmConfig.h"
#include "AmSession.h"
#include "AmUtils.h"
#include "AmSipHeaders.h"
#include "SipCtrlInterface.h"
#include "sems.h"

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
  : status(Disconnected),oa_state(OA_None),
    cseq(10),r_cseq_i(false),hdl(h),
    pending_invites(0),cancel_pending(false),
    outbound_proxy(AmConfig::OutboundProxy),
    force_outbound_proxy(AmConfig::ForceOutboundProxy)
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

void AmSipDialog::onRxRequest(const AmSipRequest& req)
{
  DBG("AmSipDialog::onRxRequest(request)\n");

  if ((req.method != "ACK") && (req.method != "CANCEL")) {

    // Sanity checks
    if (r_cseq_i && req.cseq <= r_cseq){
      INFO("remote cseq lower than previous ones - refusing request\n");
      // see 12.2.2
      reply_error(req, 500, "Server Internal Error");
      return;
    }

    if (req.method == "INVITE") {
      if(pending_invites) {      
	reply_error(req,500,"Server Internal Error",
		    "Retry-After: " + int2str(get_random() % 10) + CRLF);
	return;
      }
      else {
	pending_invites++;
      }      
    }
    
    r_cseq = req.cseq;
    r_cseq_i = true;
    uas_trans[req.cseq] = AmSipTransaction(req.method,req.cseq,req.tt);
    
    // target refresh requests
    if (req.from_uri.length() && 
	(req.method == "INVITE" || 
	 req.method == "UPDATE" ||
	 req.method == "SUBSCRIBE" ||
	 req.method == "NOTIFY")) {

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
    if(req.method == "INVITE")
      status = Trying;
    break;
  case Connected:
    if(req.method == "BYE")
      status = Disconnecting;
    break;

  case Trying:
  case Proceeding:
  case Early:
    if(req.method == "BYE")
      status = Disconnecting;
    else if(req.method == "CANCEL")
      status = Cancelling;
    break;

  default: break;
  }

  if((req.method == "INVITE" || req.method == "UPDATE" || req.method == "ACK") &&
     !req.body.empty() && 
     (req.content_type == "application/sdp")) {

    const char* err_txt=NULL;
    int err_code = onRxSdp(req.body,&err_txt);
    if(err_code){
      if( req.method != "ACK" ){ // INVITE || UPDATE
	reply(req,err_code,err_txt);
      }
      else { // ACK
	DBG("error %i with SDP received in ACK request: sending BYE\n",err_code);
	bye();
      }
    }
  }

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
    return hdl->onSdpCompleted(sdp_local, sdp_remote);

  case OA_OfferSent:
    // There is already a pending offer!!!
    return -1;

  default:
    break;
  }

  return 0;
}

int AmSipDialog::triggerOfferAnswer(string& content_type, string& body)
{
  switch(status){
  case Connected:
  case Early:
    switch(oa_state){
    case OA_None:
    case OA_Completed:
      if(hdl->getSdpOffer(sdp_local)){
	sdp_local.print(body);
	content_type = "application/sdp";//FIXME
      }
      else {
	DBG("No SDP Offer to include in the reply.\n");
	return -1;
      }
      break;
    case OA_OfferRecved:
      if(hdl->getSdpAnswer(sdp_remote,sdp_local)){
	sdp_local.print(body);
	content_type = "application/sdp";//FIXME
      }
      else {
	DBG("No SDP Answer to include in the reply.\n");
	return -1;
      }
      break;
      
    default: 
      break;
    }
    break;
    
  default: 
    break;
  }

  return 0;
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
  if((req.method == "INVITE") && (status == Disconnected)){
    status = Trying;
  }
  else if((req.method == "BYE") && (status != Disconnecting)){
    status = Disconnecting;
  }

  if((req.method == "INVITE") || (req.method == "UPDATE")){
    if(triggerOfferAnswer(req.content_type, req.body))
      return -1;
  }

  if(req.content_type == "application/sdp") {

    if(onTxSdp(req.body)){
      DBG("onTxSdp() failed\n");
      return -1;
    }
  }

  return 0;
}

// UAS behavior for locally sent replies
int AmSipDialog::onTxReply(AmSipReply& reply)
{
  TransMap::iterator t_it = uas_trans.find(reply.cseq);
  if(t_it == uas_trans.end()){
    ERROR("could not find any transaction matching request\n");
    ERROR("method=%s; callid=%s; local_tag=%s; remote_tag=%s; cseq=%i\n",
	  reply.cseq_method.c_str(),callid.c_str(),local_tag.c_str(),
	  remote_tag.c_str(),reply.cseq);
    return -1;
  }
  DBG("reply: transaction found!\n");
    
  AmSipTransaction& t = t_it->second;

  // update Dialog status
  switch(status){

  case Connected:
  case Disconnected:
    break;

  case Cancelling:
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

  // update Offer/Answer state
  // TODO: support multipart mime
  if(reply.content_type.empty()){

    if((reply.cseq_method == "INVITE") || (reply.cseq_method == "UPDATE")){
      
      if(triggerOfferAnswer(reply.content_type, reply.body)){
	DBG("triggerOfferAnswer() failed\n");
	return -1;
      }
    }
  }

  if(reply.content_type == "application/sdp") {

    if(onTxSdp(reply.body)){

      DBG("onTxSdp() failed (replying 500 internal error)\n");
      reply.code = 500;
      reply.reason = "internal error";
      reply.body = "";
      reply.content_type = "";
    }
  }

  if((reply.code >= 200) && ((t.method != "INVITE") || (reply.cseq_method != "CANCEL"))){
    
    DBG("reply.cseq_method = %s; t.method = %s\n",
	reply.cseq_method.c_str(),t.method.c_str());
    
    if(t.method == "INVITE")
      pending_invites--;
    
    uas_trans.erase(t_it);
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
  DBG("onRxReply(reply): transaction found!\n");

  AmSipDialog::Status old_dlg_status = status;

  // rfc3261 12.1
  // Dialog established only by 101-199 or 2xx 
  // responses to INVITE

  if(reply.cseq_method == "INVITE") {

    switch(status){

    case Trying:
    case Proceeding:
      if(reply.code < 200){
	if(reply.to_tag.empty())
	  status = Proceeding;
	else
	  status = Early;
      }
      else if(reply.code < 300){
	status = Connected;
	route = reply.route;
	remote_uri = reply.contact;

	if(reply.to_tag.empty()){
	  DBG("received 2xx reply without to-tag "
	      "(callid=%s): sending BYE\n",reply.callid.c_str());

	  sendRequest("BYE");
	}
	else {
	  remote_tag = reply.to_tag;
	}
      }
      else { // error reply
	status = Disconnected;
      }
      
      if(cancel_pending){
	cancel_pending = false;
	bye();
      }
      break;

    case Early:
      // TODO:
      //
      // if((reply.code) != 100 && 
      //    !remote_tag.empty() && 
      //    (remote_tag != reply.to_tag)) {
      //   // fork a new dialog!!!
      // }

      if(reply.code < 200){
	// ignore this for now
      }
      else if(reply.code < 300){
	status = Connected;
	route = reply.route;
	remote_uri = reply.contact;
      }
      else { // error reply
	status = Disconnected;
      }
      break;

    case Cancelling:
      if(reply.cseq_method == "INVITE"){
	if(reply.code >= 300){
	  // CANCEL accepted
	  status = Disconnected;
	}
	else {
	  // CANCEL rejected
	  sendRequest("BYE");
	}
      }
      else {
	// we don't care about
	// the reply to the CANCEL itself...
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

  if(reply.code >= 200){
    if((reply.code < 300) && (reply.cseq_method == "INVITE")) {
      hdl->onInvite2xx(reply);
    }
    else {
      uac_trans.erase(t_it);
    }
  }

  hdl->onSipReply(reply, old_dlg_status);
}

void AmSipDialog::uasTimeout(AmSipTimeoutEvent* to_ev)
{
  assert(to_ev);

  switch(to_ev->type){
  case AmSipTimeoutEvent::no2xxACK:
    DBG("Timeout: missing 2xx-ACK\n");
    hdl->onNo2xxACK(to_ev->cseq);
    break;

  case AmSipTimeoutEvent::noErrorACK:
    DBG("Timeout: missing non-2xx-ACK\n");
    hdl->onNoErrorACK(to_ev->cseq);
    break;

  case AmSipTimeoutEvent::noPRACK:
    //TODO
    DBG("Timeout: missing PRACK\n");
    break;

  case AmSipTimeoutEvent::_noEv:
  default:
    break;
  };
  
  to_ev->processed = true;
}

string AmSipDialog::getContactHdr()
{
  if(contact_uri.empty()) {

    contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) "<sip:";

    if(!user.empty()) {
      contact_uri += user + "@";
    }
    
    contact_uri += (AmConfig::PublicIP.empty() ? 
      AmConfig::LocalSIPIP : AmConfig::PublicIP ) 
      + ":";
    contact_uri += int2str(AmConfig::LocalSIPPort);
    contact_uri += ">";

    contact_uri += CRLF;
  }

  return contact_uri;
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

  hdl->onSendReply(req,code,reason,
		   content_type,body,m_hdrs,flags);

  AmSipReply reply;

  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  reply.to_tag = local_tag;
  reply.hdrs = m_hdrs;
  reply.cseq = req.cseq;
  reply.cseq_method = req.method;

  if (!flags&SIP_FLAGS_VERBATIM) {
    // add Signature
    if (AmConfig::Signature.length())
      reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;
  }

  //if ((req.method!="CANCEL")&&
  //  !((req.method=="BYE")&&(code<300)))
  reply.contact = getContactHdr();

  if(!content_type.empty() && !body.empty()) {
    reply.content_type = content_type;
    reply.body = body;
  }

  if(onTxReply(reply)){
    DBG("onTxReply failed\n");
    return -1;
  }

  DBG("About to send reply...\n");
  int ret = SipCtrlInterface::send(reply);
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),req.method.c_str(),req.callid.c_str(),req.cseq);
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

  int ret = SipCtrlInterface::send(reply);
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),req.method.c_str(),req.callid.c_str(),req.cseq);
  }
  return ret;
}


int AmSipDialog::bye(const string& hdrs)
{
    switch(status){

    case Connected:
      return sendRequest("BYE", "", "", hdrs);

    case Trying:
    case Proceeding:
    case Early:
      return cancel();

      //case Cancelling:
      //case Disconnecting:
    default:
      DBG("bye(): we are not connected "
	  "(status=%i). do nothing!\n",status);
	return 0;
    }	
}

int AmSipDialog::reinvite(const string& hdrs,  
			  const string& content_type,
			  const string& body)
{
  if(status == Connected) {
    return sendRequest("INVITE", content_type, body, hdrs);
  }
  else {
    DBG("reinvite(): we are not connected "
	"(status=%i). do nothing!\n",status);
  }

  return -1;
}

int AmSipDialog::invite(const string& hdrs,  
			const string& content_type,
			const string& body)
{
  if(status == Disconnected) {
    int res = sendRequest("INVITE", content_type, body, hdrs);
    status = Trying;
    return res;
  }
  else {
    DBG("invite(): we are already connected."
	"(status=%i). do nothing!\n",status);
  }

  return -1;
}

int AmSipDialog::update(const string& hdrs)
{
  switch(status){
  case Trying:
  case Proceeding:
  case Early:
  case Connected://if Connected, we should send a re-INVITE instead...
    return sendRequest("UPDATE", "", "", hdrs);

  default:
  case Cancelling:
  case Disconnected:
  case Disconnecting:
    DBG("update(): we are not connected (anymore)."
	" (status=%i). do nothing!\n",status);

    return 0;
  }
}

int AmSipDialog::refer(const string& refer_to,
		       int expires)
{
  if(status != Connected) {
    string hdrs = SIP_HDR_COLSP(SIP_HDR_REFER_TO) + refer_to + CRLF;
    if (expires>=0) 
      hdrs+= SIP_HDR_COLSP(SIP_HDR_EXPIRES) + int2str(expires) + CRLF;
    return sendRequest("REFER", "", "", hdrs);
  }
  else {
    DBG("refer(): we are not Connected."
	"(status=%i). do nothing!\n",status);

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
			
      hdrs = PARAM_HDR ": " "Transfer-RR=\"" + route + "\"";
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
  string msg,ser_cmd;
  string m_hdrs = hdrs;

  if(hdl)
    hdl->onSendRequest(method,content_type,body,m_hdrs,flags,cseq);

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

  if(!route.empty()) {

    req.route = SIP_HDR_COLSP(SIP_HDR_ROUTE);
    if(force_outbound_proxy && !outbound_proxy.empty()){
      req.route += "<" + outbound_proxy + ";lr>, ";
    }
    req.route += route + CRLF;
  }
  else if (remote_tag.empty() && !outbound_proxy.empty()) {
    req.route = SIP_HDR_COLSP(SIP_HDR_ROUTE) "<" + outbound_proxy + ";lr>" CRLF;
  }

  if(!body.empty()) {
    req.content_type = content_type;
    req.body = body;
  }

  if(onTxRequest(req))
    return -1;

  if (SipCtrlInterface::send(req))
    return -1;
 
  uac_trans[cseq] = AmSipTransaction(method,cseq,req.tt);
  cseq++; // increment for next request

  return 0;
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

  if(!route.empty()) {
    req.route = SIP_HDR_COLSP(SIP_HDR_ROUTE) + route + CRLF;
  }

  if(!body.empty()) {
    req.content_type = content_type;
    req.body = body;
  }

  if (SipCtrlInterface::send(req))
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
