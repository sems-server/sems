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
#include "AmB2BSession.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "ampi/MonitoringAPI.h"
#include "AmSipHeaders.h"
#include "AmUtils.h"

#include <assert.h>

//
// AmB2BSession methods
//

AmB2BSession::AmB2BSession()
  : sip_relay_only(true),
    b2b_mode(B2BMode_Transparent)
{
}

AmB2BSession::AmB2BSession(const string& other_local_tag)
  : other_id(other_local_tag),
    sip_relay_only(true)
{
}


AmB2BSession::~AmB2BSession()
{
  DBG("relayed_req.size() = %u\n",(unsigned int)relayed_req.size());
  DBG("recvd_req.size() = %u\n",(unsigned int)recvd_req.size());
}

void AmB2BSession::set_sip_relay_only(bool r) { 
  sip_relay_only = r; 
}

AmB2BSession::B2BMode AmB2BSession::getB2BMode() const {
  return b2b_mode;
}

void AmB2BSession::clear_other()
{
#if __GNUC__ < 3
  string cleared ("");
  other_id.assign (cleared, 0, 0);
#else
  other_id.clear();
#endif
}

void AmB2BSession::process(AmEvent* event)
{
  B2BEvent* b2b_e = dynamic_cast<B2BEvent*>(event);
  if(b2b_e){

    onB2BEvent(b2b_e);
    return;
  }

  AmSession::process(event);
}

void AmB2BSession::onB2BEvent(B2BEvent* ev)
{
  switch(ev->event_id){

  case B2BSipRequest:
    {   
      B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
      assert(req_ev);

      if(req_ev->forward){
	if (req_ev->req.method == SIP_METH_INVITE &&
	    dlg.getUACInvTransPending()) {
	  // don't relay INVITE if INV trans pending
	  AmSipReply n_reply;
	  n_reply.code = 491;
	  n_reply.reason = "Request Pending";
	  n_reply.cseq = req_ev->req.cseq;
	  relayEvent(new B2BSipReplyEvent(n_reply, true, SIP_METH_INVITE));
	  return;
	}

	relaySip(req_ev->req);
      }
      else if( (req_ev->req.method == "BYE") ||
	       (req_ev->req.method == "CANCEL") ) {
		
	onOtherBye(req_ev->req);
      }
    }
    return;

  case B2BSipReply:
    {
      B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
      assert(reply_ev);

      DBG("B2BSipReply: %i %s (fwd=%s)\n",reply_ev->reply.code,
	  reply_ev->reply.reason.c_str(),reply_ev->forward?"true":"false");
      DBG("B2BSipReply: content-type = %s\n",reply_ev->reply.content_type.c_str());

      if(reply_ev->forward){

        std::map<int,AmSipRequest>::iterator t_req =
	  recvd_req.find(reply_ev->reply.cseq);

	if (t_req != recvd_req.end()) {
	  relaySip(t_req->second,reply_ev->reply);
		
	  if(reply_ev->reply.code >= 200){

	    if( (t_req->second.method == SIP_METH_INVITE) &&
		(reply_ev->reply.code == 487)){
	      
	      terminateLeg();
	    }
	    recvd_req.erase(t_req);
	  } 
	} else {
	  ERROR("Request with CSeq %u not found in recvd_req.\n",
		reply_ev->reply.cseq);
	}
      } else {
	// check whether not-forwarded (locally initiated)
	// INV/UPD transaction changed session in other leg
	if (SIP_IS_200_CLASS(reply_ev->reply.code) &&
	    (!reply_ev->reply.body.empty()) &&
	    (reply_ev->reply.method == SIP_METH_INVITE ||
	     reply_ev->reply.method == SIP_METH_UPDATE)) {
	  if (updateSessionDescription(reply_ev->reply.content_type,
				       reply_ev->reply.body)) {
	    if (dlg.getUACInvTransPending()) {
	      DBG("changed session, but UAC INVITE trans pending\n");
	      // todo(?): save until trans is finished?
	      return;
	    }
	    DBG("session description changed - refreshing\n");
	    sendEstablishedReInvite();
	  }
	}
      }
    }
    return;

  case B2BTerminateLeg:
    terminateLeg();
    break;

  }

  //ERROR("unknown event caught\n");
}

void AmB2BSession::onSipRequest(const AmSipRequest& req)
{
  bool fwd = sip_relay_only &&
    (req.method != "BYE") &&
    (req.method != "CANCEL");

  if(!fwd)
    AmSession::onSipRequest(req);
  else {
    updateRefreshMethod(req.hdrs);
    recvd_req.insert(std::make_pair(req.cseq,req));
  }

  B2BSipRequestEvent* r_ev = new B2BSipRequestEvent(req,fwd);
  
  // filter relayed INVITE/UPDATE body
  if (fwd && b2b_mode != B2BMode_Transparent &&
      (req.method == SIP_METH_INVITE || req.method == SIP_METH_UPDATE ||
       req.method == SIP_METH_ACK)) {
    DBG("filtering body for request '%s' (c/t '%s')\n",
	req.method.c_str(), req.content_type.c_str());
    filterBody(r_ev->req.content_type, r_ev->req.body, a_leg);
  }

  relayEvent(r_ev);
}

void AmB2BSession::onSipReply(const AmSipReply& reply,
			      int old_dlg_status,
			      const string& trans_method)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = (t != relayed_req.end()) && (reply.code != 100);

  DBG("onSipReply: %s -> %i %s (fwd=%s)\n",
      trans_method.c_str(), reply.code,reply.reason.c_str(),fwd?"true":"false");
  DBG("onSipReply: content-type = %s\n",reply.content_type.c_str());
  if(fwd) {
    updateRefreshMethod(reply.hdrs);

    AmSipReply n_reply = reply;
    n_reply.cseq = t->second.cseq;

    // filter relayed INVITE/UPDATE body
    if (b2b_mode != B2BMode_Transparent &&
	(trans_method == SIP_METH_INVITE || trans_method == SIP_METH_UPDATE)) {
      filterBody(n_reply.content_type, n_reply.body, a_leg);
    }
    
    relayEvent(new B2BSipReplyEvent(n_reply, true, t->second.method));

    if(reply.code >= 200) {
      if ((reply.code < 300) && (t->second.method == SIP_METH_INVITE)) {
	DBG("not removing relayed INVITE transaction yet...\n");
      } else 
	relayed_req.erase(t);
    }
  } else {
    AmSession::onSipReply(reply, old_dlg_status, trans_method);
    relayEvent(new B2BSipReplyEvent(reply, false, trans_method));
  }
}

void AmB2BSession::onInvite2xx(const AmSipReply& reply)
{
  TransMap::iterator it = relayed_req.find(reply.cseq);
  bool req_fwded = it != relayed_req.end();
  if(!req_fwded) {
    AmSession::onInvite2xx(reply);
  } else {
    DBG("no 200 ACK now: waiting for the 200 ACK from the other side...\n");
  }
}

void AmB2BSession::relayEvent(AmEvent* ev)
{
  DBG("AmB2BSession::relayEvent: id=%s\n",
      other_id.c_str());

  if(!other_id.empty())
    AmSessionContainer::instance()->postEvent(other_id,ev);
  else 
    delete ev;
}

void AmB2BSession::onOtherBye(const AmSipRequest& req)
{
  DBG("onOtherBye()\n");
  terminateLeg();
}

bool AmB2BSession::onOtherReply(const AmSipReply& reply)
{
  if(reply.code >= 300) 
    setStopped();
  
  return false;
}

void AmB2BSession::terminateLeg()
{
  setStopped();
  if ((dlg.getStatus() == AmSipDialog::Pending) 
      || (dlg.getStatus() == AmSipDialog::Connected))
    dlg.bye("", SIP_FLAGS_VERBATIM);
}

void AmB2BSession::terminateOtherLeg()
{
  if (!other_id.empty())
    relayEvent(new B2BEvent(B2BTerminateLeg));

  clear_other();
}

void AmB2BSession::onSessionTimeout() {
  DBG("Session Timer: Timeout, ending other leg.");
  terminateOtherLeg();
  AmSession::onSessionTimeout();
}

void AmB2BSession::saveSessionDescription(const string& content_type,
					  const string& body) {
  DBG("saving session description (%s, %.*s...)\n",
      content_type.c_str(), 50, body.c_str());
  established_content_type = content_type;
  established_body = body;

  const char* cmp_body_begin = body.c_str();
  size_t cmp_body_length = body.length();
  if (content_type == SIP_APPLICATION_SDP) {
    // for SDP, skip v and o line
    // (o might change even if SDP unchanged)
#define skip_line						\
    while (cmp_body_length-- && *cmp_body_begin != '\n')	\
      cmp_body_begin++;						\
    cmp_body_begin++;						\

    skip_line;
    skip_line;
  }

  body_hash = hashlittle(cmp_body_begin, cmp_body_length, 0);
}

bool AmB2BSession::updateSessionDescription(const string& content_type,
					    const string& body) {
  const char* cmp_body_begin = body.c_str();
  size_t cmp_body_length = body.length();
  if (content_type == SIP_APPLICATION_SDP) {
    // for SDP, skip v and o line
    // (o might change even if SDP unchanged)
    skip_line;
    skip_line;
  }

#undef skip_line

  uint32_t new_body_hash = hashlittle(cmp_body_begin, cmp_body_length, 0);

  if (body_hash != new_body_hash) {
    DBG("session description changed - saving (%s, %.*s...)\n",
	content_type.c_str(), 50, body.c_str());
    body_hash = new_body_hash;
    established_content_type = content_type;
    established_body = body;
    return true;
  }

  return false;
}

int AmB2BSession::sendEstablishedReInvite() {
  if (established_content_type.empty() || established_body.empty()) {
    ERROR("trying to re-INVITE with saved description, but none saved\n");
    return -1;
  }

  DBG("sending re-INVITE with saved session description\n");
  return dlg.reinvite("", established_content_type, established_body,
		      SIP_FLAGS_VERBATIM);
}

bool AmB2BSession::refresh(int flags) {
  DBG(" *** AmB2BSession::refresh *** \n");
  // not in B2B mode
  if (other_id.empty() ||
      // UPDATE as refresh handled like normal session
      refresh_method == REFRESH_UPDATE) {
    return AmSession::refresh(SIP_FLAGS_VERBATIM);
  }

  // refresh with re-INVITE
  if (dlg.getUACInvTransPending()) {
    DBG("INVITE transaction pending - not refreshing now\n");
    return false;
  }
  return sendEstablishedReInvite() == 0;
}

void AmB2BSession::relaySip(const AmSipRequest& req)
{
  if (req.method != "ACK") {
    relayed_req[dlg.cseq] = AmSipTransaction(req.method,req.cseq,req.tt);

    const string* hdrs = &req.hdrs;
    string m_hdrs;

    // translate RAck for PRACK
    if (req.method == SIP_METH_PRACK && req.rseq) {
      TransMap::iterator t;
      for (t=relayed_req.begin(); t != relayed_req.end(); t++) {
	if (t->second.cseq == req.rack_cseq) {
	  m_hdrs = req.hdrs +
	    SIP_HDR_COLSP(SIP_HDR_RACK) + int2str(req.rseq) +
	    " " + int2str(t->first) + " " + req.rack_method + CRLF;
	  hdrs = &m_hdrs;
	  break;
	}
      }
      if (t==relayed_req.end()) {
	WARN("Transaction with CSeq %d not found for translating RAck cseq\n",
	     req.rack_cseq);
      }
    }

    dlg.sendRequest(req.method, req.content_type, req.body, *hdrs, SIP_FLAGS_VERBATIM);
    // todo: relay error event back if sending fails

    if ((refresh_method != REFRESH_UPDATE) &&
	(req.method == SIP_METH_INVITE ||
	 req.method == SIP_METH_UPDATE) &&
	!req.body.empty()) {
      saveSessionDescription(req.content_type, req.body);
    }

  } else {
    //its a (200) ACK 
    TransMap::iterator t = relayed_req.begin(); 

    while (t != relayed_req.end()) {
      if (t->second.cseq == req.cseq)
	break;
      t++;
    } 
    if (t == relayed_req.end()) {
      ERROR("transaction for ACK not found in relayed requests\n");
      return;
    }

    DBG("sending relayed ACK\n");
    dlg.send_200_ack(AmSipTransaction(t->second.method, t->first,t->second.tt), 
		     req.content_type, req.body, req.hdrs, SIP_FLAGS_VERBATIM);

    if ((refresh_method != REFRESH_UPDATE) &&
	!req.body.empty() &&
	(t->second.method == SIP_METH_INVITE)) {
    // delayed SDP negotiation - save SDP
      saveSessionDescription(req.content_type, req.body);
    }

    relayed_req.erase(t);
  }
}

void AmB2BSession::relaySip(const AmSipRequest& orig, const AmSipReply& reply)
{
  const string* hdrs = &reply.hdrs;
  string m_hdrs;

  if (reply.rseq != 0) {
    m_hdrs = reply.hdrs +
      SIP_HDR_COLSP(SIP_HDR_RSEQ) + int2str(reply.rseq) + CRLF;
    hdrs = &m_hdrs;
  }

  dlg.reply(orig,reply.code,reply.reason,
	    reply.content_type,
	    reply.body, *hdrs,SIP_FLAGS_VERBATIM);

  if ((refresh_method != REFRESH_UPDATE) &&
      (orig.method == SIP_METH_INVITE ||
       orig.method == SIP_METH_UPDATE) &&
      !reply.body.empty()) {
    saveSessionDescription(reply.content_type, reply.body);
  }

}

int AmB2BSession::filterBody(string& content_type, string& body, bool is_a2b) {
  if (body.empty())
    return 0;

  if (content_type == SIP_APPLICATION_SDP) {
    AmSdp f_sdp;
    f_sdp.setBody(body.c_str());
    int res = f_sdp.parse();
    if (0 != res) {
      DBG("SDP parsing failed!\n");
      return res;
    }
    filterBody(f_sdp, is_a2b);
    f_sdp.print(body);
  }

  return 0;
}

int AmB2BSession::filterBody(AmSdp& sdp, bool is_a2b) {
  // default: transparent
  return 0;
}

// 
// AmB2BCallerSession methods
//

AmB2BCallerSession::AmB2BCallerSession()
  : AmB2BSession(),
    callee_status(None), sip_relay_early_media_sdp(false)
{
  a_leg = true;
}

AmB2BCallerSession::~AmB2BCallerSession()
{
}

void AmB2BCallerSession::set_sip_relay_early_media_sdp(bool r)
{
  sip_relay_early_media_sdp = r; 
}

void AmB2BCallerSession::terminateLeg()
{
  AmB2BSession::terminateLeg();
}

void AmB2BCallerSession::terminateOtherLeg()
{
  AmB2BSession::terminateOtherLeg();
  callee_status = None;
}

void AmB2BCallerSession::onB2BEvent(B2BEvent* ev)
{
  bool processed = false;

  if(ev->event_id == B2BSipReply){

    AmSipReply& reply = ((B2BSipReplyEvent*)ev)->reply;

    if(other_id != reply.local_tag){
      DBG("Dialog mismatch!\n");
      return;
    }

    DBG("reply received from other leg\n");
      
    switch(callee_status){
    case NoReply:
    case Ringing:
	
      if(reply.code < 200){
	if ((!sip_relay_only) && sip_relay_early_media_sdp && 
	    reply.code>=180 && reply.code<=183 && (!reply.body.empty())) {
	  if (reinviteCaller(reply)) {
	    ERROR("re-INVITEing caller for early session - "
		  "stopping this and other leg\n");
	    terminateOtherLeg();
	    terminateLeg();
	  }
	}
	  
	callee_status = Ringing;
      }
      else if(reply.code < 300){
	  
	callee_status  = Connected;
	  
	if (!sip_relay_only) {
	  sip_relay_only = true;
	  if (reinviteCaller(reply)) {
	    ERROR("re-INVITEing caller - stopping this and other leg\n");
	    terminateOtherLeg();
	    terminateLeg();
	  }
	}
      }
      else {
	// 	DBG("received %i from other leg: other_id=%s; reply.local_tag=%s\n",
	// 	    reply.code,other_id.c_str(),reply.local_tag.c_str());
	  
	terminateOtherLeg();
      }
	
      processed = onOtherReply(reply);
      break;
	
    default:
      DBG("reply from callee: %i %s\n",reply.code,reply.reason.c_str());
      break;
    }
  }
   
  if (!processed)
    AmB2BSession::onB2BEvent(ev);
}

void AmB2BCallerSession::relayEvent(AmEvent* ev)
{
  if(other_id.empty()){

    if(dynamic_cast<B2BEvent*>(ev)){

      B2BSipEvent*     sip_ev = dynamic_cast<B2BSipEvent*>(ev);
      B2BConnectEvent* co_ev  = dynamic_cast<B2BConnectEvent*>(ev);
	    
      if( (sip_ev && sip_ev->forward) || co_ev ) {
	createCalleeSession();
	if (other_id.length()) {
	  MONITORING_LOG(getLocalTag().c_str(), "b2b_leg", other_id.c_str());
	}
      }
    }
  }

  AmB2BSession::relayEvent(ev);
}

void AmB2BCallerSession::onSessionStart(const AmSipRequest& req)
{
  invite_req = req;
  AmB2BSession::onSessionStart(req);
}

void AmB2BCallerSession::connectCallee(const string& remote_party,
				       const string& remote_uri,
				       bool relayed_invite)
{
  if(callee_status != None)
    terminateOtherLeg();

  if (relayed_invite) {
    // relayed INVITE - we need to add the original INVITE to
    // list of received (relayed) requests
    recvd_req.insert(std::make_pair(invite_req.cseq,invite_req));

    // in SIP relay mode from the beginning
    sip_relay_only = true;
  }

  B2BConnectEvent* ev = new B2BConnectEvent(remote_party,remote_uri);

  if (b2b_mode == B2BMode_SDPFilter) {
    filterBody(invite_req.content_type, invite_req.body, true);
  }

  ev->content_type = invite_req.content_type;
  ev->body         = invite_req.body;
  ev->hdrs         = invite_req.hdrs;
  ev->relayed_invite = relayed_invite;
  ev->r_cseq       = invite_req.cseq;

  relayEvent(ev);
  callee_status = NoReply;
}

int AmB2BCallerSession::reinviteCaller(const AmSipReply& callee_reply)
{
  return dlg.sendRequest(SIP_METH_INVITE,
			 callee_reply.content_type, callee_reply.body,
			 "" /* hdrs */, SIP_FLAGS_VERBATIM);
}

void AmB2BCallerSession::createCalleeSession() {
  AmB2BCalleeSession* callee_session = newCalleeSession();  
  if (NULL == callee_session) 
    return;

  AmSipDialog& callee_dlg = callee_session->dlg;

  other_id = AmSession::getNewId();
  
  callee_dlg.local_tag    = other_id;
  callee_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;

  callee_dlg.local_party  = dlg.remote_party;
  callee_dlg.remote_party = dlg.local_party;
  callee_dlg.remote_uri   = dlg.local_uri;

  if (AmConfig::LogSessions) {
    INFO("Starting B2B callee session %s app %s\n",
	 callee_session->getLocalTag().c_str(), invite_req.cmd.c_str());
  }

  MONITORING_LOG5(other_id.c_str(), 
		  "app",  invite_req.cmd.c_str(),
		  "dir",  "out",
		  "from", callee_dlg.local_party.c_str(),
		  "to",   callee_dlg.remote_party.c_str(),
		  "ruri", callee_dlg.remote_uri.c_str());

  callee_session->start();

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}

AmB2BCalleeSession* AmB2BCallerSession::newCalleeSession()
{
  return new AmB2BCalleeSession(this);
}

AmB2BCalleeSession::AmB2BCalleeSession(const string& other_local_tag)
  : AmB2BSession(other_local_tag)
{
  a_leg = false;
}

AmB2BCalleeSession::AmB2BCalleeSession(const AmB2BCallerSession* caller)
  : AmB2BSession(caller->getLocalTag())
{
  a_leg = false;
  b2b_mode = caller->getB2BMode();
}

AmB2BCalleeSession::~AmB2BCalleeSession() {
}

void AmB2BCalleeSession::onB2BEvent(B2BEvent* ev)
{
  if(ev->event_id == B2BConnectLeg){
    B2BConnectEvent* co_ev = dynamic_cast<B2BConnectEvent*>(ev);
    if (!co_ev)
      return;

    MONITORING_LOG3(getLocalTag().c_str(), 
		    "b2b_leg", other_id.c_str(),
		    "to", co_ev->remote_party.c_str(),
		    "ruri", co_ev->remote_uri.c_str());


    dlg.remote_party = co_ev->remote_party;
    dlg.remote_uri   = co_ev->remote_uri;

    if (co_ev->relayed_invite) {
      relayed_req[dlg.cseq] =
	AmSipTransaction(SIP_METH_INVITE, co_ev->r_cseq, trans_ticket());
    }

    if (dlg.sendRequest(SIP_METH_INVITE,
			co_ev->content_type, co_ev->body,
			co_ev->hdrs, SIP_FLAGS_VERBATIM) < 0) {

      DBG("sending INVITE failed, relaying back 400 Bad Request\n");
      AmSipReply n_reply;
      n_reply.code = 400;
      n_reply.reason = "Bad Request";
      n_reply.cseq = co_ev->r_cseq;
      n_reply.local_tag = dlg.local_tag;
      relayEvent(new B2BSipReplyEvent(n_reply, co_ev->relayed_invite, SIP_METH_INVITE));

      if (co_ev->relayed_invite)
	relayed_req.erase(dlg.cseq);

      setStopped();
      return;
    }

    if (refresh_method != REFRESH_UPDATE)
      saveSessionDescription(co_ev->content_type, co_ev->body);

    return;
  }    

  AmB2BSession::onB2BEvent(ev);
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
