#include "CallLeg.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "ampi/MonitoringAPI.h"
#include "AmSipHeaders.h"
#include "AmUtils.h"
#include "AmRtpReceiver.h"

#define TRACE DBG

// helper functions

static const char *callStatus2str(const CallLeg::CallStatus state)
{
  static const char *disconnected = "Disconnected";
  static const char *disconnecting = "Disconnecting";
  static const char *noreply = "NoReply";
  static const char *ringing = "Ringing";
  static const char *connected = "Connected";
  static const char *unknown = "???";

  switch (state) {
    case CallLeg::Disconnected: return disconnected;
    case CallLeg::Disconnecting: return disconnecting;
    case CallLeg::NoReply: return noreply;
    case CallLeg::Ringing: return ringing;
    case CallLeg::Connected: return connected;
  }

  return unknown;
}

ReliableB2BEvent::~ReliableB2BEvent()
{
  TRACE("reliable event was %sprocessed, sending %p to %s\n",
      processed ? "" : "NOT ",
      processed ? processed_reply : unprocessed_reply,
      sender.c_str());
  if (processed) {
    if (unprocessed_reply) delete unprocessed_reply;
    if (processed_reply) AmSessionContainer::instance()->postEvent(sender, processed_reply);
  }
  else {
    if (processed_reply) delete processed_reply;
    if (unprocessed_reply) AmSessionContainer::instance()->postEvent(sender, unprocessed_reply);
  }
}

////////////////////////////////////////////////////////////////////////////////

// callee
CallLeg::CallLeg(const CallLeg* caller):
  AmB2BSession(caller->getLocalTag()),
  call_status(Disconnected),
  hold_request_cseq(0)
{
  a_leg = !caller->a_leg; // we have to be the complement

  set_sip_relay_only(false); // will be changed later on (for now we have no peer so we can't relay)

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
  call_status(Disconnected),
  hold_request_cseq(0)
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

  // FIXME: call disconnect if connected (to put remote on hold)?
  updateCallStatus(Disconnected); // no B legs should be remaining
}

void CallLeg::terminateNotConnectedLegs()
{
  bool found = false;
  OtherLegInfo b;

  for (vector<OtherLegInfo>::iterator i = other_legs.begin(); i != other_legs.end(); ++i) {
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
  other_legs.clear();
  if (found) other_legs.push_back(b);
}

void CallLeg::removeOtherLeg(const string &id)
{
  if (other_id == id) other_id.clear();

  // remove the call leg from list of B legs
  for (vector<OtherLegInfo>::iterator i = other_legs.begin(); i != other_legs.end(); ++i) {
    if (i->id == id) {
      i->releaseMediaSession();
      other_legs.erase(i);
      break;
    }
  }

  /*if (terminate) AmSessionContainer::instance()->postEvent(id, new B2BEvent(B2BTerminateLeg));*/
}

// composed for caller and callee already
void CallLeg::onB2BEvent(B2BEvent* ev)
{
  switch (ev->event_id) {

    case B2BSipReply:
      onB2BReply(dynamic_cast<B2BSipReplyEvent*>(ev));
      break;

    case ConnectLeg:
      onB2BConnect(dynamic_cast<ConnectLegEvent*>(ev));
      break;

    case ReconnectLeg:
      onB2BReconnect(dynamic_cast<ReconnectLegEvent*>(ev));
      break;

    case ReplaceLeg:
      onB2BReplace(dynamic_cast<ReplaceLegEvent*>(ev));
      break;

    case ReplaceInProgress:
      onB2BReplaceInProgress(dynamic_cast<ReplaceInProgressEvent*>(ev));
      break;

    case DisconnectLeg:
      if (dynamic_cast<DisconnectLegEvent*>(ev)) disconnect(true);
      break;

    case B2BSipRequest:
      if (!sip_relay_only) {
        // disable forwarding of relayed request if we are not connected [yet]
        // (only we known that, the B leg has just delayed information about being
        // connected to us and thus it can't set)
        // Need not to be done if we have only one possible B leg so instead of
        // checking call_status we can check if sip_relay_only is set or not
        B2BSipRequestEvent *req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
        if (req_ev) req_ev->forward = false;
      }
      // continue handling in AmB2bSession

    default:
      AmB2BSession::onB2BEvent(ev);
  }
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

// was for caller only
void CallLeg::onB2BReply(B2BSipReplyEvent *ev)
{
  if (!ev) {
    ERROR("BUG: invalid argument given\n");
    return;
  }

  AmSipReply& reply = ev->reply;

  TRACE("%s: B2B SIP reply %d/%d %s received in %s state\n",
      getLocalTag().c_str(),
      reply.code, reply.cseq, reply.cseq_method.c_str(),
      callStatus2str(call_status));

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

  // handle relayed initial replies specific way
  // testing est_invite_cseq is wrong! (checking in what direction or what role
  // would be needed)
  if (reply.cseq_method == SIP_METH_INVITE &&
      (call_status == NoReply || call_status == Ringing) &&
      ((reply.cseq == est_invite_cseq && ev->forward) ||
       (!ev->forward))) { // connect not related to initial INVITE
    // handle only replies to the original INVITE (CSeq is really the same?)

    TRACE("established CSeq: %d, forward: %s\n", est_invite_cseq, ev->forward ? "yes": "no");

    // TODO: stop processing of 100 reply here or add Trying state to handle it
    // without remembering other_id

    if (reply.code < 200) { // 1xx replies
      if (call_status == NoReply) {
        if (ev->forward && relaySipReply(reply) != 0) {
          stopCall();
          return;
        }
        if (!reply.to_tag.empty()) {
          other_id = reply.from_tag;
          TRACE("1xx reply with to-tag received in NoReply state, changing status to Ringing and remembering the other leg ID (%s)\n", other_id.c_str());
          if (ev->forward && relaySipReply(reply) != 0) {
            stopCall();
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
        if (ev->forward && relaySipReply(reply) != 0) {
          stopCall();
          return;
        }
      }
    } else if (reply.code < 300) { // 2xx replies
      other_id = reply.from_tag;
      TRACE("setting call status to connected with leg %s\n", other_id.c_str());

      // terminate all other legs than the connected one (determined by other_id)
      terminateNotConnectedLegs();

      if (other_legs.empty()) {
        ERROR("BUG: connected but there is no B leg remaining\n");
        stopCall();
        return;
      }

      // connect media with the other leg if RTP relay is enabled
      clearRtpReceiverRelay(); // release old media session if set
      setMediaSession(other_legs.begin()->media_session);
      other_legs.begin()->releaseMediaSession(); // remove reference hold by OtherLegInfo
      other_legs.clear(); // no need to remember the connected leg here
      if (media_session) {
        TRACE("connecting media session: %s to %s\n", dlg.local_tag.c_str(), other_id.c_str());
        media_session->changeSession(a_leg, this);
        if (initial_sdp_stored && ev->forward) updateRemoteSdp(initial_sdp);
      }

      onCallConnected(reply);
      set_sip_relay_only(true); // relay only from now on

      if (!ev->forward) {
        // we need to generate re-INVITE based on received SDP
        saveSessionDescription(reply.body);
        sendEstablishedReInvite();
      }
      else if (relaySipReply(reply) != 0) {
        stopCall();
        return;
      }
      updateCallStatus(Connected);
    } else { // 3xx-6xx replies
      removeOtherLeg(reply.from_tag); // we don't care about this leg any more
      onBLegRefused(reply); // possible serial fork here

      // there are other B legs for us => wait for their responses and do not
      // relay current response
      if (!other_legs.empty()) return;

      if (ev->forward) relaySipReply(reply);

      // no other B legs, terminate
      updateCallStatus(Disconnected);
      stopCall();
    }

    return; // relayed reply to initial request is processed
  }

  // reply not from our peer (might be one of the discarded ones)
  if (other_id != reply.from_tag) {
    TRACE("ignoring reply from %s in %s state\n", reply.from_tag.c_str(), callStatus2str(call_status));
    return;
  }

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

  if (call_status != Disconnected) {
    ERROR("BUG: ConnectLegEvent received in %s state\n", callStatus2str(call_status));
    return;
  }

  MONITORING_LOG3(getLocalTag().c_str(), 
      "b2b_leg", other_id.c_str(),
      "to", dlg.remote_party.c_str(),
      "ruri", dlg.remote_uri.c_str());

  // This leg is marked as 'relay only' since the beginning because it might
  // need not to know on time that it is connected and thus should relay.
  //
  // For example: B leg received 2xx reply, relayed it to A leg and is
  // immediatelly processing in-dialog request which should be relayed, but
  // A leg didn't have chance to process the relayed reply so the B leg is not
  // connected to the A leg yet when handling the in-dialog request.
  set_sip_relay_only(true); // we should relay everything to the other leg from now

  updateCallStatus(NoReply);

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

    stopCall();
    return;
  }

  if (co_ev->relayed_invite) {
    relayed_req[dlg.cseq - 1] = AmSipTransaction(SIP_METH_INVITE, co_ev->r_cseq, trans_ticket());
    est_invite_other_cseq = co_ev->r_cseq;
  }
  else est_invite_other_cseq = 0;

  if (refresh_method != REFRESH_UPDATE)
    saveSessionDescription(co_ev->body);

  // save CSeq of establising INVITE
  est_invite_cseq = dlg.cseq - 1;
}

void CallLeg::onB2BReconnect(ReconnectLegEvent* ev)
{
  if (!ev) {
    ERROR("BUG: invalid argument given\n");
    return;
  }
  TRACE("handling ReconnectLegEvent, other: %s, connect to %s\n", other_id.c_str(), ev->session_tag.c_str());

  ev->markAsProcessed();

  // release old signaling and media session
  terminateOtherLeg();
  clearRtpReceiverRelay();

  other_id = ev->session_tag;
  if (ev->role == ReconnectLegEvent::A) a_leg = true;
  else a_leg = false;
  // FIXME: What about calling SBC CC modules in this case? Original CC
  // interface is called from A leg only and it might happen that we were call
  // leg A before.

  set_sip_relay_only(true); // we should relay everything to the other leg from now
  updateCallStatus(NoReply);

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

    stopCall();
    return;
  }

  if (ev->relayed_invite) {
    relayed_req[dlg.cseq - 1] = AmSipTransaction(SIP_METH_INVITE, ev->r_cseq, trans_ticket());
    est_invite_other_cseq = ev->r_cseq;
  }
  else est_invite_other_cseq = 0;

  if (refresh_method != REFRESH_UPDATE) saveSessionDescription(ev->body);

  // save CSeq of establising INVITE
  est_invite_cseq = dlg.cseq - 1;
}

void CallLeg::onB2BReplace(ReplaceLegEvent *e)
{
  if (!e) {
    ERROR("BUG: invalid argument given\n");
    return;
  }
  e->markAsProcessed();

  ReconnectLegEvent *reconnect = e->getReconnectEvent();
  if (!reconnect) {
    ERROR("BUG: invalid ReconnectLegEvent\n");
    return;
  }

  TRACE("handling ReplaceLegEvent, other: %s, connect to %s\n", other_id.c_str(), reconnect->session_tag.c_str());

  string id(other_id);
  if (id.empty()) {
    // try it with the first B leg?
    if (other_legs.empty()) {
      ERROR("BUG: there is no B leg to connect our replacement to\n");
      return;
    }
    id = other_legs[0].id;
  }

  // send session ID of the other leg to the originator
  AmSessionContainer::instance()->postEvent(reconnect->session_tag, new ReplaceInProgressEvent(id));

  // send the ReconnectLegEvent to the other leg
  AmSessionContainer::instance()->postEvent(id, reconnect);

  // remove the B leg from our B leg list
  removeOtherLeg(id);

  // commit suicide if our last B leg is stolen
  if (other_legs.empty() && other_id.empty()) stopCall();
}

void CallLeg::onB2BReplaceInProgress(ReplaceInProgressEvent *e)
{
  for (vector<OtherLegInfo>::iterator i = other_legs.begin(); i != other_legs.end(); ++i) {
    if (i->id.empty()) {
      // replace the temporary (invalid) session with the correct one
      i->id = e->dst_session;
      return;
    }
  }
}

void CallLeg::disconnect(bool hold_remote)
{
  TRACE("disconnecting call leg %s from the other\n", getLocalTag().c_str());

  switch (call_status) {
    case Disconnecting:
    case Disconnected:
      DBG("trying to disconnect already disconnected (or disconnecting) call leg\n");
      return;

    case NoReply:
    case Ringing:
      WARN("trying to disconnect in not connected state, terminating not connected legs in advance (was it intended?)\n");
      terminateNotConnectedLegs();
      break;

    case Connected:
      clearRtpReceiverRelay(); // we can't stay connected (at media level) with the other leg
      break; // this is OK
  }

  clear_other();
  set_sip_relay_only(false); // we can't relay once disconnected

  // put the remote on hold (we have no 'other leg', we can do what we want)
  if (!hold_remote) {
    updateCallStatus(Disconnected);
    return;
  }

  // FIXME: throw this out?
  if (media_session && media_session->isOnHold(a_leg)) {
    updateCallStatus(Disconnected);
    return; // already on hold
  }

  TRACE("putting remote on hold\n");

  updateCallStatus(Disconnecting);

  if (getRtpRelayMode() != AmB2BSession::RTP_Relay)
    setRtpRelayMode(RTP_Relay);

  if (!media_session)
    setMediaSession(new AmB2BMedia(a_leg ? this: NULL, a_leg ? NULL : this));

  AmSdp sdp;
  // FIXME: mark the media line as inactive rather than sendonly?
  media_session->createHoldRequest(sdp, a_leg, false /*mark_zero_connection*/, true /*mark_sendonly*/);
  updateLocalSdp(sdp);

  // generate re-INVITE with hold request
  //reinvite(sdp, hold_request_cseq);
  AmMimeBody body;
  string body_str;
  sdp.print(body_str);
  body.parse(SIP_APPLICATION_SDP, (const unsigned char*)body_str.c_str(), body_str.length());
  if (dlg.reinvite("", &body, SIP_FLAGS_VERBATIM) != 0) {
    ERROR("re-INVITE failed\n");
  }
  else hold_request_cseq = dlg.cseq - 1;
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

void CallLeg::onSipRequest(const AmSipRequest& req)
{
  TRACE("%s: SIP request %d %s received in %s state\n",
      getLocalTag().c_str(),
      req.cseq, req.method.c_str(), callStatus2str(call_status));

  // we need to handle cases if there is no other leg (for example call parking)
  // Note that setting sip_relay_only to false in this case doesn't solve the
  // problem because AmB2BSession always tries to relay the request into the
  // other leg.
  if (getCallStatus() == Disconnected) {
    TRACE("handling request %s in disconnected state", req.method.c_str());
    // this is not correct but what is?
    AmSession::onSipRequest(req);
    if (req.method == SIP_METH_BYE) stopCall(); // is this needed?
  }
  else AmB2BSession::onSipRequest(req);
}

void CallLeg::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool relayed_request = (t != relayed_req.end());

  TRACE("%s: SIP reply %d/%d %s (%s) received in %s state\n",
      getLocalTag().c_str(),
      reply.code, reply.cseq, reply.cseq_method.c_str(),
      (relayed_request ? "to relayed request" : "to locally generated request"),
      callStatus2str(call_status));

  AmB2BSession::onSipReply(reply, old_dlg_status);

  // update internal state and call related callbacks based on received reply
  // (i.e. B leg in case of initial INVITE)
  if (reply.cseq == est_invite_cseq && reply.cseq_method == SIP_METH_INVITE &&
    (call_status == NoReply || call_status == Ringing)) {
    // reply to the initial request
    if ((reply.code > 100) && (reply.code < 200)) {
      if (((call_status == NoReply)) && (!reply.to_tag.empty()))
        updateCallStatus(Ringing);
    }
    else if ((reply.code >= 200) && (reply.code < 300)) {
      onCallConnected(reply);
      updateCallStatus(Connected);
    }
    else if (reply.code >= 300) {
      terminateLeg(); // commit suicide (don't let the master to kill us)
    }
  }

  if ((reply.cseq == hold_request_cseq) && (reply.cseq_method == SIP_METH_INVITE)) {
    // hold request replied
    if (reply.code < 200) { return; /* wait for final reply */ }
    else {
      // ignore the result (if hold request is not accepted that's a pitty but
      // we are Disconnected anyway)
      if (call_status == Disconnecting) updateCallStatus(Disconnected);
      hold_request_cseq = 0;
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
  AmB2BSession::onInvite2xx(reply);
}

void CallLeg::onCancel(const AmSipRequest& req)
{
  // initial INVITE handling
  if ((call_status == Ringing) || (call_status == NoReply)) {
    if (a_leg) {
      // terminate whole B2B call if the caller receives CANCEL
      stopCall();
    }
    // else { } ... ignore for B leg
  }
  // FIXME: was it really expected to terminate the call for CANCELed re-INVITEs
  // (in both directions) as well?
}

void CallLeg::terminateLeg()
{
  AmB2BSession::terminateLeg();
  onCallStopped();
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
}

void CallLeg::addNewCallee(CallLeg *callee, ConnectLegEvent *e)
{
  OtherLegInfo b;
  b.id = callee->getLocalTag();

  if ((rtp_relay_mode == RTP_Relay)) {
    // do not initialise the media session with A leg to avoid unnecessary A leg
    // RTP stream creation in every B leg's media session
    if (a_leg) b.media_session = new AmB2BMedia(NULL, callee);
    else b.media_session = new AmB2BMedia(callee, NULL);
    b.media_session->addReference(); // new reference for me
    callee->setMediaSession(b.media_session);
  }
  else b.media_session = NULL;
  other_legs.push_back(b);

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
  // Warning: correct callee's role must be already set (in constructor or so)
  TRACE("relaying connect leg event to the new leg\n");
  // other stuff than relayed INVITE should be set directly when creating callee
  // (remote_uri, remote_party is not propagated and thus B2BConnectEvent is not
  // used because it would just overwrite already set things. Note that in many
  // classes derived from AmB2BCaller[Callee]Session was a lot of things set
  // explicitly)
  AmSessionContainer::instance()->postEvent(b.id, e);

  if (call_status == Disconnected) updateCallStatus(NoReply);
}

void CallLeg::updateCallStatus(CallStatus new_status)
{
  if (new_status == Connected)
    TRACE("%s leg %s changing status from %s to %s with %s\n",
        a_leg ? "A" : "B",
        getLocalTag().c_str(),
        callStatus2str(call_status),
        callStatus2str(new_status),
        other_id.c_str());
  else
    TRACE("%s leg %s changing status from %s to %s\n",
        a_leg ? "A" : "B",
        getLocalTag().c_str(),
        callStatus2str(call_status),
        callStatus2str(new_status));

  call_status = new_status;
  onCallStatusChange();
}

void CallLeg::addExistingCallee(const string &session_tag, ReconnectLegEvent *ev)
{
  // add existing session as our B leg

  OtherLegInfo b;
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
  ev->setMedia(b.media_session);
  // TODO: what about the RTP relay and other settings? send them as well?
  if (!AmSessionContainer::instance()->postEvent(session_tag, ev)) {
    // session doesn't exist - can't connect
    INFO("the B leg to connect to (%s) doesn't exist\n", session_tag.c_str());
    if (b.media_session) delete b.media_session;
    return;
  }

  other_legs.push_back(b);
  if (call_status == Disconnected) updateCallStatus(NoReply);
}

void CallLeg::addCallee(const string &session_tag, const AmSipRequest &relayed_invite)
{
  addExistingCallee(session_tag, new ReconnectLegEvent(getLocalTag(), relayed_invite));
}

void CallLeg::replaceExistingLeg(const string &session_tag, const AmSipRequest &relayed_invite)
{
  // add existing session as our B leg

  OtherLegInfo b;
  b.id.clear(); // this is an invalid local tag (temporarily)
  if ((rtp_relay_mode == RTP_Relay)) {
    // let the other leg to set its part, we will set our once connected
    b.media_session = new AmB2BMedia(NULL, NULL);
    b.media_session->addReference(); // new reference for me
  }
  else b.media_session = NULL;

  ReplaceLegEvent *ev = new ReplaceLegEvent(getLocalTag(), relayed_invite, b.media_session);
  // TODO: what about the RTP relay and other settings? send them as well?
  if (!AmSessionContainer::instance()->postEvent(session_tag, ev)) {
    // session doesn't exist - can't connect
    INFO("the call leg to be replaced (%s) doesn't exist\n", session_tag.c_str());
    if (b.media_session) delete b.media_session;
    return;
  }

  other_legs.push_back(b);
  if (call_status == Disconnected) updateCallStatus(NoReply); // we are something like connected to another leg
}

void CallLeg::replaceExistingLeg(const string &session_tag, const string &hdrs)
{
  // add existing session as our B leg

  OtherLegInfo b;
  b.id.clear(); // this is an invalid local tag (temporarily)
  if ((rtp_relay_mode == RTP_Relay)) {
    // let the other leg to set its part, we will set our once connected
    b.media_session = new AmB2BMedia(NULL, NULL);
    b.media_session->addReference(); // new reference for me
  }
  else b.media_session = NULL;

  ReconnectLegEvent *rev = new ReconnectLegEvent(a_leg ? ReconnectLegEvent::B : ReconnectLegEvent::A, getLocalTag(), hdrs, established_body);
  rev->setMedia(b.media_session);
  ReplaceLegEvent *ev = new ReplaceLegEvent(getLocalTag(), rev);
  // TODO: what about the RTP relay and other settings? send them as well?
  if (!AmSessionContainer::instance()->postEvent(session_tag, ev)) {
    // session doesn't exist - can't connect
    INFO("the call leg to be replaced (%s) doesn't exist\n", session_tag.c_str());
    if (b.media_session) delete b.media_session;
    return;
  }

  other_legs.push_back(b);
  if (call_status == Disconnected) updateCallStatus(NoReply); // we are something like connected to another leg
}

void CallLeg::clear_other()
{
  removeOtherLeg(other_id);
  AmB2BSession::clear_other();
}

void CallLeg::stopCall() {
  terminateNotConnectedLegs();
  terminateOtherLeg();
  terminateLeg();
}
