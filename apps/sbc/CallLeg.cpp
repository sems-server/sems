#include "CallLeg.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "ampi/MonitoringAPI.h"
#include "AmSipHeaders.h"
#include "AmUtils.h"
#include "AmRtpReceiver.h"

#define TRACE INFO

// helper functions

static const char *callStatus2str(const CallLeg::CallStatus state)
{
  static const char *disconnected = "Disconnected";
  static const char *noreply = "NoReply";
  static const char *ringing = "Ringing";
  static const char *connected = "Connected";
  static const char *unknown = "???";

  switch (state) {
    case CallLeg::Disconnected: return disconnected;
    case CallLeg::NoReply: return noreply;
    case CallLeg::Ringing: return ringing;
    case CallLeg::Connected: return connected;
  }

  return unknown;
}

////////////////////////////////////////////////////////////////////////////////

#if 0
// callee
CallLeg::CallLeg(const string& other_local_tag): 
  AmB2BSession(other_local_tag),
  call_status(Disconnected)
{
  a_leg = false;
  // TODO: copy RTP settings
}
#endif

// callee
CallLeg::CallLeg(const CallLeg* caller):
  AmB2BSession(caller->getLocalTag()),
  call_status(NoReply) // we already have the other leg
{
  a_leg = false;

  // B leg is marked as 'relay only' since the beginning because it might
  // need not now on time that it is connected and thus should relay. 
  //
  // For example: B leg received 2xx reply, relayed it to A leg and is
  // immediatelly processing in-dialog request which should be relayed, but
  // A leg didn't have chance to process the relayed reply so the B leg is not
  // connected to the A leg already when handling the in-dialog request
  //
  // FIXME: can we really relay from all B legs since the beginning?
  set_sip_relay_only(true);

  // code below taken from createCalleeSession

  const AmSipDialog& caller_dlg = caller->dlg;

  dlg.local_tag    = AmSession::getNewId();
  dlg.callid       = AmSession::getNewId();

  // take important data from A leg
  dlg.local_party  = caller_dlg.remote_party;
  dlg.remote_party = caller_dlg.local_party;
  dlg.remote_uri   = caller_dlg.local_uri;

/*  if (AmConfig::LogSessions) {
    INFO("Starting B2B callee session %s\n",
	 getLocalTag().c_str());
  }

  MONITORING_LOG4(other_id.c_str(), 
		  "dir",  "out",
		  "from", dlg.local_party.c_str(),
		  "to",   dlg.remote_party.c_str(),
		  "ruri", dlg.remote_uri.c_str());
*/

  // copy common RTP relay settings from A leg
  //initRTPRelay(caller);
  vector<SdpPayload> lowfi_payloads;
  setRtpRelayMode(caller->getRtpRelayMode());
  setEnableDtmfTranscoding(caller->getEnableDtmfTranscoding());
  caller->getLowFiPLs(lowfi_payloads);
  setLowFiPLs(lowfi_payloads);
}

// caller
CallLeg::CallLeg(): 
  AmB2BSession(),
  call_status(Disconnected)
{
  a_leg = true;

  // At least in the first version we start relaying after the call is fully
  // established.  This is because of forking possibility - we can't simply
  // relay if we have one A leg and multiple B legs.
  // It is possible to start relaying before call is established if we have
  // exactly one B leg (i.e. no parallel fork happened).
  set_sip_relay_only(false);
}
    
void CallLeg::terminateOtherLeg()
{
  if (call_status != Connected) {
    DBG("trying to terminate other leg in %s state -> terminating the others as well\n", callStatus2str(call_status));
    // FIXME: may happen when for example reply forward fails, do we want to terminate
    // all other legs in such case?
    terminateNotConnectedLegs(); // terminates all except the one identified by other_id
  }
  
  AmB2BSession::terminateOtherLeg();

  updateCallStatus(Disconnected); // no B legs should be remaining
}

void CallLeg::terminateOtherLeg(const string &id)
{
  // remove the call leg from list of B legs
  for (vector<BLegInfo>::iterator i = b_legs.begin(); i != b_legs.end(); ++i) {
    if (i->id == id) {
      i->releaseMediaSession();
      b_legs.erase(i);
      AmSessionContainer::instance()->postEvent(id, new B2BEvent(B2BTerminateLeg));
      return;
    }
  }
  DBG("trying to terminate other leg which is not in the list of legs, might be terminated already\n");
}

void CallLeg::terminateNotConnectedLegs()
{
  bool found = false;
  BLegInfo b;

  for (vector<BLegInfo>::iterator i = b_legs.begin(); i != b_legs.end(); ++i) {
    if (i->id != other_id) {
      i->releaseMediaSession();
      AmSessionContainer::instance()->postEvent(i->id, new B2BEvent(B2BTerminateLeg));
    }
    else {
      found = true; // other_id is there
      b = *i;
    }
  }

  // quick hack to remove all terminated entries from the list
  b_legs.clear();
  if (found) b_legs.push_back(b);
}

// composed for caller and callee already
void CallLeg::onB2BEvent(B2BEvent* ev)
{
  // was handled for caller only
  if (ev->event_id == B2BSipReply) {
    onB2BReply(dynamic_cast<B2BSipReplyEvent*>(ev));
    return;
  }

  // was only for callee:
  if (ev->event_id == ConnectLeg) {
    onB2BConnect(dynamic_cast<ConnectLegEvent*>(ev));
    return;
  }

  if (ev->event_id == ReconnectLeg) {
    onB2BReconnect(dynamic_cast<ReconnectLegEvent*>(ev));
    return;
  }

  if (ev->event_id == B2BSipRequest) {
    if (a_leg && (!sip_relay_only)) {
      // disable forwarding of relayed request if we are not connected [yet]
      // (only we known that, the B leg has just delayed information about being
      // connected to us and thus it can't set)
      // Need not to be done if we have only one possible B leg so instead of
      // checking call_status we can check if sip_relay_only is set or not
      B2BSipRequestEvent *req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
      if (req_ev) req_ev->forward = false;
    }
  }

  AmB2BSession::onB2BEvent(ev);
}

int CallLeg::relaySipReply(AmSipReply &reply)
{
  std::map<int,AmSipRequest>::iterator t_req = recvd_req.find(reply.cseq);

  if (t_req == recvd_req.end()) {
    ERROR("Request with CSeq %u not found in recvd_req.\n", reply.cseq);
    return 0; // ignore?
  }

  int res;

  if ((reply.code >= 300) && (reply.code <= 305) && !reply.contact.empty()) {
    // relay with Contact in 300 - 305 redirect messages
    AmSipReply n_reply(reply);
    n_reply.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) + reply.contact + CRLF;

    res = relaySip(t_req->second, n_reply);
  }
  else res = relaySip(t_req->second, reply) < 0; // relay response directly

  if (reply.code >= 200){
    DBG("recvd_req.erase(<%u,%s>)\n", t_req->first, t_req->second.method.c_str());
    recvd_req.erase(t_req);
  }
  return res;
}

void CallLeg::terminateCall()
{
  terminateNotConnectedLegs();
  terminateOtherLeg();
  terminateLeg();
}

// was for caller only
void CallLeg::onB2BReply(B2BSipReplyEvent *ev)
{
  if (!ev) {
    ERROR("BUG: invalid argument given\n");
    return;
  }

  AmSipReply& reply = ev->reply;

  // FIXME: do we wat to have the check below? multiple other legs are
  // possible so the check can stay as it is for Connected state but for other
  // should check other_legs instead of other_id
#if 0
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
#endif

  DBG("%u %s reply received from other leg\n", reply.code, reply.reason.c_str());

  // A leg handles relayed initial replies specific way
  if (a_leg && (reply.cseq == est_invite_cseq)) {
    // handle only replies to the original INVITE (CSeq is really the same?)
    
    // do not handle replies to initial request in states where it has no effect
    if ((call_status == Connected) || (call_status == Disconnected)) {
      DBG("ignoring reply in %s state\n", callStatus2str(call_status));
      return;
    }

    // TODO: stop processing of 100 reply here or add Trying state to handle it
    // without remembering other_id

    if (reply.code < 200) { // 1xx replies
      if (call_status == NoReply) {
        if (relaySipReply(reply) != 0) {
          terminateCall();
          return;
        }
        if (!reply.to_tag.empty()) {
          other_id = reply.from_tag;
          TRACE("1xx reply with to-tag received in NoReply state, changing status to Ringing and remembering the other leg ID (%s)\n", other_id.c_str());
          if (relaySipReply(reply) != 0) {
            terminateCall();
            return;
          }
          updateCallStatus(Ringing);
        }
      }
      else {
        if (other_id != reply.from_tag) {
           // in Ringing state but the reply comes from another B leg than
           // previous 1xx reply => do not relay or process other way
          DBG("1xx reply received in %s state from another B leg, ignoring\n", callStatus2str(call_status));
          return;
        }
        // we can relay this reply because it is from the same B leg from which
        // we already relayed something
        // FIXME: but we shouldn't relay the body until we are connected because
        // fork still can happen, right? (so no early media support? or we would
        // just destroy the before-fork-media-session and use the
        // after-fork-one? but problem could be with two B legs trying to do
        // early media)
        if (!sip_relay_only && !reply.body.empty()) {
          DBG("not going to relay 1xx body\n");
          static const AmMimeBody empty_body;
          reply.body = empty_body;
        }
        if (relaySipReply(reply) != 0) {
          terminateCall();
          return;
        }
      }
    } else if (reply.code < 300) { // 2xx replies
      other_id = reply.from_tag;
      TRACE("setting call status to connected with leg %s\n", other_id.c_str());

      // terminate all other legs than the connected one (determined by other_id)
      terminateNotConnectedLegs();

      if (b_legs.empty()) {
        ERROR("BUG: connected but there is no B leg remaining\n");
        setStopped();
        return;
      }

      // connect media with the other leg if RTP relay is enabled
      setMediaSession(b_legs.begin()->media_session);
      b_legs.begin()->releaseMediaSession(); // remove reference hold by BLegInfo
      b_legs.clear(); // no need to remember the connected leg here
      if (media_session) {
        TRACE("connecting media session: %s to %s\n", dlg.local_tag.c_str(), other_id.c_str());
        media_session->changeSession(a_leg, this);
        if (initial_sdp_stored) updateRemoteSdp(initial_sdp);
      }

      onCallConnected(reply);
      set_sip_relay_only(true); // relay only from now on

      if (relaySipReply(reply) != 0) {
        terminateCall();
        return;
      }
      updateCallStatus(Connected);
    } else { // 3xx-6xx replies
      if (other_id == reply.from_tag) clear_other();

      // clean up the other leg; 
      // eventually do serial fork, handle redirect or whatever else
      terminateOtherLeg(reply.from_tag);

      onBLegRefused(reply);

      if (!b_legs.empty()) {
        // there are other B legs for us, wait for their responses and do not
        // relay current response
        return;
      }

      // FIXME: call this from ternimateLeg or let the successors override terminateLeg instead?
      onCallStopped();

      // no other B legs, terminate
      setStopped(); // would be called in onOtherReply

      relaySipReply(reply);
      updateCallStatus(Disconnected);
    }

    return; // relayed reply to initial request is processed
  }

  // TODO: handle call transfers (i.e. the other leg is reINVITing and we should
  // reINVITE as well according to its result)

  // handle replies to other requests than the initial one
  DBG("handling reply via AmB2BSession\n");
  AmB2BSession::onB2BEvent(ev);
}

// TODO: original callee's version, update
void CallLeg::onB2BConnect(ConnectLegEvent* co_ev)
{
  if (!co_ev) {
    ERROR("BUG: invalid argument given\n");
    return;
  }

  MONITORING_LOG3(getLocalTag().c_str(), 
      "b2b_leg", other_id.c_str(),
      "to", dlg.remote_party.c_str(),
      "ruri", dlg.remote_uri.c_str());


  AmMimeBody r_body(co_ev->body);
  const AmMimeBody* body = &co_ev->body;
  if (rtp_relay_mode == RTP_Relay) {
    try {
      body = co_ev->body.hasContentType(SIP_APPLICATION_SDP);
      if (body && updateLocalBody(*body, *r_body.hasContentType(SIP_APPLICATION_SDP))) {
        body = &r_body;
      }
      else {
        body = &co_ev->body;
      }
    } catch (const string& s) {
      relayError(SIP_METH_INVITE, co_ev->r_cseq, true, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      throw;
    }
  }

  int res = dlg.sendRequest(SIP_METH_INVITE, body,
      co_ev->hdrs, SIP_FLAGS_VERBATIM);
  if (res < 0) {
    DBG("sending INVITE failed, relaying back error reply\n");
    relayError(SIP_METH_INVITE, co_ev->r_cseq, true, res);

    setStopped();
    return;
  }
  
  // always relayed INVITE - store it
  relayed_req[dlg.cseq - 1] = AmSipTransaction(SIP_METH_INVITE, co_ev->r_cseq, trans_ticket());

  if (refresh_method != REFRESH_UPDATE)
    saveSessionDescription(co_ev->body);

  // save CSeq of establising INVITE
  est_invite_cseq = dlg.cseq - 1;
  est_invite_other_cseq = co_ev->r_cseq;
}

void CallLeg::onB2BReconnect(ReconnectLegEvent* ev)
{
  if (!ev) {
    ERROR("BUG: invalid argument given\n");
    return;
  }

  // release old signaling and media session
  terminateOtherLeg();
  clearRtpReceiverRelay();

  other_id = ev->session_tag;
  a_leg = false; // we are B leg regardless what we were up to now
  // FIXME: What about calling SBC CC modules in this case? Original CC
  // interface is called from A leg only and it might happen that we were call
  // leg A before.


  // use new media session if given
  if (ev->media) {
    rtp_relay_mode = RTP_Relay;
    setMediaSession(ev->media);
    media_session->changeSession(a_leg, this);
  }
  else rtp_relay_mode = RTP_Direct;

  MONITORING_LOG3(getLocalTag().c_str(),
      "b2b_leg", other_id.c_str(),
      "to", dlg.remote_party.c_str(),
      "ruri", dlg.remote_uri.c_str());

  AmMimeBody r_body(ev->body);
  const AmMimeBody* body = &ev->body;
  if (rtp_relay_mode == RTP_Relay) {
    try {
      body = ev->body.hasContentType(SIP_APPLICATION_SDP);
      if (body && updateLocalBody(*body, *r_body.hasContentType(SIP_APPLICATION_SDP))) {
        body = &r_body;
      }
      else {
        body = &ev->body;
      }
    } catch (const string& s) {
      relayError(SIP_METH_INVITE, ev->r_cseq, true, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      throw;
    }
  }

  // generate re-INVITE
  int res = dlg.sendRequest(SIP_METH_INVITE, body, ev->hdrs, SIP_FLAGS_VERBATIM);
  if (res < 0) {
    DBG("sending re-INVITE failed, relaying back error reply\n");
    relayError(SIP_METH_INVITE, ev->r_cseq, true, res);

    setStopped();
    return;
  }

  // always relayed INVITE - store it
  relayed_req[dlg.cseq - 1] = AmSipTransaction(SIP_METH_INVITE, ev->r_cseq, trans_ticket());

  if (refresh_method != REFRESH_UPDATE) saveSessionDescription(ev->body);

  // save CSeq of establising INVITE
  est_invite_cseq = dlg.cseq - 1;
  est_invite_other_cseq = ev->r_cseq;
}

// was for caller only
void CallLeg::onInvite(const AmSipRequest& req)
{
  // do not call AmB2BSession::onInvite(req); we changed the behavior
  // this method is not called for re-INVITEs because once connected we are in
  // sip_relay_only mode and the re-INVITEs are relayed instead of processing
  // (see AmB2BSession::onSipRequest)

  if (call_status == Disconnected) { // for initial INVITE only
    est_invite_cseq = req.cseq; // remember initial CSeq
    // initialize RTP relay

    // relayed INVITE - we need to add the original INVITE to
    // list of received (relayed) requests
    recvd_req.insert(std::make_pair(req.cseq, req));

    initial_sdp_stored = false;
    if (rtp_relay_mode == RTP_Relay) {
      const AmMimeBody* sdp_body = req.body.hasContentType(SIP_APPLICATION_SDP);
      DBG("SDP %sfound in initial INVITE\n", sdp_body ? "": "not ");
      if (sdp_body && (initial_sdp.parse((const char *)sdp_body->getPayload()) == 0)) {
        DBG("storing remote SDP for later\n");
        initial_sdp_stored = true;
      }
    }
  }
}

void CallLeg::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status)
{
  AmB2BSession::onSipReply(reply, old_dlg_status);

  // update internal state and call related callbacks based on received reply
  // (i.e. B leg in case of initial INVITE)
  if ((reply.cseq == est_invite_cseq) && (reply.cseq_method == SIP_METH_INVITE)) {
    // reply to the initial request
    if ((reply.code > 100) && (reply.code < 200)) {
      if (((call_status == Disconnected) || (call_status == NoReply)) && (!reply.to_tag.empty()))
        updateCallStatus(Ringing);
    }
    else if ((reply.code >= 200) && (reply.code < 300)) {
      if (call_status != Connected) {
        onCallConnected(reply);
        updateCallStatus(Connected);
      }
    }
    else if (reply.code >= 300) {
      if ((call_status != Disconnected) || (dlg.getStatus() != old_dlg_status)) {
        // in case of canceling it happens that B leg is already in Disconnected
        // status (terminateLeg called) but later comes the 487 reply and
        // updates dlg, we need to call callbacks on that change!
        updateCallStatus(Disconnected);
      }
    }
  }

}

// was for caller only
void CallLeg::onInvite2xx(const AmSipReply& reply)
{
  // We don't want to remember reply.cseq as est_invite_cseq, do we? It was in
  // AmB2BCallerSession but we already have initial INVITE cseq remembered and
  // we don't need to change it to last reINVITE one, right? Otherwise we should
  // remember UPDATE cseq as well because SDP may change by it as well (used
  // when handling B2BSipReply in AmB2BSession to check if reINVITE should be
  // sent).
  // 
  // est_invite_cseq = reply.cseq;

  // we don't want to handle the 2xx using AmSession so the following may be
  // unwanted for us:
  // 
  // AmB2BSession::onInvite2xx(reply);
}

void CallLeg::onCancel(const AmSipRequest& req)
{
  // initial INVITE handling
  if ((call_status == Ringing) || (call_status == NoReply)) {
    if (a_leg) {
      // terminate whole B2B call if the caller receives CANCEL
      terminateNotConnectedLegs();
      terminateOtherLeg();
      terminateLeg();
    }
    // else { } ... ignore for B leg
  }
  // FIXME: was it really expected to terminate the call for CANCELed re-INVITEs
  // (in both directions) as well?
}

void CallLeg::terminateLeg()
{
  AmB2BSession::terminateLeg();
  if (call_status != Disconnected) updateCallStatus(Disconnected);
}

// was for caller only
void CallLeg::onRemoteDisappeared(const AmSipReply& reply) 
{
  if (call_status == Connected) {
    // only in case we are really connected (called on timeout or 481 from the remote)

    DBG("remote unreachable, ending B2BUA call\n");
    clearRtpReceiverRelay(); // FIXME: shouldn't be cleared in AmB2BSession as well?
    AmB2BSession::onRemoteDisappeared(reply); // terminates the other leg
    updateCallStatus(Disconnected);
  }
}

// was for caller only
void CallLeg::onBye(const AmSipRequest& req)
{
  clearRtpReceiverRelay(); // FIXME: shouldn't be cleared in AmB2BSession as well?
  AmB2BSession::onBye(req);
  updateCallStatus(Disconnected); // shouldn't we wait for BYE response?
}

void CallLeg::addCallee(CallLeg *callee, const AmSipRequest &relayed_invite)
{
  BLegInfo b;
  b.id = callee->getLocalTag();

  if ((rtp_relay_mode == RTP_Relay)) {
    // do not initialise the media session with A leg to avoid unnecessary A leg
    // RTP stream creation in every B leg's media session
    b.media_session = new AmB2BMedia(NULL, callee);
    b.media_session->addReference(); // new reference for me
    callee->setMediaSession(b.media_session);
  }
  else b.media_session = NULL;
  b_legs.push_back(b);

  if (AmConfig::LogSessions) {
    TRACE("Starting B2B callee session %s\n",
	 callee->getLocalTag().c_str()/*, invite_req.cmd.c_str()*/);
  }

  AmSipDialog& callee_dlg = callee->dlg;
  MONITORING_LOG4(b.id.c_str(),
		  "dir",  "out",
		  "from", callee_dlg.local_party.c_str(),
		  "to",   callee_dlg.remote_party.c_str(),
		  "ruri", callee_dlg.remote_uri.c_str());

  callee->start();

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(b.id, callee);

  // generate connect event to the newly added leg
  DBG("relaying connect leg event to the B leg\n");
  // other stuff than relayed INVITE should be set directly when creating callee
  // (remote_uri, remote_party is not propagated and thus B2BConnectEvent is not
  // used because it would just overwrite already set things. Note that in many
  // classes derived from AmB2BCaller[Callee]Session was a lot of things set
  // explicitly)
  AmSessionContainer::instance()->postEvent(b.id, new ConnectLegEvent(relayed_invite));

  if (call_status == Disconnected) updateCallStatus(NoReply);
}

void CallLeg::updateCallStatus(CallStatus new_status)
{
  TRACE("%s leg changing status from %s to %s\n", 
      a_leg ? "A" : "B",
      callStatus2str(call_status),
      callStatus2str(new_status));

  call_status = new_status;
  onCallStatusChange();
}

void CallLeg::addCallee(const string &session_tag, const AmSipRequest &relayed_invite)
{
  // add existing session as our B leg

  BLegInfo b;
  b.id = session_tag;
  if ((rtp_relay_mode == RTP_Relay)) {
    // do not initialise the media session with A leg to avoid unnecessary A leg
    // RTP stream creation in every B leg's media session
    b.media_session = new AmB2BMedia(NULL, NULL);
    b.media_session->addReference(); // new reference for me
  }
  else b.media_session = NULL;

  // generate connect event to the newly added leg
  TRACE("relaying re-connect leg event to the B leg\n");
  ReconnectLegEvent *ev = new ReconnectLegEvent(getLocalTag(), relayed_invite, b.media_session);
  // TODO: what about the RTP relay and other settings? send them as well?
  if (!AmSessionContainer::instance()->postEvent(session_tag, ev)) {
    // session doesn't exist - can't connect
    INFO("the B leg to connect to (%s) doesn't exist\n", session_tag.c_str());
    if (b.media_session) delete b.media_session;
    return;
  }

  b_legs.push_back(b);
  if (call_status == Disconnected) updateCallStatus(NoReply);

  // TODO: start a timer here to handle the case when the other leg can't reply
  // the reconnect event
}
