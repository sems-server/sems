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
#include "AmRtpReceiver.h"

#include <assert.h>

//
// helper functions
//

/** count active and inactive media streams in given SDP */
static void countStreams(const AmSdp &sdp, int &active, int &inactive)
{
  active = 0;
  inactive = 0;
  for (vector<SdpMedia>::const_iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) {
    if (m->port == 0) inactive++;
    else active++;
  }
}

static void errCode2RelayedReply(AmSipReply &reply, int err_code, unsigned default_code = 500)
{
  // FIXME: use cleaner method to propagate error codes/reasons, 
  // do it everywhere in the code
  if ((err_code < -399) && (err_code > -700)) {
    reply.code = -err_code;
  }
  else reply.code = default_code;

  // TODO: optimize with a table
  switch (reply.code) {
    case 400: reply.reason = "Bad Request"; break;
    case 478: reply.reason = "Unresolvable destination"; break;
    default: reply.reason = SIP_REPLY_SERVER_INTERNAL_ERROR;
  }
}

//
// AmB2BSession methods
//

AmB2BSession::AmB2BSession(const string& other_local_tag)
  : other_id(other_local_tag),
    sip_relay_only(true),
    b2b_mode(B2BMode_Transparent),
    rtp_relay_enabled(false),
    rtp_relay_force_symmetric_rtp(false),
    relay_rtp_streams(NULL), relay_rtp_streams_cnt(0),
    rtp_relay_transparent_seqno(true), rtp_relay_transparent_ssrc(true),
    est_invite_cseq(0),est_invite_other_cseq(0)
{
  memset(other_stream_fds,0,sizeof(int)*MAX_RELAY_STREAMS);
}

AmB2BSession::~AmB2BSession()
{
  if (rtp_relay_enabled)
    clearRtpReceiverRelay();

  if (NULL != relay_rtp_streams){
    for(unsigned int i=0; i<relay_rtp_streams_cnt; i++)
      delete relay_rtp_streams[i];
    delete[] relay_rtp_streams;
  }

  DBG("relayed_req.size() = %u\n",(unsigned int)relayed_req.size());

  map<int,AmSipRequest>::iterator it = recvd_req.begin();
  DBG("recvd_req.size() = %u\n",(unsigned int)recvd_req.size());
  for(;it != recvd_req.end(); ++it){
    DBG("  <%i,%s>\n",it->first,it->second.method.c_str());
  }
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
  DBG("AmB2BSession::onB2BEvent\n");
  switch(ev->event_id){

  case B2BSipRequest:
    {   
      B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
      assert(req_ev);

      DBG("B2BSipRequest: %s (fwd=%s)\n",
	  req_ev->req.method.c_str(),
	  req_ev->forward?"true":"false");

      if(req_ev->forward){

	if (req_ev->req.method == SIP_METH_INVITE &&
	    dlg.getUACInvTransPending()) {
	  // don't relay INVITE if INV trans pending
	  AmSipReply n_reply;
	  n_reply.code = 491;
	  n_reply.reason = SIP_REPLY_PENDING;
	  n_reply.cseq = req_ev->req.cseq;
	  n_reply.from_tag = dlg.local_tag;
	  DBG("relaying B2B SIP reply 491 " SIP_REPLY_PENDING "\n");
	  relayEvent(new B2BSipReplyEvent(n_reply, true, SIP_METH_INVITE));
	  return;
	}

        int res = relaySip(req_ev->req);
	if(res < 0) {
	  // reply relayed request internally
	  AmSipReply n_reply;
          errCode2RelayedReply(n_reply, res, 500);
	  n_reply.cseq = req_ev->req.cseq;
	  n_reply.from_tag = dlg.local_tag;
	  DBG("relaying B2B SIP error reply %u %s\n", n_reply.code, n_reply.reason.c_str());
	  relayEvent(new B2BSipReplyEvent(n_reply, true, req_ev->req.method));
	  return;
	}
      }
      
      if( (req_ev->req.method == SIP_METH_BYE) ||
	  (req_ev->req.method == SIP_METH_CANCEL) ) {
	
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
      DBG("B2BSipReply: content-type = %s\n",
	  reply_ev->reply.body.getCTStr().c_str());

      if(reply_ev->forward){

        std::map<int,AmSipRequest>::iterator t_req =
	  recvd_req.find(reply_ev->reply.cseq);

	if (t_req != recvd_req.end()) {
	  if ((reply_ev->reply.code >= 300) && (reply_ev->reply.code <= 305) &&
	      !reply_ev->reply.contact.empty()) {
	    // relay with Contact in 300 - 305 redirect messages
	    AmSipReply n_reply(reply_ev->reply);
	    n_reply.hdrs+=SIP_HDR_COLSP(SIP_HDR_CONTACT) +
	      reply_ev->reply.contact+ CRLF;

	    if(relaySip(t_req->second,n_reply) < 0) {
	      terminateOtherLeg();
	      terminateLeg();
	    }
	  } else {
	    // relay response
	    if(relaySip(t_req->second,reply_ev->reply) < 0) {
	      terminateOtherLeg();
	      terminateLeg();
	    }
	  }
		
	  if(reply_ev->reply.code >= 200){

	    if( (t_req->second.method == SIP_METH_INVITE) &&
		(reply_ev->reply.code >= 300)){
	      DBG("relayed INVITE failed with %u %s\n",
		  reply_ev->reply.code, reply_ev->reply.reason.c_str());
	    }
	    DBG("recvd_req.erase(<%u,%s>)\n", t_req->first, t_req->second.method.c_str());
	    recvd_req.erase(t_req);
	  } 
	} else {
	  ERROR("Request with CSeq %u not found in recvd_req.\n", reply_ev->reply.cseq);
	}
      } else {
	// check whether not-forwarded (locally initiated)
	// INV/UPD transaction changed session in other leg
	if (SIP_IS_200_CLASS(reply_ev->reply.code) &&
	    (!reply_ev->reply.body.empty()) &&
	    (reply_ev->reply.cseq_method == SIP_METH_INVITE ||
	     reply_ev->reply.cseq_method == SIP_METH_UPDATE)) {
	  if (updateSessionDescription(reply_ev->reply.body)) {
	    if (reply_ev->reply.cseq != est_invite_cseq) {
	      if (dlg.getUACInvTransPending()) {
		DBG("changed session, but UAC INVITE trans pending\n");
		// todo(?): save until trans is finished?
		return;
	      }
	      DBG("session description changed - refreshing\n");
	      sendEstablishedReInvite();
	    } else {
	      DBG("reply to establishing INVITE request - not refreshing\n");
	    }
	  }
	}
      }
    }
    return;

  case B2BTerminateLeg:
    DBG("terminateLeg()\n");
    terminateLeg();
    break;
  }

  //ERROR("unknown event caught\n");
}

void AmB2BSession::onSipRequest(const AmSipRequest& req)
{
  bool fwd = sip_relay_only &&
    (req.method != SIP_METH_CANCEL);

  if(!fwd)
    AmSession::onSipRequest(req);
  else {
    updateRefreshMethod(req.hdrs);

    if(req.method != SIP_METH_ACK)
      recvd_req.insert(std::make_pair(req.cseq,req));

    if(req.method == SIP_METH_BYE)
      onBye(req);
  }

  B2BSipRequestEvent* r_ev = new B2BSipRequestEvent(req,fwd);
  
  auto_ptr<AmSdp> req_sdp;

  // filter relayed INVITE/UPDATE body
  if (fwd && b2b_mode != B2BMode_Transparent &&
      (req.method == SIP_METH_INVITE || req.method == SIP_METH_UPDATE ||
       req.method == SIP_METH_ACK)) {
    if (req.cseq == est_invite_cseq && req.method == SIP_METH_INVITE)
      req_sdp.reset(invite_sdp.release());
    if (NULL == req_sdp.get())
      req_sdp.reset(new AmSdp());

    DBG("filtering body for request '%s' (c/t '%s')\n",
	req.method.c_str(), req.body.getCTStr().c_str());
    // todo: handle filtering errors
    filterBody(r_ev->req.body, *req_sdp.get(), a_leg);

    int active, inactive;
    countStreams(*req_sdp, active, inactive);
    if ((inactive > 0) && (active == 0)) {
      // no active streams remaining => reply 488 (FIXME: does it matter if we
      // filtered them out or they were already inactive?)

      DBG("all streams are marked as inactive, reply 488 "
	  SIP_REPLY_NOT_ACCEPTABLE_HERE"\n");
      dlg.reply(req, 488, SIP_REPLY_NOT_ACCEPTABLE_HERE);

      // cleanup
      delete r_ev;
      if(req.method != SIP_METH_ACK) {
        std::map<int,AmSipRequest>::iterator r = recvd_req.find(req.cseq);
        if (r != recvd_req.end()) recvd_req.erase(r);
      }

      return;
    }
  }

  if (rtp_relay_enabled &&
      (req.method == SIP_METH_INVITE || req.method == SIP_METH_UPDATE ||
       req.method == SIP_METH_ACK || req.method == SIP_METH_ACK)
      // don't update for initial INVITE again
      && (!(req.cseq == est_invite_cseq && req.method == SIP_METH_INVITE))
      ) {

    if (NULL == req_sdp.get()) {
      if (req.cseq == est_invite_cseq && req.method == SIP_METH_INVITE)
	req_sdp.reset(invite_sdp.release());
      if (NULL == req_sdp.get())
	req_sdp.reset(new AmSdp());
    }

    updateRelayStreams(r_ev->req.body, *req_sdp.get());
  }

  DBG("relaying B2B SIP request %s %s\n", r_ev->req.method.c_str(), r_ev->req.r_uri.c_str());
  relayEvent(r_ev);
}

/** update RTP relay streams with address/port from SDP body 
    create rtp_relay_streams if not existing
*/
void AmB2BSession::updateRelayStreams(const AmMimeBody& body,
				      AmSdp& parser_sdp) 
{
  const AmMimeBody* sdp_body = body.hasContentType(SIP_APPLICATION_SDP);
  if (!sdp_body)
    return;

  if (parser_sdp.media.empty()) {
    // SDP has not yet been parsed
    if (parser_sdp.parse((const char*)sdp_body->getPayload())) {
      DBG("SDP parsing failed!\n");
      return;
    }
  }

  if (NULL == relay_rtp_streams) {
    relay_rtp_streams_cnt = parser_sdp.media.size();
    if (relay_rtp_streams_cnt > MAX_RELAY_STREAMS) {
      WARN("got SDP with more media streams (%d) than MAX_RELAY_STREAMS (%u),"
	   "consider changing MAX_RELAY_STREAMS and rebuilding SEMS.\n",
	   relay_rtp_streams_cnt, MAX_RELAY_STREAMS);
      relay_rtp_streams_cnt = MAX_RELAY_STREAMS;
    }

    relay_rtp_streams = new AmRtpStream*[relay_rtp_streams_cnt];
    for(unsigned int i=0; i<relay_rtp_streams_cnt; i++){
      // create relay stream on set interface, else dlg interface
      int used_relay_interface = rtp_interface<0 ?
	dlg.getOutboundIf() : rtp_interface;
      DBG("using relay interface %i (rtp_interface=%i)\n",
	  used_relay_interface, rtp_interface);
      relay_rtp_streams[i] = new AmRtpStream(NULL, used_relay_interface);
      relay_rtp_streams[i]->setRtpRelayTransparentSeqno(rtp_relay_transparent_seqno);
      relay_rtp_streams[i]->setRtpRelayTransparentSSRC(rtp_relay_transparent_ssrc);
    }
    DBG("Created %u RTP relay streams\n", relay_rtp_streams_cnt);
  }

  unsigned int media_index = 0;
  for (std::vector<SdpMedia>::iterator it =
	 parser_sdp.media.begin(); it != parser_sdp.media.end(); it++) {

    if (media_index >= relay_rtp_streams_cnt) {
      WARN("trying to relay SDP with more media lines than "
	    "relay streams initialized (%u)\n", relay_rtp_streams_cnt);
      break;
    }

    string r_addr = it->conn.address;
    if (r_addr.empty())
      r_addr = parser_sdp.conn.address;

    if(it->port) {
      DBG("initializing RTP relay stream %u with remote <%s:%u>\n",
	  media_index, r_addr.c_str(), it->port);
      
      relay_rtp_streams[media_index]->setRAddr(r_addr, it->port);
      if ((it->dir == SdpMedia::DirActive) || rtp_relay_force_symmetric_rtp) {

	relay_rtp_streams[media_index]->setPassiveMode(true);

	if (rtp_relay_force_symmetric_rtp) {
	  DBG("Symmetric RTP: forced passive mode (#stream = %i)\n",media_index);
	} else {
	  DBG("Symmetric RTP: remote NATed, passive mode enabled (#stream = %i)\n",media_index);
	}
      }
      relay_rtp_streams[media_index]->resume();
    }
    else {
      relay_rtp_streams[media_index]->pause();
      DBG("disabled RTP relay stream %u\n",media_index);
    }

    media_index ++;
  }
}

bool AmB2BSession::replaceConnectionAddress(const AmMimeBody& body, 
					    AmMimeBody& r_body) {

  if(!body.isContentType(SIP_APPLICATION_SDP)) {
    DBG("body is not an SDP\n");
    return false;
  }

  AmSdp parser_sdp;
  if (parser_sdp.parse((const char*)body.getPayload())) {
    DBG("SDP parsing failed!\n");
    return false;
  }

  string relay_address;
  if (rtp_interface >= 0 && (unsigned)rtp_interface < AmConfig::Ifs.size()) {
    relay_address = AmConfig::Ifs[rtp_interface].PublicIP.empty() ?
      AmConfig::Ifs[rtp_interface].LocalIP :
      AmConfig::Ifs[rtp_interface].PublicIP;
  } else {
    relay_address = advertisedIP();
  }

  // place relay_address in connection address
  if (!parser_sdp.conn.address.empty()) {
    parser_sdp.conn.address = relay_address;
    DBG("new connection address: %s",parser_sdp.conn.address.c_str());
  }

  string replaced_ports;

  unsigned int media_index = 0;
  for (std::vector<SdpMedia>::iterator it =
	 parser_sdp.media.begin(); it != parser_sdp.media.end(); it++) {

    if (media_index >= relay_rtp_streams_cnt) {
      WARN("trying to relay SDP with more media lines than "
	   "relay streams initialized (%u)\n", relay_rtp_streams_cnt);
      break;
    }

    if(it->port) { // if stream active
      if (!it->conn.address.empty()) {
	it->conn.address = relay_address;
	DBG("new stram connection address: %s",it->conn.address.c_str());
      }
      try {
	it->port = relay_rtp_streams[media_index]->getLocalPort();
	replaced_ports += (!media_index) ? int2str(it->port) : "/"+int2str(it->port);
      } catch (const string& s) {
	ERROR("setting port: '%s'\n", s.c_str());
	throw string("error setting RTP port\n");
      }
    }
    media_index++;
  }

  // regenerate SDP
  string n_body;
  parser_sdp.print(n_body);
  r_body.parse(body.getCTStr(),
	       (const unsigned char*)n_body.c_str(),
	       n_body.length());

  DBG("replaced connection address in SDP with %s:%s.\n",
      relay_address.c_str(), replaced_ports.c_str());

  return true;
}

void AmB2BSession::updateUACTransCSeq(unsigned int old_cseq, unsigned int new_cseq) {
  if (old_cseq == new_cseq)
    return;

  TransMap::iterator t = relayed_req.find(old_cseq);
  if (t != relayed_req.end()) {
    relayed_req[new_cseq] = t->second;
    relayed_req.erase(t);
    DBG("updated relayed_req (UAC trans): CSeq %u -> %u\n", old_cseq, new_cseq);
  }
}


void AmB2BSession::onSipReply(const AmSipReply& reply,
			      AmSipDialog::Status old_dlg_status)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = (t != relayed_req.end()) && (reply.code != 100);

  DBG("onSipReply: %s -> %i %s (fwd=%s), c-t=%s\n",
      reply.cseq_method.c_str(), reply.code,reply.reason.c_str(),
      fwd?"true":"false",reply.body.getCTStr().c_str());

  if(fwd) {
    updateRefreshMethod(reply.hdrs);

    AmSipReply n_reply = reply;
    n_reply.cseq = t->second.cseq;

    AmSdp filter_sdp;

    // filter relayed INVITE/UPDATE body
    if (b2b_mode != B2BMode_Transparent &&
	(reply.cseq_method == SIP_METH_INVITE || reply.cseq_method == SIP_METH_UPDATE)) {
      filterBody(n_reply.body, filter_sdp, a_leg);
    }
    
    if (rtp_relay_enabled &&
	(reply.code >= 180  && reply.code < 300) &&
	(reply.cseq_method == SIP_METH_INVITE || reply.cseq_method == SIP_METH_UPDATE ||
	 reply.cseq_method == SIP_METH_ACK || reply.cseq_method == SIP_METH_ACK)) {
      updateRelayStreams(n_reply.body, filter_sdp);
    }

    DBG("relaying B2B SIP reply %u %s\n", n_reply.code, n_reply.reason.c_str());
    relayEvent(new B2BSipReplyEvent(n_reply, true, t->second.method));

    if(reply.code >= 200) {
      if ((reply.code < 300) && (t->second.method == SIP_METH_INVITE)) {
	DBG("not removing relayed INVITE transaction yet...\n");
      } else 
	relayed_req.erase(t);
    }
  } else {
    AmSession::onSipReply(reply, old_dlg_status);

    AmSipReply n_reply = reply;
    if(est_invite_cseq == reply.cseq){
      n_reply.cseq = est_invite_other_cseq;
    }
    else {
      // the reply here will not have the proper cseq for the other side.
    }
    DBG("relaying B2B SIP reply %u %s\n", n_reply.code, n_reply.reason.c_str());
    relayEvent(new B2BSipReplyEvent(n_reply, false, reply.cseq_method));
  }
}

void AmB2BSession::onInvite2xx(const AmSipReply& reply)
{
  TransMap::iterator it = relayed_req.find(reply.cseq);
  bool req_fwded = it != relayed_req.end();
  if(!req_fwded) {
    DBG("req not fwded\n");
    AmSession::onInvite2xx(reply);
  } else {
    DBG("no 200 ACK now: waiting for the 200 ACK from the other side...\n");
  }
}

int AmB2BSession::onSdpCompleted(const AmSdp& local_sdp, const AmSdp& remote_sdp)
{
  if(!sip_relay_only){
    return AmSession::onSdpCompleted(local_sdp,remote_sdp);
  }
  
  DBG("sip_relay_only = true: doing nothing!\n");
  return 0;
}

int AmB2BSession::relayEvent(AmEvent* ev)
{
  DBG("AmB2BSession::relayEvent: to other_id='%s'\n",
      other_id.c_str());

  if(!other_id.empty()) {
    if (!AmSessionContainer::instance()->postEvent(other_id,ev))
      return -1;
  } else {
    delete ev;
  }

  return 0;
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

  if (rtp_relay_enabled)
    clearRtpReceiverRelay();

  dlg.bye("", SIP_FLAGS_VERBATIM);
}

void AmB2BSession::terminateOtherLeg()
{
  if (!other_id.empty())
    relayEvent(new B2BEvent(B2BTerminateLeg));

  clear_other();
}

void AmB2BSession::onSessionTimeout() {
  DBG("Session Timer: Timeout, ending other leg\n");
  terminateOtherLeg();
  AmSession::onSessionTimeout();
}

void AmB2BSession::onRemoteDisappeared(const AmSipReply& reply) {
  DBG("remote unreachable, ending other leg\n");
  terminateOtherLeg();
  AmSession::onRemoteDisappeared(reply);
}

void AmB2BSession::onNoAck(unsigned int cseq)
{
  DBG("OnNoAck(%u): terminate other leg.\n",cseq);
  terminateOtherLeg();
  AmSession::onNoAck(cseq);
}

void AmB2BSession::saveSessionDescription(const AmMimeBody& body) {

  const AmMimeBody* sdp_body = body.hasContentType(SIP_APPLICATION_SDP);
  if(!sdp_body)
    return;

  DBG("saving session description (%s, %.*s...)\n",
      sdp_body->getCTStr().c_str(), 50, sdp_body->getPayload());

  established_body = *sdp_body;

  const char* cmp_body_begin = (const char*)sdp_body->getPayload();
  size_t cmp_body_length = sdp_body->getLen();

#define skip_line						\
    while (cmp_body_length && *cmp_body_begin != '\n') {	\
      cmp_body_begin++;						\
      cmp_body_length--;					\
    }								\
    if (cmp_body_length) {					\
      cmp_body_begin++;						\
      cmp_body_length--;					\
    }

  if (cmp_body_length) {
  // for SDP, skip v and o line
  // (o might change even if SDP unchanged)
  skip_line;
  skip_line;
  }

  body_hash = hashlittle(cmp_body_begin, cmp_body_length, 0);
}

bool AmB2BSession::updateSessionDescription(const AmMimeBody& body) {

  const AmMimeBody* sdp_body = body.hasContentType(SIP_APPLICATION_SDP);
  if(!sdp_body)
    return false;

  const char* cmp_body_begin = (const char*)sdp_body->getPayload();
  size_t cmp_body_length = sdp_body->getLen();
  if (cmp_body_length) {
    // for SDP, skip v and o line
    // (o might change even if SDP unchanged)
    skip_line;
    skip_line;
  }

#undef skip_line

  uint32_t new_body_hash = hashlittle(cmp_body_begin, cmp_body_length, 0);

  if (body_hash != new_body_hash) {
    DBG("session description changed - saving (%s, %.*s...)\n",
	sdp_body->getCTStr().c_str(), 50, sdp_body->getPayload());
    body_hash = new_body_hash;
    established_body = body;
    return true;
  }

  return false;
}

int AmB2BSession::sendEstablishedReInvite() {  
  if (established_body.empty()) {
    ERROR("trying to re-INVITE with saved description, but none saved\n");
    return -1;
  }

  DBG("sending re-INVITE with saved session description\n");

  try {
    AmMimeBody* body = &established_body; // contains only SDP
    AmMimeBody r_body;
    if (rtp_relay_enabled &&
	replaceConnectionAddress(established_body, r_body)) {
      body = &r_body; // should we keep the old one intact???
    }

    return dlg.reinvite("", body,
			SIP_FLAGS_VERBATIM);
  } catch (const string& s) {
    ERROR("sending established SDP reinvite: %s\n", s.c_str());
    return -1;
  }
}

bool AmB2BSession::refresh(int flags) {
  // no session refresh if not connected
  if (dlg.getStatus() != AmSipDialog::Connected)
    return false;

  DBG(" AmB2BSession::refresh: refreshing session\n");
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

int AmB2BSession::relaySip(const AmSipRequest& req)
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

    AmMimeBody r_body(req.body);
    const AmMimeBody* body = &r_body;
    if (body && rtp_relay_enabled &&
	(req.method == SIP_METH_INVITE || req.method == SIP_METH_UPDATE ||
	 req.method == SIP_METH_ACK || req.method == SIP_METH_ACK)) {

      body = req.body.hasContentType(SIP_APPLICATION_SDP);
      if (replaceConnectionAddress(*body, *r_body.hasContentType(SIP_APPLICATION_SDP))) {
	body = &r_body;
      }
      else {
	body = &req.body;
      }
    }

    DBG("relaying SIP request %s %s\n", req.method.c_str(), req.r_uri.c_str());
    int err = dlg.sendRequest(req.method, body, *hdrs, SIP_FLAGS_VERBATIM);
    if(err < 0){
      ERROR("dlg.sendRequest() failed\n");
      return err;
    }

    if ((refresh_method != REFRESH_UPDATE) &&
	(req.method == SIP_METH_INVITE ||
	 req.method == SIP_METH_UPDATE) &&
	!req.body.empty()) {
      saveSessionDescription(req.body);
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
      return -1;
    }

    DBG("sending relayed 200 ACK\n");
    int err = dlg.send_200_ack(t->first, &req.body, 
			       req.hdrs, SIP_FLAGS_VERBATIM);
    if(err < 0) {
      ERROR("dlg.send_200_ack() failed\n");
      return err;
    }

    if ((refresh_method != REFRESH_UPDATE) &&
	!req.body.empty() &&
	(t->second.method == SIP_METH_INVITE)) {
    // delayed SDP negotiation - save SDP
      saveSessionDescription(req.body);
    }

    relayed_req.erase(t);
  }

  return 0;
}

int AmB2BSession::relaySip(const AmSipRequest& orig, const AmSipReply& reply)
{
  const string* hdrs = &reply.hdrs;
  string m_hdrs;

  if (reply.rseq != 0) {
    m_hdrs = reply.hdrs +
      SIP_HDR_COLSP(SIP_HDR_RSEQ) + int2str(reply.rseq) + CRLF;
    hdrs = &m_hdrs;
  }

  AmMimeBody r_body(reply.body);
  const AmMimeBody* body = &r_body;
  if (rtp_relay_enabled &&
      (orig.method == SIP_METH_INVITE || orig.method == SIP_METH_UPDATE ||
       orig.method == SIP_METH_ACK || orig.method == SIP_METH_ACK)) {

    body = reply.body.hasContentType(SIP_APPLICATION_SDP);
    if (body && replaceConnectionAddress(*body, *r_body.hasContentType(SIP_APPLICATION_SDP))) {
      body = &r_body;
    }
    else {
      body = &reply.body;
    }
  }

  DBG("relaying SIP reply %u %s\n", reply.code, reply.reason.c_str());
  int err = dlg.reply(orig,reply.code,reply.reason,
		      body, *hdrs, SIP_FLAGS_VERBATIM);
  if(err < 0){
    ERROR("dlg.reply() failed\n");
    return err;
  }

  if ((refresh_method != REFRESH_UPDATE) &&
      (orig.method == SIP_METH_INVITE ||
       orig.method == SIP_METH_UPDATE) &&
      !reply.body.empty()) {
    saveSessionDescription(reply.body);
  }

  return 0;
}

int AmB2BSession::filterBody(AmMimeBody& body, AmSdp& filter_sdp,
			     bool is_a2b) {
  if (body.empty())
    return 0;

  AmMimeBody* sdp_body = body.hasContentType(SIP_APPLICATION_SDP);
  if (sdp_body) {
    int res = filter_sdp.parse((const char*)sdp_body->getPayload());
    if (0 != res) {
      DBG("SDP parsing failed!\n");
      return res;
    }
    filterBody(filter_sdp, is_a2b);
    string n_body;
    filter_sdp.print(n_body);
    sdp_body->setPayload((const unsigned char*)n_body.c_str(),
			 n_body.length());
  }

  return 0;
}

int AmB2BSession::filterBody(AmSdp& sdp, bool is_a2b) {
  // default: transparent
  return 0;
}

void AmB2BSession::enableRtpRelay(const AmSipRequest& initial_invite_req) {
  DBG("enabled RTP relay mode for B2B call '%s'\n",
      getLocalTag().c_str());
  rtp_relay_enabled = true;

  // save AmSdp object of initial INVITE body
  invite_sdp.reset(new AmSdp());
  updateRelayStreams(initial_invite_req.body,
		     *invite_sdp.get());
}

void AmB2BSession::enableRtpRelay() {
  DBG("enabled RTP relay mode for B2B call '%s'\n",
      getLocalTag().c_str());
  rtp_relay_enabled = true;
}

void AmB2BSession::disableRtpRelay() {
  DBG("disabled RTP relay mode for B2B call '%s'\n",
      getLocalTag().c_str());
  rtp_relay_enabled = false;
}

void AmB2BSession::setupRelayStreams(AmB2BSession* other_session) {
  if (!rtp_relay_enabled)
    return;

  if (NULL == other_session) {
    ERROR("trying to setup relay for NULL b2b session\n");
    return;
  }

  if (NULL == relay_rtp_streams) {
    relay_rtp_streams_cnt = other_session->relay_rtp_streams_cnt;
    DBG("creating %u RTP streams from other_session\n",
	relay_rtp_streams_cnt);
    relay_rtp_streams = new AmRtpStream*[relay_rtp_streams_cnt];
    for(unsigned int i=0; i<relay_rtp_streams_cnt; i++){
      // create relay stream on set interface, else dlg interface
      relay_rtp_streams[i] = new AmRtpStream(NULL, rtp_interface<0 ?
					     dlg.getOutboundIf() : rtp_interface);
      relay_rtp_streams[i]->setRtpRelayTransparentSeqno(rtp_relay_transparent_seqno);
      relay_rtp_streams[i]->setRtpRelayTransparentSSRC(rtp_relay_transparent_ssrc);
    }
  }

  // link the other streams as our relay streams
  for (unsigned int i=0; i<relay_rtp_streams_cnt; i++) {
    other_session->relay_rtp_streams[i]->setRelayStream(relay_rtp_streams[i]);
    other_stream_fds[i] = other_session->relay_rtp_streams[i]->getLocalSocket();
    relay_rtp_streams[i]->setLocalIP(localRTPIP());
    relay_rtp_streams[i]->enableRtpRelay();
  }
}

void AmB2BSession::setRtpRelayInterface(int relay_interface) {
  DBG("setting RTP relay interface for session '%s' to %i\n",
      getLocalTag().c_str(), relay_interface);
  rtp_interface = relay_interface;
}

void AmB2BSession::setRtpRelayTransparentSeqno(bool transparent) {
  rtp_relay_transparent_seqno = transparent;
}

void AmB2BSession::setRtpRelayTransparentSSRC(bool transparent) {
  rtp_relay_transparent_ssrc = transparent;
}

void AmB2BSession::clearRtpReceiverRelay() {
  for (unsigned int i=0; i<relay_rtp_streams_cnt; i++) {
    // clear the other call's RTP relay streams from RTP receiver
    if (other_stream_fds[i]) {
      AmRtpReceiver::instance()->removeStream(other_stream_fds[i]);
      other_stream_fds[i] = 0;
    }
    // clear our relay streams from RTP receiver
    if (relay_rtp_streams[i]->hasLocalSocket()) {
      AmRtpReceiver::instance()->removeStream(relay_rtp_streams[i]->getLocalSocket());
    }
  }
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

    if(other_id.empty()){
      //DBG("Discarding B2BSipReply from other leg (other_id empty)\n");
      DBG("B2BSipReply: other_id empty ("
	  "reply code=%i; method=%s; callid=%s; from_tag=%s; "
	  "to_tag=%s; cseq=%i)\n",
	  reply.code,reply.cseq_method.c_str(),reply.callid.c_str(),reply.from_tag.c_str(),
	  reply.to_tag.c_str(),reply.cseq);
      //return;
    }
    else if(other_id != reply.from_tag){// was: local_tag
      DBG("Dialog mismatch! (oi=%s;ft=%s)\n",
	  other_id.c_str(),reply.from_tag.c_str());
      return;
    }

    DBG("%u %s reply received from other leg\n", reply.code, reply.reason.c_str());
      
    switch(callee_status){
    case NoReply:
    case Ringing:
      if (reply.cseq == invite_req.cseq) {
	if(reply.code < 200){
	  if ((!sip_relay_only) && sip_relay_early_media_sdp &&
	      reply.code>=180 && reply.code<=183 && (!reply.body.empty())) {
	    DBG("sending re-INVITE to caller with early media SDP\n");
	    if (reinviteCaller(reply)) {
	      ERROR("re-INVITEing caller for early session failed - "
		    "stopping this and other leg\n");
	      terminateOtherLeg();
	      terminateLeg();
	    }
	  }
	  
	  callee_status = Ringing;
	} else if(reply.code < 300){
	  
	  callee_status  = Connected;
	  DBG("setting callee status to connected\n");
	  if (!sip_relay_only) {
	    DBG("received 200 class reply to establishing INVITE: "
		"switching to SIP relay only mode, sending re-INVITE to caller\n");
	    sip_relay_only = true;
	    if (reinviteCaller(reply)) {
	      ERROR("re-INVITEing caller failed - stopping this and other leg\n");
	      terminateOtherLeg();
	      terminateLeg();
	    }
	  }
	} else {
	  // 	DBG("received %i from other leg: other_id=%s; reply.local_tag=%s\n",
	  // 	    reply.code,other_id.c_str(),reply.local_tag.c_str());
	  
	  // TODO: terminated my own leg instead? (+ clear_other())
	  terminateOtherLeg();
	}

	processed = onOtherReply(reply);
      }
	
      break;
	
    default:
      DBG("reply from callee: %u %s\n",reply.code,reply.reason.c_str());
      break;
    }
  }
   
  if (!processed)
    AmB2BSession::onB2BEvent(ev);
}

int AmB2BCallerSession::relayEvent(AmEvent* ev)
{
  if(other_id.empty() && !getStopped()){

    bool create_callee = false;
    B2BSipEvent* sip_ev = dynamic_cast<B2BSipEvent*>(ev);
    if (sip_ev && sip_ev->forward)
      create_callee = true;
    else
      create_callee = dynamic_cast<B2BConnectEvent*>(ev) != NULL;

    if (create_callee) {
      createCalleeSession();
      if (other_id.length()) {
	MONITORING_LOG(getLocalTag().c_str(), "b2b_leg", other_id.c_str());
      }
    }

  }

  return AmB2BSession::relayEvent(ev);
}

void AmB2BCallerSession::onInvite(const AmSipRequest& req)
{
  invite_req = req;
  est_invite_cseq = req.cseq;

  AmB2BSession::onInvite(req);
}

void AmB2BCallerSession::onInvite2xx(const AmSipReply& reply)
{
  invite_req.cseq = reply.cseq;
  est_invite_cseq = reply.cseq;

  AmB2BSession::onInvite2xx(reply);
}

void AmB2BCallerSession::onCancel(const AmSipRequest& req)
{
  terminateOtherLeg();
  terminateLeg();
}

void AmB2BCallerSession::onSystemEvent(AmSystemEvent* ev) {
  if (ev->sys_event == AmSystemEvent::ServerShutdown) {
    terminateOtherLeg();
  }

  AmB2BSession::onSystemEvent(ev);
}

void AmB2BCallerSession::onRemoteDisappeared(const AmSipReply& reply) {
  DBG("remote unreachable, ending B2BUA call\n");
  if (rtp_relay_enabled)
    clearRtpReceiverRelay();

  AmB2BSession::onRemoteDisappeared(reply);
}

void AmB2BCallerSession::onBye(const AmSipRequest& req)
{
  if (rtp_relay_enabled)
    clearRtpReceiverRelay();

  AmB2BSession::onBye(req);
}

void AmB2BCallerSession::connectCallee(const string& remote_party,
				       const string& remote_uri,
				       bool relayed_invite)
{
  if(callee_status != None)
    terminateOtherLeg();

  if (b2b_mode == B2BMode_SDPFilter) {
    AmSdp filter_sdp;
    filterBody(invite_req.body, filter_sdp, true);
    int active, inactive;
    countStreams(filter_sdp, active, inactive);
    if ((inactive > 0) && (active == 0)) {
      // no active streams remaining => reply 488 (FIXME: does it matter if we
      // filtered them out or they were already inactive?)
      DBG("all streams are marked as inactive\n");
      throw AmSession::Exception(488, SIP_REPLY_NOT_ACCEPTABLE_HERE);
    }
  }

  if (relayed_invite) {
    // relayed INVITE - we need to add the original INVITE to
    // list of received (relayed) requests
    recvd_req.insert(std::make_pair(invite_req.cseq,invite_req));

    // in SIP relay mode from the beginning
    sip_relay_only = true;
  }

  B2BConnectEvent* ev = new B2BConnectEvent(remote_party,remote_uri);

  ev->body         = invite_req.body;
  ev->hdrs         = invite_req.hdrs;
  ev->relayed_invite = relayed_invite;
  ev->r_cseq       = invite_req.cseq;

  DBG("relaying B2B connect event to %s\n", remote_uri.c_str());
  relayEvent(ev);
  callee_status = NoReply;
}

int AmB2BCallerSession::reinviteCaller(const AmSipReply& callee_reply)
{
  return dlg.sendRequest(SIP_METH_INVITE,
			 &callee_reply.body,
			 "" /* hdrs */, SIP_FLAGS_VERBATIM);
}

void AmB2BCallerSession::createCalleeSession() {
  AmB2BCalleeSession* callee_session = newCalleeSession();  
  if (NULL == callee_session) 
    return;

  AmSipDialog& callee_dlg = callee_session->dlg;

  other_id = AmSession::getNewId();
  
  callee_dlg.local_tag    = other_id;
  callee_dlg.callid       = AmSession::getNewId();

  callee_dlg.local_party  = dlg.remote_party;
  callee_dlg.remote_party = dlg.local_party;
  callee_dlg.remote_uri   = dlg.local_uri;

  if (AmConfig::LogSessions) {
    INFO("Starting B2B callee session %s\n",
	 callee_session->getLocalTag().c_str());
  }

  MONITORING_LOG4(other_id.c_str(), 
		  "dir",  "out",
		  "from", callee_dlg.local_party.c_str(),
		  "to",   callee_dlg.remote_party.c_str(),
		  "ruri", callee_dlg.remote_uri.c_str());

  try {
    initializeRTPRelay(callee_session);
  } catch (...) {
    delete callee_session;
    throw;
  }

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);

  callee_session->start();
}

AmB2BCalleeSession* AmB2BCallerSession::newCalleeSession()
{
  return new AmB2BCalleeSession(this);
}

void AmB2BCallerSession::initializeRTPRelay(AmB2BCalleeSession* callee_session) {
  if (!callee_session || !rtp_relay_enabled)
    return;

  callee_session->enableRtpRelay();
  callee_session->setupRelayStreams(this);
  setupRelayStreams(callee_session);

  // bind caller session's relay_streams to a port
  for (unsigned int i=0; i<relay_rtp_streams_cnt; i++)
    relay_rtp_streams[i]->getLocalPort();
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
  rtp_relay_enabled = caller->getRtpRelayEnabled();
  rtp_relay_force_symmetric_rtp = caller->getRtpRelayForceSymmetricRtp();
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

    AmMimeBody r_body(co_ev->body);
    const AmMimeBody* body = &co_ev->body;
    if (rtp_relay_enabled) {
      try {
	body = co_ev->body.hasContentType(SIP_APPLICATION_SDP);
	if (replaceConnectionAddress(*body, *r_body.hasContentType(SIP_APPLICATION_SDP))) {
	  body = &r_body;
	}
	else {
	  body = &co_ev->body;
	}
      } catch (const string& s) {
	AmSipReply n_reply;
	n_reply.code = 500;
	n_reply.reason = SIP_REPLY_SERVER_INTERNAL_ERROR;
	n_reply.cseq = co_ev->r_cseq;
	n_reply.from_tag = dlg.local_tag;
	DBG("relaying B2B SIP reply 500" SIP_REPLY_SERVER_INTERNAL_ERROR "\n");
	relayEvent(new B2BSipReplyEvent(n_reply, co_ev->relayed_invite, SIP_METH_INVITE));
	  throw;
      }
    }

    int res = dlg.sendRequest(SIP_METH_INVITE, body,
			co_ev->hdrs, SIP_FLAGS_VERBATIM);
    if (res < 0) {
      DBG("sending INVITE failed, relaying back error reply\n");
      AmSipReply n_reply;
      errCode2RelayedReply(n_reply, res, 400);
      n_reply.cseq = co_ev->r_cseq;
      n_reply.from_tag = dlg.local_tag;
      DBG("relaying B2B SIP reply %u %s\n", n_reply.code, n_reply.reason.c_str());
      relayEvent(new B2BSipReplyEvent(n_reply, co_ev->relayed_invite, SIP_METH_INVITE));

      if (co_ev->relayed_invite)
	relayed_req.erase(dlg.cseq);

      setStopped();
      return;
    }

    if (refresh_method != REFRESH_UPDATE)
      saveSessionDescription(co_ev->body);

    // save CSeq of establising INVITE
    est_invite_cseq = dlg.cseq - 1;
    est_invite_other_cseq = co_ev->r_cseq;

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
