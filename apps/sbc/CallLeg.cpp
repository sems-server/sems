#include "CallLeg.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "ampi/MonitoringAPI.h"
#include "AmSipHeaders.h"
#include "AmUtils.h"
#include "AmRtpReceiver.h"

// helper functions

static const char *callStatus2str(const CallLeg::CallStatus state)
{
  static const char *disconnected = "Disconnected";
  static const char *noreply = "NoReply";
  static const char *ringing = "Ringing";
  static const char *connected = "connected";
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
  call_status(Disconnected)
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

  call_status = Disconnected; // no B legs should be remaining
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

void CallLeg::onBLegRefused(const AmSipReply& reply)
{
  terminateOtherLeg(reply.from_tag);
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
        call_status = Ringing;
        other_id = reply.from_tag;
        DBG("1xx reply received in NoReply state, changing status to Ringing and remembering the other leg ID (%s)\n", other_id.c_str());
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
      }
    } else if (reply.code < 300) { // 2xx replies
      call_status = Connected;
      other_id = reply.from_tag;
      INFO("setting call status to connected with leg %s\n", other_id.c_str());

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
      INFO("connecting media session: %s to %s\n", dlg.local_tag.c_str(), other_id.c_str());

      onCallConnected(reply);
      set_sip_relay_only(true); // relay only from now on
    } else { // 3xx-6xx replies
      if (other_id == reply.from_tag) clear_other();

      // clean up the other leg; 
      // eventually do serial fork, handle redirect or whatever else
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
    }
      
    // FIXME: really call only for initial INVITE?
    // FIXME: eliminate this call because onOtherReply is a bit foggy and rather
    // redefine onB2BEvent than introducing a half-defined method
    // if (!processed) processed = onOtherReply(reply);
    
    // relay current reply upstream to let know that we are
    // ringing, connected or terminated
    ev->forward = true; // hack which could work?
    AmB2BSession::onB2BEvent(ev);
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

// was for caller only
void CallLeg::onInvite(const AmSipRequest& req)
{
  // do not call AmB2BSession::onInvite(req); we changed the behavior

  if (call_status == Disconnected) { // for initial INVITE only
    est_invite_cseq = req.cseq; // remember initial CSeq
    // initialize RTP relay

    // relayed INVITE - we need to add the original INVITE to
    // list of received (relayed) requests
    recvd_req.insert(std::make_pair(req.cseq, req));

    if (rtp_relay_mode == RTP_Relay) {
      // TODO: setMediaSession(new AmB2BMedia(this, NULL));

      AmSdp sdp;
      const AmMimeBody* sdp_body = req.body.hasContentType(SIP_APPLICATION_SDP);
      DBG("SDP %sfound in initial INVITE\n", sdp_body ? "": "not ");
      if (sdp_body && (sdp.parse((const char *)sdp_body->getPayload()) == 0)) {
        DBG("updating remote SDP\n");
        updateRemoteSdp(sdp);
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

// was for caller only
void CallLeg::onCancel(const AmSipRequest& req)
{
  // initial INVITE handling
  if (a_leg) terminateNotConnectedLegs();

  // FIXME: expected for CANCELed re-INVITEs (in both directions)?
  terminateOtherLeg();
  terminateLeg();
}

void CallLeg::terminateLeg()
{
  AmB2BSession::terminateLeg();
  call_status = Disconnected;
}

// was for caller only
void CallLeg::onRemoteDisappeared(const AmSipReply& reply) {
  DBG("remote unreachable, ending B2BUA call\n");
  // FIXME: shouldn't be cleared in AmB2BSession as well?
  clearRtpReceiverRelay();

  AmB2BSession::onRemoteDisappeared(reply);
}

// was for caller only
void CallLeg::onBye(const AmSipRequest& req)
{
  // FIXME: shouldn't be cleared in AmB2BSession as well?
  clearRtpReceiverRelay();
  AmB2BSession::onBye(req);
}
    
void CallLeg::addCallee(CallLeg *callee, const AmSipRequest &relayed_invite)
{
  BLegInfo b;
  b.id = callee->getLocalTag();

  if ((rtp_relay_mode == RTP_Relay)) {
    // FIXME: do not initialise the media session with A leg to avoid
    // unnecessary A leg RTP stream creation in every B leg's media session
    b.media_session = new AmB2BMedia(this, callee);
    b.media_session->addReference(); // new reference for me
    callee->setMediaSession(b.media_session);
  }
  else b.media_session = NULL;
  b_legs.push_back(b);

  if (AmConfig::LogSessions) {
    INFO("Starting B2B callee session %s\n",
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

  if (call_status == Disconnected) call_status = NoReply;
}

