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

AmSipDialog::AmSipDialog(AmSipDialogEventHandler* h)
  : AmBasicSipDialog(h),pending_invites(0),sdp_local(),
    sdp_remote(),
    early_session_started(false),session_started(false),
    oa(this),
    offeranswer_enabled(true), rel100(this,h)
{
}

AmSipDialog::~AmSipDialog()
{
}

bool AmSipDialog::onRxReqSanity(const AmSipRequest& req)
{
  if (req.method == SIP_METH_ACK) {
    if(onRxReqStatus(req) && hdl)
      hdl->onSipRequest(req);
    return false;
  }

  if (req.method == SIP_METH_CANCEL) {

    if (uas_trans.find(req.cseq) == uas_trans.end()) {
      reply_error(req,481,SIP_REPLY_NOT_EXIST);
      return false;
    }

    if(onRxReqStatus(req) && hdl)
      hdl->onSipRequest(req);

    return false;
  }

  if(!AmBasicSipDialog::onRxReqSanity(req))
    return false;

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
      return false;
    }

    pending_invites++;
  }

  return rel100.onRequestIn(req);
}

bool AmSipDialog::onRxReqStatus(const AmSipRequest& req)
{
  switch(status){
  case Disconnected:
    if(req.method == SIP_METH_INVITE)
      setStatus(Trying);
    break;
  case Connected:
    if(req.method == SIP_METH_BYE)
      setStatus(Disconnecting);
    break;

  case Trying:
  case Proceeding:
  case Early:
    if(req.method == SIP_METH_BYE)
      setStatus(Disconnecting);
    else if(req.method == SIP_METH_CANCEL){
      setStatus(Cancelling);
      reply(req,200,"OK");
    }
    break;

  default: break;
  }

  bool cont = true;
  if (offeranswer_enabled) {
    cont = (oa.onRequestIn(req) == 0);
  }

  return cont;
}

int AmSipDialog::onSdpCompleted()
{
  if(!hdl) return 0;

  int ret = ((AmSipDialogEventHandler*)hdl)->
    onSdpCompleted(oa.getLocalSdp(), oa.getRemoteSdp());

  if(!ret) {
    sdp_local = oa.getLocalSdp();
    sdp_remote = oa.getRemoteSdp();

    if((getStatus() == Early) && !early_session_started) {
      ((AmSipDialogEventHandler*)hdl)->onEarlySessionStart();
      early_session_started = true;
    }

    if((getStatus() == Connected) && !session_started) {
      ((AmSipDialogEventHandler*)hdl)->onSessionStart();
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
  if(!hdl) return false;
  return ((AmSipDialogEventHandler*)hdl)->getSdpOffer(offer);
}

bool AmSipDialog::getSdpAnswer(const AmSdp& offer, AmSdp& answer)
{
  if(!hdl) return false;
  return ((AmSipDialogEventHandler*)hdl)->getSdpAnswer(offer,answer);
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

int AmSipDialog::onTxRequest(AmSipRequest& req, int& flags)
{
  rel100.onRequestOut(req);

  if (offeranswer_enabled && oa.onRequestOut(req))
    return -1;

  if(AmBasicSipDialog::onTxRequest(req,flags) < 0)
    return -1;

  // add transcoder statistics into request headers
  addTranscoderStats(req.hdrs);

  if((req.method == SIP_METH_INVITE) && (status == Disconnected)){
    setStatus(Trying);
  }
  else if((req.method == SIP_METH_BYE) && (status != Disconnecting)){
    setStatus(Disconnecting);
  }

  if ((req.method == SIP_METH_BYE) || (req.method == SIP_METH_CANCEL)) {
    flags |= SIP_FLAGS_NOCONTACT;
  }

  return 0;
}

// UAS behavior for locally sent replies
int AmSipDialog::onTxReply(const AmSipRequest& req, AmSipReply& reply, int& flags)
{
  if (offeranswer_enabled) {
    if(oa.onReplyOut(reply) < 0)
      return -1;
  }

  rel100.onReplyOut(reply);

  // update Dialog status
  switch(status){

  case Connected:
  case Disconnected:
    break;

  case Cancelling:
    if( (reply.cseq_method == SIP_METH_INVITE) &&
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
    if(reply.cseq_method == SIP_METH_INVITE){
      if(reply.code < 200) {
	setStatus(Early);
      }
      else if(reply.code < 300)
	setStatus(Connected);
      else
	setStatus(Disconnected);
    }
    break;

  case Disconnecting:
    if(reply.cseq_method == SIP_METH_BYE){

      // Only reason for refusing a BYE: 
      //  authentication (NYI at this place)
      // Also: we should not send provisionnal replies to a BYE
      if(reply.code >= 200)
	setStatus(Disconnected);
    }
    break;

  default:
    assert(0);
    break;
  }

  // add transcoder statistics into reply headers
  addTranscoderStats(reply.hdrs);

  // target-refresh requests and their replies need to contain Contact (1xx
  // replies only those establishing dialog, take care about them?)
  if(reply.cseq_method != SIP_METH_INVITE && 
     reply.cseq_method != SIP_METH_UPDATE) {
    
    flags |= SIP_FLAGS_NOCONTACT;
  }

  return AmBasicSipDialog::onTxReply(req,reply,flags);
}

void AmSipDialog::onReplyTxed(const AmSipRequest& req, const AmSipReply& reply)
{
  AmBasicSipDialog::onReplyTxed(req,reply);

  if (offeranswer_enabled) {
    oa.onReplySent(reply);
  }

  if (reply.code >= 200) {
    if(reply.cseq_method == SIP_METH_INVITE)
	pending_invites--;
  }
}

void AmSipDialog::onRequestTxed(const AmSipRequest& req)
{
  AmBasicSipDialog::onRequestTxed(req);

  if (offeranswer_enabled) {
    oa.onRequestSent(req);
  }
}

bool AmSipDialog::onRxReplySanity(const AmSipReply& reply)
{
  if(!getRemoteTag().empty()
     && reply.to_tag != getRemoteTag()) {

    if(status == Early) {
      if(reply.code < 200 && !reply.to_tag.empty()) {
	// Provision reply, such as 180, can come from a new UAS
	return true;
      }
    }
    else {
      DBG("dropping reply (%u %s) in non-Early state\n",
	  reply.code, reply.reason.c_str());
      return false;
    }
  }

  return true;
}

bool AmSipDialog::onRxReplyStatus(const AmSipReply& reply)
{
  // rfc3261 12.1
  // Dialog established only by 101-199 or 2xx 
  // responses to INVITE

  if(reply.cseq_method == SIP_METH_INVITE) {

    switch(status){

    case Trying:
    case Proceeding:
      if(reply.code < 200){
	// Do not go to Early state if reply did not come from an UAS
	if (((reply.code == 100) || (reply.code == 181) || (reply.code == 182))
	    || reply.to_tag.empty())
	  setStatus(Proceeding);
	else {
	  setStatus(Early);
	  setRemoteTag(reply.to_tag);
	  setRouteSet(reply.route);
	}
      }
      else if(reply.code < 300){
	setStatus(Connected);
	setRouteSet(reply.route);
	if(reply.to_tag.empty()){
	  DBG("received 2xx reply without to-tag "
	      "(callid=%s): sending BYE\n",reply.callid.c_str());

	  send_200_ack(reply.cseq);
	  sendRequest(SIP_METH_BYE);
	}
	else {
	  setRemoteTag(reply.to_tag);
	}
      }

      if(reply.code >= 300) {// error reply
	setStatus(Disconnected);
	setRemoteTag(reply.to_tag);
      }
      break;

    case Early:
      if(reply.code < 200){
	if (!reply.to_tag.empty() && (reply.to_tag != getRemoteTag())) {
	  DBG("updating dialog based on reply (%u %s) in Early state\n",
	      reply.code, reply.reason.c_str());
	  setRemoteTag(reply.to_tag);
	  setRouteSet(reply.route);
	  break;
	}
        //DROP!!!
	DBG("ignoring provisional reply (%u %s) in Early state\n",
	    reply.code, reply.reason.c_str());
      }
      else if(reply.code < 300){
	setStatus(Connected);
	setRouteSet(reply.route);
	if(reply.to_tag.empty()){
	  DBG("received 2xx reply without to-tag "
	      "(callid=%s): sending BYE\n",reply.callid.c_str());

	  sendRequest(SIP_METH_BYE);
	}
	else {
	  setRemoteTag(reply.to_tag);
	}
      }
      else { // error reply
	setStatus(Disconnected);
	setRemoteTag(reply.to_tag);
      }
      break;

    case Cancelling:
      if(reply.code >= 300){
	// CANCEL accepted
	DBG("CANCEL accepted, status -> Disconnected\n");
	setStatus(Disconnected);
      }
      else if(reply.code < 300){
	// CANCEL rejected
	DBG("CANCEL rejected/too late - bye()\n");
	setRemoteTag(reply.to_tag);
	setStatus(Connected);
	bye();
	// if BYE could not be sent,
	// there is nothing we can do anymore...
      }
      break;

    //case Connected: // late 200...
    //  TODO: if reply.to_tag != getRemoteTag()
    //        -> ACK + BYE (+absorb answer)
    default:
      break;
    }
  }

  if(status == Disconnecting){

    DBG("?Disconnecting?: cseq_method = %s; code = %i\n",
	reply.cseq_method.c_str(), reply.code);

    if((reply.cseq_method == SIP_METH_BYE) && (reply.code >= 200)){
      //TODO: support the auth case here (401/403)
      setStatus(Disconnected);
    }
  }

  if (offeranswer_enabled) {
    oa.onReplyIn(reply);
  }

  bool cont = true;
  if( (reply.code >= 200) && (reply.code < 300) &&
      (reply.cseq_method == SIP_METH_INVITE) ) {

    if(hdl) ((AmSipDialogEventHandler*)hdl)->onInvite2xx(reply);

  } else {
    cont = AmBasicSipDialog::onRxReplyStatus(reply);
  }

  return cont && rel100.onReplyIn(reply);
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
    if(hdl) ((AmSipDialogEventHandler*)hdl)->onNoAck(to_ev->cseq);
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

bool AmSipDialog::getUACInvTransPending() {
  for (TransMap::iterator it=uac_trans.begin();
       it != uac_trans.end(); it++) {
    if (it->second.method == SIP_METH_INVITE)
      return true;
  }
  return false;
}

AmSipRequest* AmSipDialog::getUASPendingInv()
{
  for (TransMap::iterator it=uas_trans.begin();
       it != uas_trans.end(); it++) {
    if (it->second.method == SIP_METH_INVITE)
      return &(it->second);
  }
  return NULL;
}

int AmSipDialog::bye(const string& hdrs, int flags)
{
  switch(status){

    case Disconnecting:
    case Connected: {
      // collect INVITE UAC transactions
      vector<unsigned int> ack_trans;
      for (TransMap::iterator it=uac_trans.begin(); it != uac_trans.end(); it++) {
	if (it->second.method == SIP_METH_INVITE){
	  ack_trans.push_back(it->second.cseq);
	}
      }
      // finish any UAC transaction before sending BYE
      for (vector<unsigned int>::iterator it=
	     ack_trans.begin(); it != ack_trans.end(); it++) {
	send_200_ack(*it);
      }

      if (status != Disconnecting) {
	setStatus(Disconnected);
	return sendRequest(SIP_METH_BYE, NULL, hdrs, flags);
      } else {
	return 0;
      }
    }

    case Trying:
    case Proceeding:
    case Early:
      if(getUACInvTransPending())
	return cancel(hdrs);
      else {
	    for (TransMap::iterator it=uas_trans.begin();
		 it != uas_trans.end(); it++) {
	      if (it->second.method == SIP_METH_INVITE){
		// let quit this call by sending final reply
		return reply(it->second,
			     487,"Request terminated");
	      }
	    }

	    // missing AmSipRequest to be able
	    // to send the reply on behalf of the app.
	    ERROR("ignoring bye() in %s state: "
		  "no UAC transaction to cancel or UAS transaction to reply.\n",
		  getStatusStr());
	    setStatus(Disconnected);
	}
	return 0;

    case Cancelling:
      for (TransMap::iterator it=uas_trans.begin();
	   it != uas_trans.end(); it++) {
	if (it->second.method == SIP_METH_INVITE){
	  // let's quit this call by sending final reply
	  return reply(it->second, 487,"Request terminated");
	}
      }

      // missing AmSipRequest to be able
      // to send the reply on behalf of the app.
      DBG("ignoring bye() in %s state: no UAS transaction to reply",getStatusStr());
      setStatus(Disconnected);
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
  if(getStatus() == Connected) {
    return sendRequest(SIP_METH_INVITE, body, hdrs, flags);
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
  if(getStatus() == Disconnected) {
    int res = sendRequest(SIP_METH_INVITE, body, hdrs);
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
  switch(getStatus()){
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
		       int expires,
		       const string& referred_by,
		       const string& extrahdrs)
{
  if(getStatus() == Connected) {
    string hdrs = SIP_HDR_COLSP(SIP_HDR_REFER_TO) + refer_to + CRLF;
    if (expires>=0) 
      hdrs+= SIP_HDR_COLSP(SIP_HDR_EXPIRES) + int2str(expires) + CRLF;
    hdrs+= extrahdrs;
    if (!referred_by.empty())
      hdrs+= SIP_HDR_COLSP(SIP_HDR_REFERRED_BY) + referred_by + CRLF;

    return sendRequest("REFER", NULL, hdrs);
  }
  else {
    DBG("refer(): we are not Connected."
	"(status=%s). do nothing!\n",getStatusStr());

    return 0;
  }	
}

int AmSipDialog::info(const string& hdrs, const AmMimeBody* body)
{
  if(getStatus() == Connected) {
    return sendRequest("INFO", body, hdrs);
  } else {
    DBG("info(): we are not Connected."
	"(status=%s). do nothing!\n", getStatusStr());
    return 0;
  }
}    

// proprietary
int AmSipDialog::transfer(const string& target)
{
  if(getStatus() == Connected){

    setStatus(Disconnecting);
		
    string      hdrs = "";
    AmSipDialog tmp_d(*this);
		
    tmp_d.route = "";
    // TODO!!!
    //tmp_d.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) 
    //  "<" + tmp_d.remote_uri + ">" CRLF;
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
  switch(getStatus()) {
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

	  if(getStatus() != Cancelling){
	    setStatus(Cancelling);
	    return SipCtrlInterface::cancel(&t->second.tt, local_tag,
					    t->first, t->second.hdrs);
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

int AmSipDialog::cancel(const string& hdrs)
{
    for(TransMap::reverse_iterator t = uac_trans.rbegin();
	t != uac_trans.rend(); t++) {

	if(t->second.method == SIP_METH_INVITE){

	  if(getStatus() != Cancelling){
	    setStatus(Cancelling);
	    return SipCtrlInterface::cancel(&t->second.tt, local_tag,
					    t->first, hdrs);
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

int AmSipDialog::drop()
{	
  setStatus(Disconnected);
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

  TransMap::iterator inv_it = uac_trans.find(inv_cseq);
  if (inv_it == uac_trans.end()) {
    ERROR("trying to ACK a non-existing transaction (cseq=%i;local_tag=%s)\n",
	  inv_cseq,local_tag.c_str());
    return -1;
  }

  AmSipRequest req;

  req.method = SIP_METH_ACK;
  req.r_uri = remote_uri;

  req.from = SIP_HDR_COLSP(SIP_HDR_FROM) + local_party;
  if(!ext_local_tag.empty())
    req.from += ";tag=" + ext_local_tag;
  else if(!local_tag.empty())
    req.from += ";tag=" + local_tag;
    
  req.to = SIP_HDR_COLSP(SIP_HDR_TO) + remote_party;
  if(!remote_tag.empty()) 
    req.to += ";tag=" + remote_tag;
    
  req.cseq = inv_cseq;// should be the same as the INVITE
  req.callid = callid;
  req.contact = getContactHdr();
    
  req.route = getRoute();

  req.max_forwards = inv_it->second.max_forwards;

  if(body != NULL)
    req.body = *body;

  if(onTxRequest(req,flags) < 0)
    return -1;

  if (!(flags&SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
  }

  int res = SipCtrlInterface::send(req, local_tag,
				   remote_tag.empty() || !next_hop_1st_req ? 
				   next_hop : "",
				   outbound_interface, 0, logger);
  if (res)
    return res;

  onRequestTxed(req);
  return 0;
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
