#include "SBCCallLeg.h"

#include "SBCCallControlAPI.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmSipHeaders.h"
#include "SBCSimpleRelay.h"
#include "RegisterDialog.h"
#include "SubscriptionDialog.h"

#include "sip/pcap_logger.h"
#include "sip/sip_parser.h"
#include "sip/sip_trans.h"

#include "HeaderFilter.h"
#include "ParamReplacer.h"
#include "SDPFilter.h"
#include "SBCEventLog.h"

#include <algorithm>

using namespace std;

#define TRACE DBG

// helper functions

static const SdpPayload *findPayload(const std::vector<SdpPayload>& payloads, const SdpPayload &payload, int transport)
{
  string pname = payload.encoding_name;
  transform(pname.begin(), pname.end(), pname.begin(), ::tolower);

  for (vector<SdpPayload>::const_iterator p = payloads.begin(); p != payloads.end(); ++p) {
    // fix for clients using non-standard names for static payload type (SPA504g: G729a)
    if (transport == TP_RTPAVP && payload.payload_type < 20) {
      if (payload.payload_type != p->payload_type) continue;
    }
    else {
      string s = p->encoding_name;
      transform(s.begin(), s.end(), s.begin(), ::tolower);
      if (s != pname) continue;
    }

    if (p->clock_rate != payload.clock_rate) continue;
    if ((p->encoding_param >= 0) && (payload.encoding_param >= 0) && 
        (p->encoding_param != payload.encoding_param)) continue;
    return &(*p);
  }
  return NULL;
}

static bool containsPayload(const std::vector<SdpPayload>& payloads, const SdpPayload &payload, int transport)
{
  return findPayload(payloads, payload, transport) != NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////

class SBCRelayController: public RelayController {
  private:
    SBCCallProfile::TranscoderSettings *transcoder_settings;
    bool aleg;

  public:
    SBCRelayController(SBCCallProfile::TranscoderSettings *t, bool _aleg): transcoder_settings(t), aleg(_aleg) { }

    virtual void computeRelayMask(const SdpMedia &m, bool &enable, PayloadMask &mask);
};

void SBCRelayController::computeRelayMask(const SdpMedia &m, bool &enable, PayloadMask &mask)
{
  TRACE("entering SBCRelayController::computeRelayMask(%s)\n", aleg ? "A leg" : "B leg");

  PayloadMask m1, m2;
  bool use_m1 = false;

  /* if "m" contains only "norelay" codecs, relay is enabled for them (main idea
   * of these codecs is to limit network bandwidth and it makes not much sense
   * to transcode between codecs 'which are better to avoid', right?)
   *
   * if "m" contains other codecs, relay is enabled as well
   *
   * => if m contains at least some codecs, relay is enabled */
  enable = !m.payloads.empty();

  vector<SdpPayload> &norelay_payloads =
    aleg ? transcoder_settings->audio_codecs_norelay_aleg : transcoder_settings->audio_codecs_norelay;

  vector<SdpPayload>::const_iterator p;
  for (p = m.payloads.begin(); p != m.payloads.end(); ++p) {

    // do not mark telephone-event payload for relay (and do not use it for
    // transcoding as well)
    if(strcasecmp("telephone-event",p->encoding_name.c_str()) == 0) continue;

    // mark every codec for relay in m2
    TRACE("m2: marking payload %d for relay\n", p->payload_type);
    m2.set(p->payload_type);

    if (!containsPayload(norelay_payloads, *p, m.transport)) {
      // this payload can be relayed

      TRACE("m1: marking payload %d for relay\n", p->payload_type);
      m1.set(p->payload_type);

      if (!use_m1 && containsPayload(transcoder_settings->audio_codecs, *p, m.transport)) {
        // the remote SDP contains transcodable codec which can be relayed (i.e.
        // the one with higher "priority" so we want to disable relaying of the
        // payloads which should not be ralyed if possible)
        use_m1 = true;
      }
    }
  }

  TRACE("using %s\n", use_m1 ? "m1" : "m2");
  if (use_m1) mask = m1;
  else mask = m2;
}

///////////////////////////////////////////////////////////////////////////////////////////

// map stream index and transcoder payload index (two dimensions) into one under
// presumption that there will be less than 128 payloads for transcoding
// (might be handy to remember mapping only for dynamic ones (96-127)
#define MAP_INDEXES(stream_idx, payload_idx) ((stream_idx) * 128 + payload_idx)

void PayloadIdMapping::map(int stream_index, int payload_index, int payload_id)
{
  mapping[MAP_INDEXES(stream_index, payload_index)] = payload_id;
}

int PayloadIdMapping::get(int stream_index, int payload_index)
{
  std::map<int, int>::iterator i = mapping.find(MAP_INDEXES(stream_index, payload_index));
  if (i != mapping.end()) return i->second;
  else return -1;
}

void PayloadIdMapping::reset()
{
  mapping.clear();
}

#undef MAP_INDEXES

///////////////////////////////////////////////////////////////////////////////////////////

// A leg constructor (from SBCDialog)
SBCCallLeg::SBCCallLeg(const SBCCallProfile& call_profile, AmSipDialog* p_dlg,
		       AmSipSubscription* p_subs)
  : CallLeg(p_dlg,p_subs),
    m_state(BB_Init),
    auth(NULL),
    call_profile(call_profile),
    cc_timer_id(SBC_TIMER_ID_CALL_TIMERS_START),
    ext_cc_timer_id(SBC_TIMER_ID_CALL_TIMERS_END + 1),
    cc_started(false),
    logger(NULL)
{
  set_sip_relay_only(false);
  dlg->setRel100State(Am100rel::REL100_IGNORED);

  // better here than in onInvite
  // or do we really want to start with OA when handling initial INVITE?
  dlg->setOAEnabled(false);

  memset(&call_connect_ts, 0, sizeof(struct timeval));
  memset(&call_end_ts, 0, sizeof(struct timeval));

  if(call_profile.rtprelay_bw_limit_rate > 0 &&
     call_profile.rtprelay_bw_limit_peak > 0) {

    RateLimit* limit = new RateLimit(call_profile.rtprelay_bw_limit_rate,
				     call_profile.rtprelay_bw_limit_peak, 1);
    rtp_relay_rate_limit.reset(limit);
  }
}

// B leg constructor (from SBCCalleeSession)
SBCCallLeg::SBCCallLeg(SBCCallLeg* caller, AmSipDialog* p_dlg,
		       AmSipSubscription* p_subs)
  : auth(NULL),
    call_profile(caller->getCallProfile()),
    CallLeg(caller,p_dlg,p_subs),
    cc_started(false),
    logger(NULL)
{
  // FIXME: do we want to inherit cc_vars from caller?
  // Can be pretty dangerous when caller stored pointer to object - we should
  // not probably operate on it! But on other hand it could be handy for
  // something, so just take care when using stored objects...
  // call_profile.cc_vars.clear();

  dlg->setRel100State(Am100rel::REL100_IGNORED);
  dlg->setOAEnabled(false);

  // we need to apply it here instead of in applyBProfile because we have caller
  // here (FIXME: do it on better place and better way than accessing internals
  // of caller->dlg directly)
  if (call_profile.transparent_dlg_id && caller) {
    dlg->setCallid(caller->dlg->getCallid());
    dlg->setExtLocalTag(caller->dlg->getRemoteTag());
  }

  // copy RTP rate limit from caller leg
  if(caller->rtp_relay_rate_limit.get()) {
    rtp_relay_rate_limit.reset(new RateLimit(*caller->rtp_relay_rate_limit.get()));
  }

  // CC interfaces and variables should be already "evaluated" by A leg, we just
  // need to load the DI interfaces for us (later they will be initialized with
  // original INVITE so it must be done in A leg's thread!)
  if (!getCCInterfaces()) {
    throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  initCCExtModules();

  setLogger(caller->getLogger());
}

SBCCallLeg::SBCCallLeg(AmSipDialog* p_dlg, AmSipSubscription* p_subs)
  : CallLeg(p_dlg,p_subs),
    m_state(BB_Init),
    auth(NULL),
    cc_timer_id(SBC_TIMER_ID_CALL_TIMERS_START),
    cc_started(false),
    logger(NULL)
{
}

void SBCCallLeg::onStart()
{
  // this should be the first thing called in session's thread
  CallLeg::onStart();
  if (!a_leg) applyBProfile(); // A leg needs to evaluate profile first
}

void SBCCallLeg::applyAProfile()
{
  // apply A leg configuration (but most of the configuration is applied in
  // SBCFactory::onInvite)

  if (call_profile.rtprelay_enabled || call_profile.transcoder.isActive()) {
    DBG("Enabling RTP relay mode for SBC call\n");

    setRtpRelayForceSymmetricRtp(call_profile.aleg_force_symmetric_rtp_value);
    DBG("%s\n",getRtpRelayForceSymmetricRtp() ?
	"forcing symmetric RTP (passive mode)":
	"disabled symmetric RTP (normal mode)");

    if (call_profile.aleg_rtprelay_interface_value >= 0) {
      setRtpInterface(call_profile.aleg_rtprelay_interface_value);
      DBG("using RTP interface %i for A leg\n", rtp_interface);
    }

    setRtpRelayTransparentSeqno(call_profile.rtprelay_transparent_seqno);
    setRtpRelayTransparentSSRC(call_profile.rtprelay_transparent_ssrc);

    if(call_profile.transcoder.isActive()) {
      setRtpRelayMode(RTP_Transcoding);
      switch(call_profile.transcoder.dtmf_mode) {
      case SBCCallProfile::TranscoderSettings::DTMFAlways:
        enable_dtmf_transcoding = true; break;
      case SBCCallProfile::TranscoderSettings::DTMFNever:
        enable_dtmf_transcoding = false; break;
      case SBCCallProfile::TranscoderSettings::DTMFLowFiCodecs:
        enable_dtmf_transcoding = false;
        lowfi_payloads = call_profile.transcoder.lowfi_codecs;
        break;
      };
    }
    else {
      setRtpRelayMode(RTP_Relay);
    }

    // copy stats counters
    rtp_pegs = call_profile.aleg_rtp_counters;
  }
}

int SBCCallLeg::applySSTCfg(AmConfigReader& sst_cfg, 
			   const AmSipRequest* p_req)
{
  DBG("Enabling SIP Session Timers\n");  
  if (NULL == SBCFactory::instance()->session_timer_fact) {
    ERROR("session_timer module not loaded - "
	  "unable to create call with SST\n");
    return -1;
  }
    
  if (p_req && !SBCFactory::instance()->session_timer_fact->
      onInvite(*p_req, sst_cfg)) {
    return -1;
  }

  AmSessionEventHandler* h = SBCFactory::instance()->session_timer_fact->
    getHandler(this);

  if (!h) {
    ERROR("could not get a session timer event handler\n");
    return -1;
  }

  if (h->configure(sst_cfg)) {
    ERROR("Could not configure the session timer: "
	  "disabling session timers.\n");
    delete h;
  }
  else {
    addHandler(h);
    // hack: repeat calling the handler again to start timers because
    // it was called before SST was applied
    if(p_req) h->onSipRequest(*p_req);
  }

  return 0;
}

void SBCCallLeg::applyBProfile()
{
  // TODO: fix this!!! (see d85ed5c7e6b8d4c24e7e5b61c732c2e1ddd31784)
  // if (!call_profile.contact.empty()) {
  //   dlg->contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) + call_profile.contact + CRLF;
  // }

  if (call_profile.auth_enabled) {
    // adding auth handler
    AmSessionEventHandlerFactory* uac_auth_f =
      AmPlugIn::instance()->getFactory4Seh("uac_auth");
    if (NULL == uac_auth_f)  {
      INFO("uac_auth module not loaded. uac auth NOT enabled.\n");
    } else {
      AmSessionEventHandler* h = uac_auth_f->getHandler(this);

      // we cannot use the generic AmSessionEventHandler hooks,
      // because the hooks don't work in AmB2BSession
      setAuthHandler(h);
      DBG("uac auth enabled for callee session.\n");
    }
  }

  if (call_profile.sst_enabled_value) {
    if(applySSTCfg(call_profile.sst_b_cfg,NULL) < 0) {
       throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
  }

  if (!call_profile.outbound_proxy.empty()) {
    dlg->outbound_proxy = call_profile.outbound_proxy;
    dlg->force_outbound_proxy = call_profile.force_outbound_proxy;
  }

  if (!call_profile.next_hop.empty()) {
    dlg->setNextHop(call_profile.next_hop);
    dlg->setNextHop1stReq(call_profile.next_hop_1st_req);
  }

  DBG("patch_ruri_next_hop = %i",call_profile.patch_ruri_next_hop);
  dlg->setPatchRURINextHop(call_profile.patch_ruri_next_hop);

  // was read from caller but reading directly from profile now
  if (call_profile.outbound_interface_value >= 0)
    dlg->setOutboundInterface(call_profile.outbound_interface_value);

  // was read from caller but reading directly from profile now
  if (call_profile.rtprelay_enabled || call_profile.transcoder.isActive()) {

    if (call_profile.rtprelay_interface_value >= 0)
      setRtpInterface(call_profile.rtprelay_interface_value);

    setRtpRelayForceSymmetricRtp(call_profile.force_symmetric_rtp_value);
    DBG("%s\n",getRtpRelayForceSymmetricRtp() ?
	"forcing symmetric RTP (passive mode)":
	"disabled symmetric RTP (normal mode)");

    setRtpRelayTransparentSeqno(call_profile.rtprelay_transparent_seqno);
    setRtpRelayTransparentSSRC(call_profile.rtprelay_transparent_ssrc);

    // copy stats counters
    rtp_pegs = call_profile.bleg_rtp_counters;
  }

  // was read from caller but reading directly from profile now
  if (!call_profile.callid.empty()) 
    dlg->setCallid(call_profile.callid);
}

int SBCCallLeg::relayEvent(AmEvent* ev)
{
    switch (ev->event_id) {
      case B2BSipRequest:
        {
          B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
          assert(req_ev);

          if (call_profile.headerfilter.size()) {
            //B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
            // header filter
            inplaceHeaderFilter(req_ev->req.hdrs, call_profile.headerfilter);
          }

          DBG("filtering body for request '%s' (c/t '%s')\n",
              req_ev->req.method.c_str(), req_ev->req.body.getCTStr().c_str());
          // todo: handle filtering errors

          int res = filterSdp(req_ev->req.body, req_ev->req.method);
          if (res < 0) return res;
        }
        break;

      case B2BSipReply:
        {
          B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
          assert(reply_ev);

          if(call_profile.transparent_dlg_id &&
	     (reply_ev->reply.from_tag == dlg->getExtLocalTag()))
            reply_ev->reply.from_tag = dlg->getLocalTag();

          if (call_profile.headerfilter.size() ||
              call_profile.reply_translations.size()) {
            // header filter
            if (call_profile.headerfilter.size()) {
              inplaceHeaderFilter(reply_ev->reply.hdrs, call_profile.headerfilter);
            }

            // reply translations
            map<unsigned int, pair<unsigned int, string> >::iterator it =
              call_profile.reply_translations.find(reply_ev->reply.code);

            if (it != call_profile.reply_translations.end()) {
              DBG("translating reply %u %s => %u %s\n",
                  reply_ev->reply.code, reply_ev->reply.reason.c_str(),
                  it->second.first, it->second.second.c_str());
              reply_ev->reply.code = it->second.first;
              reply_ev->reply.reason = it->second.second;
            }
          }

          DBG("filtering body for reply '%s' (c/t '%s')\n",
              reply_ev->trans_method.c_str(), reply_ev->reply.body.getCTStr().c_str());

          filterSdp(reply_ev->reply.body, reply_ev->reply.cseq_method);
        }

        break;
    }

  return CallLeg::relayEvent(ev);
}

SBCCallLeg::~SBCCallLeg()
{
  if (auth)
    delete auth;
  if (logger) dec_ref(logger);
}

void SBCCallLeg::onBeforeDestroy()
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    (*i)->onDestroyLeg(this);
  }
}

UACAuthCred* SBCCallLeg::getCredentials() {
  if (a_leg) return &call_profile.auth_aleg_credentials;
  else return &call_profile.auth_credentials;
}

void SBCCallLeg::onSipRequest(const AmSipRequest& req) {
  // AmB2BSession does not call AmSession::onSipRequest for 
  // forwarded requests - so lets call event handlers here
  // todo: this is a hack, replace this by calling proper session 
  // event handler in AmB2BSession
  bool fwd = sip_relay_only && (req.method != SIP_METH_CANCEL);
  if (fwd) {
    CALL_EVENT_H(onSipRequest,req);
  }

  if (fwd && call_profile.messagefilter.size()) {
    for (vector<FilterEntry>::iterator it=
	   call_profile.messagefilter.begin(); 
	 it != call_profile.messagefilter.end(); it++) {

      if (isActiveFilter(it->filter_type)) {
	bool is_filtered = (it->filter_type == Whitelist) ^ 
	  (it->filter_list.find(req.method) != it->filter_list.end());
	if (is_filtered) {
	  DBG("replying 405 to filtered message '%s'\n", req.method.c_str());
	  dlg->reply(req, 405, "Method Not Allowed", NULL, "", SIP_FLAGS_VERBATIM);
	  return;
	}
      }
    }
  }

  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->onInDialogRequest(this, req) == StopProcessing) return;
  }

  if (fwd && req.method == SIP_METH_INVITE) {
    DBG("replying 100 Trying to INVITE to be fwd'ed\n");
    dlg->reply(req, 100, SIP_REPLY_TRYING);
  }

  CallLeg::onSipRequest(req);
}

void SBCCallLeg::setOtherId(const AmSipReply& reply)
{
  DBG("setting other_id to '%s'",reply.from_tag.c_str());
  setOtherId(reply.from_tag);
  if(call_profile.transparent_dlg_id && !reply.to_tag.empty()) {
    dlg->setExtLocalTag(reply.to_tag);
  }
}

void SBCCallLeg::onSipReply(const AmSipRequest& req, const AmSipReply& reply,
			   AmBasicSipDialog::Status old_dlg_status)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();

  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.body.getCTStr().c_str());
  if (fwd) {
    CALL_EVENT_H(onSipReply, req, reply, old_dlg_status);
  }

  if (NULL != auth) {
    // only for SIP authenticated
    unsigned int cseq_before = dlg->cseq;
    if (auth->onSipReply(req, reply, old_dlg_status)) {
      if (cseq_before != dlg->cseq) {
        DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
            "updating relayed_req map\n", reply.cseq, cseq_before);
        updateUACTransCSeq(reply.cseq, cseq_before);
      }
    }
  }

  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->onInDialogReply(this, reply) == StopProcessing) return;
  }

  CallLeg::onSipReply(req, reply, old_dlg_status);
}

void SBCCallLeg::onSendRequest(AmSipRequest& req, int &flags) {

  if(a_leg) {
    if (!call_profile.aleg_append_headers_req.empty()) {
      DBG("appending '%s' to outbound request (A leg)\n",
	  call_profile.aleg_append_headers_req.c_str());
      req.hdrs+=call_profile.aleg_append_headers_req;
    }
  }
  else {
    if (!call_profile.append_headers_req.empty()) {
      DBG("appending '%s' to outbound request (B leg)\n", 
	  call_profile.append_headers_req.c_str());
      req.hdrs+=call_profile.append_headers_req;
    }
  }

  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", req.cseq);
    auth->onSendRequest(req, flags);
  }

  CallLeg::onSendRequest(req, flags);
}

void SBCCallLeg::onRemoteDisappeared(const AmSipReply& reply)
{
  CallLeg::onRemoteDisappeared(reply);
  if(a_leg)
    SBCEventLog::instance()->logCallEnd(dlg,"reply",&call_connect_ts);
}

void SBCCallLeg::onBye(const AmSipRequest& req)
{
  CallLeg::onBye(req);
  if(a_leg)
    SBCEventLog::instance()->logCallEnd(req,getLocalTag(),"bye",&call_connect_ts);
}

void SBCCallLeg::onOtherBye(const AmSipRequest& req)
{
  CallLeg::onOtherBye(req);
  if(a_leg)
    SBCEventLog::instance()->logCallEnd(req,getLocalTag(),"bye",&call_connect_ts);
}

void SBCCallLeg::onDtmf(int event, int duration)
{
  AmB2BMedia *ms = getMediaSession();
  if(ms) {
    DBG("received DTMF on %c-leg (%i;%i)\n",
	a_leg ? 'A': 'B', event, duration);
    ms->sendDtmf(!a_leg,event,duration);
  }
}

bool SBCCallLeg::updateLocalSdp(AmSdp &sdp)
{
  // anonymize SDP if configured to do so (we need to have our local media IP,
  // not the media IP of our peer leg there)
  if (call_profile.anonymize_sdp) normalizeSDP(sdp, call_profile.anonymize_sdp, advertisedIP());

  // remember transcodable payload IDs
  if (call_profile.transcoder.isActive()) savePayloadIDs(sdp);
  return CallLeg::updateLocalSdp(sdp);
}


bool SBCCallLeg::updateRemoteSdp(AmSdp &sdp)
{
  SBCRelayController rc(&call_profile.transcoder, a_leg);
  if (call_profile.transcoder.isActive()) {
    AmB2BMedia *ms = getMediaSession();
    if (ms) return ms->updateRemoteSdp(a_leg, sdp, &rc);
  }

  // call original implementation because our special conditions above are not met
  return CallLeg::updateRemoteSdp(sdp);
}

void SBCCallLeg::onControlCmd(string& cmd, AmArg& params) {
  if (cmd == "teardown") {
    if (a_leg) {
      // was for caller:
      DBG("teardown requested from control cmd\n");
      stopCall();
      SBCEventLog::instance()->logCallEnd(dlg,"ctrl-cmd",&call_connect_ts);
      // FIXME: don't we want to relay the controll event as well?
    }
    else {
      // was for callee:
      DBG("relaying teardown control cmd to A leg\n");
      relayEvent(new SBCControlEvent(cmd, params));
      // FIXME: don't we want to stopCall as well?
    }
    return;
  }
  DBG("ignoring unknown control cmd : '%s'\n", cmd.c_str());
}


void SBCCallLeg::process(AmEvent* ev) {
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->onEvent(this, ev) == StopProcessing) return;
  }

  if (a_leg) {
    // was for caller (SBCDialog):
    AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
    if(plugin_event && plugin_event->name == "timer_timeout") {
      int timer_id = plugin_event->data.get(0).asInt();
      if (timer_id >= SBC_TIMER_ID_CALL_TIMERS_START &&
          timer_id <= SBC_TIMER_ID_CALL_TIMERS_END) {
        DBG("timer %d timeout, stopping call\n", timer_id);
        stopCall();
	SBCEventLog::instance()->logCallEnd(dlg,"timeout",&call_connect_ts);
        ev->processed = true;
      }
    }

    SBCCallTimerEvent* ct_event;
    if (ev->event_id == SBCCallTimerEvent_ID &&
        (ct_event = dynamic_cast<SBCCallTimerEvent*>(ev)) != NULL) {
      switch (m_state) {
        case BB_Connected: 
          switch (ct_event->timer_action) {
            case SBCCallTimerEvent::Remove:
              DBG("removing timer %d on call timer request\n", ct_event->timer_id);
              removeTimer(ct_event->timer_id); return;
            case SBCCallTimerEvent::Set:
              DBG("setting timer %d to %f on call timer request\n",
                  ct_event->timer_id, ct_event->timeout);
              setTimer(ct_event->timer_id, ct_event->timeout); return;
            case SBCCallTimerEvent::Reset:
              DBG("resetting timer %d to %f on call timer request\n",
                  ct_event->timer_id, ct_event->timeout);
              removeTimer(ct_event->timer_id);
              setTimer(ct_event->timer_id, ct_event->timeout);
              return;
            default: ERROR("unknown timer_action in sbc call timer event\n"); return;
          }

        case BB_Init:
        case BB_Dialing:

          switch (ct_event->timer_action) {
            case SBCCallTimerEvent::Remove: 
              clearCallTimer(ct_event->timer_id); 
              return;

            case SBCCallTimerEvent::Set:
            case SBCCallTimerEvent::Reset:
              saveCallTimer(ct_event->timer_id, ct_event->timeout); 
              return;

            default: ERROR("unknown timer_action in sbc call timer event\n"); return;
          }
          break;

        default: break;
      }
    }
  }

  SBCControlEvent* ctl_event;
  if (ev->event_id == SBCControlEvent_ID &&
      (ctl_event = dynamic_cast<SBCControlEvent*>(ev)) != NULL) {
    onControlCmd(ctl_event->cmd, ctl_event->params);
    return;
  }

  CallLeg::process(ev);
}


//////////////////////////////////////////////////////////////////////////////////////////
// was for caller only (SBCDialog)
// FIXME: move the stuff related to CC interface outside of this class?


#define REPLACE_VALS req, app_param, ruri_parser, from_parser, to_parser

void SBCCallLeg::onInvite(const AmSipRequest& req)
{
  DBG("processing initial INVITE %s\n", req.r_uri.c_str());

  ParamReplacerCtx ctx(&call_profile);
  ctx.app_param = getHeader(req.hdrs, PARAM_HDR, true);

  // process call control
  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_start_ts, NULL);

    call_profile.eval_cc_list(ctx,req);

    // fix up module names
    for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {

      cc_it->cc_module =
	ctx.replaceParameters(cc_it->cc_module, "cc_module", req);
    }

    if (!getCCInterfaces()) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    // fix up variables
    call_profile.replace_cc_values(ctx,req,NULL);

    if (!CCStart(req)) {
      setStopped();
      oodHandlingTerminated(req, cc_modules, call_profile);
      return;
    }
  }

  call_profile.sst_aleg_enabled = 
    ctx.replaceParameters(call_profile.sst_aleg_enabled,
			  "enable_aleg_session_timer", req);

  call_profile.sst_enabled = ctx.replaceParameters(call_profile.sst_enabled, 
						   "enable_session_timer", req);

  if ((call_profile.sst_aleg_enabled == "yes") ||
      (call_profile.sst_enabled == "yes")) {

    call_profile.eval_sst_config(ctx,req,call_profile.sst_a_cfg);
    if(applySSTCfg(call_profile.sst_a_cfg,&req) < 0) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
  }

  if(dlg->reply(req, 100, "Connecting") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  if (!call_profile.evaluate(ctx, req)) {
    ERROR("call profile evaluation failed\n");
    throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  initCCExtModules();

  string ruri, to, from;
  AmSipRequest uac_req(req);
  AmUriParser uac_ruri;
  uac_ruri.uri = uac_req.r_uri;
  if(!uac_ruri.parse_uri()) {
    DBG("Error parsing R-URI '%s'\n",uac_ruri.uri.c_str());
    throw AmSession::Exception(400,"Failed to parse R-URI");
  }

  if(call_profile.contact_hiding) { 
    if(RegisterDialog::decodeUsername(req.user,uac_ruri)) {
      uac_req.r_uri = uac_ruri.uri_str();
    }
  }
  else if(call_profile.reg_caching) {
    // REG-Cache lookup
    uac_req.r_uri = call_profile.retarget(req.user,*dlg);
  }

  ruri = call_profile.ruri.empty() ? uac_req.r_uri : call_profile.ruri;
  if(!call_profile.ruri_host.empty()){
    ctx.ruri_parser.uri = ruri;
    if(!ctx.ruri_parser.parse_uri()) {
      WARN("Error parsing R-URI '%s'\n", ruri.c_str());
    }
    else {
      ctx.ruri_parser.uri_port.clear();
      ctx.ruri_parser.uri_host = call_profile.ruri_host;
      ruri = ctx.ruri_parser.uri_str();
    }
  }
  from = call_profile.from.empty() ? req.from : call_profile.from;
  to = call_profile.to.empty() ? req.to : call_profile.to;

  applyAProfile();
  call_profile.apply_a_routing(ctx,req,*dlg);

  m_state = BB_Dialing;

  // prepare request to relay to the B leg(s)

  AmSipRequest invite_req(req);
  est_invite_cseq = req.cseq;

  removeHeader(invite_req.hdrs,PARAM_HDR);
  removeHeader(invite_req.hdrs,"P-App-Name");

  if (call_profile.sst_enabled_value) {
    removeHeader(invite_req.hdrs,SIP_HDR_SESSION_EXPIRES);
    removeHeader(invite_req.hdrs,SIP_HDR_MIN_SE);
  }

  inplaceHeaderFilter(invite_req.hdrs, call_profile.headerfilter);

  if (call_profile.append_headers.size() > 2) {
    string append_headers = call_profile.append_headers;
    assertEndCRLF(append_headers);
    invite_req.hdrs+=append_headers;
  }
  
  int res = filterSdp(invite_req.body, invite_req.method);
  if (res < 0) {
    // FIXME: quick hack, throw the exception from the filtering function for
    // requests
    throw AmSession::Exception(488, SIP_REPLY_NOT_ACCEPTABLE_HERE);
  }

#undef REPLACE_VALS

  DBG("SBC: connecting to '%s'\n",ruri.c_str());
  DBG("     From:  '%s'\n",from.c_str());
  DBG("     To:  '%s'\n",to.c_str());

  // we evaluated the settings, now we can initialize internals (like RTP relay)
  // we have to use original request (not the altered one) because for example
  // codecs filtered out might be used in direction to caller
  CallLeg::onInvite(req);

  // call extend call controls
  InitialInviteHandlerParams params(to, ruri, from, &req, &invite_req);
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    (*i)->onInitialInvite(this, params);
  }

  if (getCallStatus() == Disconnected) {
    // no CC module connected a callee yet
    connectCallee(to, ruri, from, req, invite_req); // connect to the B leg(s) using modified request
  }
}

void SBCCallLeg::connectCallee(const string& remote_party, 
			       const string& remote_uri,
			       const string &from, 
			       const AmSipRequest &original_invite, 
			       const AmSipRequest &invite)
{
  // FIXME: no fork for now

  SBCCallLeg* callee_session = SBCFactory::instance()->
    getCallLegCreator()->create(this);

  callee_session->setLocalParty(from, from);
  callee_session->setRemoteParty(remote_party, remote_uri);

  DBG("Created B2BUA callee leg, From: %s\n", from.c_str());

  // FIXME: inconsistent with other filtering stuff - here goes the INVITE
  // already filtered so need not to be catched (can not) in relayEvent because
  // it is sent other way
  addCallee(callee_session, invite);
  
  // we could start in SIP relay mode from the beginning if only one B leg, but
  // serial fork might mess it
  // set_sip_relay_only(true);
}

bool SBCCallLeg::getCCInterfaces() {
  return ::getCCInterfaces(call_profile.cc_interfaces, cc_modules);
}

void SBCCallLeg::onCallConnected(const AmSipReply& reply) {
  if (a_leg) { // FIXME: really?
    m_state = BB_Connected;

    if (!startCallTimers())
      return;

    if (call_profile.cc_interfaces.size()) {
      gettimeofday(&call_connect_ts, NULL);
    }

    logCallStart(reply);
    CCConnect(reply);
  }
}

void SBCCallLeg::onStop() {
  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_end_ts, NULL);
  }

  if (a_leg && m_state == BB_Connected) { // m_state might be valid for A leg only
    stopCallTimers();
  }

  m_state = BB_Teardown;

  // call only if really started (on CCStart failure CCEnd will be called
  // explicitly)
  // Note that role may change, so testing for a_leg need not to be correct.
  if (cc_started) CCEnd();
}

void SBCCallLeg::saveCallTimer(int timer, double timeout) {
  call_timers[timer] = timeout;
}

void SBCCallLeg::clearCallTimer(int timer) {
  call_timers.erase(timer);
}

void SBCCallLeg::clearCallTimers() {
  call_timers.clear();
}

/** @return whether successful */
bool SBCCallLeg::startCallTimers() {
  for (map<int, double>::iterator it=
	 call_timers.begin(); it != call_timers.end(); it++) {
    DBG("SBC: starting call timer %i of %f seconds\n", it->first, it->second);
    setTimer(it->first, it->second);
  }

  return true;
}

void SBCCallLeg::stopCallTimers() {
  for (map<int, double>::iterator it=
	 call_timers.begin(); it != call_timers.end(); it++) {
    DBG("SBC: removing call timer %i\n", it->first);
    removeTimer(it->first);
  }
}

bool SBCCallLeg::CCStart(const AmSipRequest& req) {
  if (!a_leg) return true; // preserve original behavior of the CC interface

  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);
    di_args.push(getLocalTag());
    di_args.push((AmObject*)&call_profile);
    di_args.push((AmObject*)&req); // INVITE request
    di_args.push(AmArg());
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    for (int i=0;i<4;i++)
      di_args.back().push((int)0);

    di_args.push(AmArg());
    AmArg& vals = di_args.back();
    vals.assertStruct();
    for (map<string, string>::iterator it = cc_if.cc_values.begin();
	 it != cc_if.cc_values.end(); it++) {
      vals[it->first] = it->second;
    }

    di_args.push(cc_timer_id); // current timer ID

    bool exception_occured = false;
    try {
      (*cc_mod)->invoke("start", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface start "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      exception_occured = true;
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface start "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      exception_occured = true;
    }

    if(exception_occured) {
      SBCEventLog::instance()->
	logCallStart(req, getLocalTag(), dlg->getRemoteUA(), "",
		     500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      AmBasicSipDialog::reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);

      // call 'end' of call control modules up to here
      call_end_ts.tv_sec = call_start_ts.tv_sec;
      call_end_ts.tv_usec = call_start_ts.tv_usec;
      CCEnd(cc_it);

      return false;
    }

    if (!logger && !call_profile.msg_logger_path.empty()) {
      // open the logger if not already opened
      ParamReplacerCtx ctx(&call_profile);
      string log_path = ctx.replaceParameters(call_profile.msg_logger_path,
					      "msg_logger_path",req);
      if (openLogger(log_path)) logRequest(req);
    }

    // evaluate ret
    if (isArgArray(ret)) {
      for (size_t i=0;i<ret.size();i++) {
	if (!isArgArray(ret[i]) || !ret[i].size())
	  continue;
	if (!isArgInt(ret[i][SBC_CC_ACTION])) {
	  ERROR("in call control module '%s' - action type not int\n",
		cc_if.cc_name.c_str());
	  continue;
	}
	switch (ret[i][SBC_CC_ACTION].asInt()) {
	case SBC_CC_DROP_ACTION: {
	  DBG("dropping call on call control action DROP from '%s'\n",
	      cc_if.cc_name.c_str());
	  dlg->setStatus(AmSipDialog::Disconnected);

	  // call 'end' of call control modules up to here
	  call_end_ts.tv_sec = call_start_ts.tv_sec;
	  call_end_ts.tv_usec = call_start_ts.tv_usec;
	  CCEnd(cc_it);

	  return false;
	}

	case SBC_CC_REFUSE_ACTION: {
	  if (ret[i].size() < 3 ||
	      !isArgInt(ret[i][SBC_CC_REFUSE_CODE]) ||
	      !isArgCStr(ret[i][SBC_CC_REFUSE_REASON])) {
	    ERROR("in call control module '%s' - REFUSE action parameters missing/wrong: '%s'\n",
		  cc_if.cc_name.c_str(), AmArg::print(ret[i]).c_str());
	    continue;
	  }
	  string headers;
	  if (ret[i].size() > SBC_CC_REFUSE_HEADERS) {
	    for (size_t h=0;h<ret[i][SBC_CC_REFUSE_HEADERS].size();h++)
	      headers += string(ret[i][SBC_CC_REFUSE_HEADERS][h].asCStr()) + CRLF;
	  }

	  DBG("replying with %d %s on call control action REFUSE from '%s' headers='%s'\n",
	      ret[i][SBC_CC_REFUSE_CODE].asInt(), ret[i][SBC_CC_REFUSE_REASON].asCStr(),
	      cc_if.cc_name.c_str(), headers.c_str());

	  SBCEventLog::instance()->
	    logCallStart(req, getLocalTag(), dlg->getRemoteUA(), "",
			 ret[i][SBC_CC_REFUSE_CODE].asInt(),
			 ret[i][SBC_CC_REFUSE_REASON].asCStr());

	  dlg->reply(req,
		     ret[i][SBC_CC_REFUSE_CODE].asInt(),
		     ret[i][SBC_CC_REFUSE_REASON].asCStr(),
		     NULL, headers);

	  // call 'end' of call control modules up to here
	  call_end_ts.tv_sec = call_start_ts.tv_sec;
	  call_end_ts.tv_usec = call_start_ts.tv_usec;
	  CCEnd(cc_it);
	  return false;
	}

	case SBC_CC_SET_CALL_TIMER_ACTION: {
	  if (cc_timer_id > SBC_TIMER_ID_CALL_TIMERS_END) {
	    ERROR("too many call timers - ignoring timer\n");
	    continue;
	  }

	  if (ret[i].size() < 2 ||
	      (!(isArgInt(ret[i][SBC_CC_TIMER_TIMEOUT]) ||
		 isArgDouble(ret[i][SBC_CC_TIMER_TIMEOUT])))) {
	    ERROR("in call control module '%s' - SET_CALL_TIMER action parameters missing: '%s'\n",
		  cc_if.cc_name.c_str(), AmArg::print(ret[i]).c_str());
	    continue;
	  }

	  double timeout;
	  if (isArgInt(ret[i][SBC_CC_TIMER_TIMEOUT]))
	    timeout = ret[i][SBC_CC_TIMER_TIMEOUT].asInt();
	  else
	    timeout = ret[i][SBC_CC_TIMER_TIMEOUT].asDouble();

	  DBG("saving call timer %i: timeout %f\n", cc_timer_id, timeout);
	  saveCallTimer(cc_timer_id, timeout);
	  cc_timer_id++;
	} break;
	default: {
	  ERROR("unknown call control action: '%s'\n", AmArg::print(ret[i]).c_str());
	  continue;
	}

	}

      }
    }

    cc_mod++;
  }
  cc_started = true;
  return true;
}

void SBCCallLeg::CCConnect(const AmSipReply& reply) {
  if (!cc_started) return; // preserve original behavior of the CC interface

  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);                // cc name
    di_args.push(getLocalTag());                // call ltag
    di_args.push((AmObject*)&call_profile);     // call profile
    di_args.push((AmObject*)NULL);              // there is no sip msg
    di_args.push(AmArg());                      // timestamps
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    di_args.back().push((int)call_connect_ts.tv_sec);
    di_args.back().push((int)call_connect_ts.tv_usec);
    for (int i=0;i<2;i++)
      di_args.back().push((int)0);
    di_args.push(getOtherId());                      // other leg ltag


    try {
      (*cc_mod)->invoke("connect", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface connect "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      stopCall();
      return;
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface connect "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      stopCall();
      return;
    }

    cc_mod++;
  }
}

void SBCCallLeg::CCEnd() {
  CCEnd(call_profile.cc_interfaces.end());
}

void SBCCallLeg::CCEnd(const CCInterfaceListIteratorT& end_interface) {
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != end_interface; cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);
    di_args.push(getLocalTag());                 // call ltag
    di_args.push((AmObject*)&call_profile);
    di_args.push((AmObject*)NULL);               // there is no sip msg
    di_args.push(AmArg());                       // timestamps
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    di_args.back().push((int)call_connect_ts.tv_sec);
    di_args.back().push((int)call_connect_ts.tv_usec);
    di_args.back().push((int)call_end_ts.tv_sec);
    di_args.back().push((int)call_end_ts.tv_usec);

    try {
      (*cc_mod)->invoke("end", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface end "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface end "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
    }

    cc_mod++;
  }
}

void SBCCallLeg::onCallStatusChange(const StatusChangeCause &cause)
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    (*i)->onStateChange(this, cause);
  }
}

void SBCCallLeg::onBLegRefused(const AmSipReply& reply)
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->onBLegRefused(this, reply) == StopProcessing) return;
  }
}

void SBCCallLeg::onCallFailed(CallFailureReason reason, const AmSipReply *reply)
{
  switch (reason) {
    case CallRefused:
      if (reply) logCallStart(*reply);
      break;

    case CallCanceled:
      logCanceledCall();
      break;
  }
}

bool SBCCallLeg::onBeforeRTPRelay(AmRtpPacket* p, sockaddr_storage* remote_addr)
{
  if(rtp_relay_rate_limit.get() &&
     rtp_relay_rate_limit->limit(p->getBufferSize()))
    return false; // drop

  return true; // relay
}

void SBCCallLeg::onAfterRTPRelay(AmRtpPacket* p, sockaddr_storage* remote_addr)
{
  for(list<atomic_int*>::iterator it = rtp_pegs.begin();
      it != rtp_pegs.end(); ++it) {
    (*it)->inc(p->getBufferSize());
  }
}

void SBCCallLeg::logCallStart(const AmSipReply& reply)
{
  std::map<int,AmSipRequest>::iterator t_req = recvd_req.find(reply.cseq);
  if (t_req != recvd_req.end()) {
    string b_leg_ua = getHeader(reply.hdrs,"Server");
    SBCEventLog::instance()->logCallStart(t_req->second,getLocalTag(),
					  dlg->getRemoteUA(),b_leg_ua,
					  (int)reply.code,reply.reason);
  }
  else {
    ERROR("could not log call-start/call-attempt (ci='%s';lt='%s')",
	  getCallID().c_str(),getLocalTag().c_str());
  }
}

void SBCCallLeg::logCanceledCall()
{
  std::map<int,AmSipRequest>::iterator t_req = recvd_req.find(est_invite_cseq);
  if (t_req != recvd_req.end()) {
    SBCEventLog::instance()->logCallStart(t_req->second,getLocalTag(),
					  "","",
					  0,"canceled");
  }
  else {
    ERROR("could not log call-attempt (canceled, ci='%s';lt='%s')",
	  getCallID().c_str(),getLocalTag().c_str());
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
// body filtering

int SBCCallLeg::filterSdp(AmMimeBody &body, const string &method)
{
  DBG("filtering body\n");

  AmMimeBody* sdp_body = body.hasContentType(SIP_APPLICATION_SDP);
  if (!sdp_body) return 0;

  // filter body for given methods only
  if (!(method == SIP_METH_INVITE ||
       method == SIP_METH_UPDATE ||
       method == SIP_METH_PRACK ||
       method == SIP_METH_ACK)) return 0;

  AmSdp sdp;
  int res = sdp.parse((const char *)sdp_body->getPayload());
  if (0 != res) {
    DBG("SDP parsing failed during body filtering!\n");
    return res;
  }

  bool changed = false;
  bool prefer_existing_codecs = call_profile.codec_prefs.preferExistingCodecs(a_leg);

  bool needs_normalization =
          call_profile.codec_prefs.shouldOrderPayloads(a_leg) ||
          call_profile.transcoder.isActive() ||
          !call_profile.sdpfilter.empty();

  if (needs_normalization) {
    normalizeSDP(sdp, false, ""); // anonymization is done in the other leg to use correct IP address
    changed = true;
  }

  if (prefer_existing_codecs) {
    // We have to order payloads before adding transcoder codecs to leave
    // transcoding as the last chance (existing codecs are preferred thus
    // relaying will be used if possible).
    if (call_profile.codec_prefs.shouldOrderPayloads(a_leg)) {
      call_profile.codec_prefs.orderSDP(sdp, a_leg);
      changed = true;
    }
  }

  // Add transcoder codecs before filtering because otherwise SDP filter could
  // inactivate some media lines which shouldn't be inactivated.

  if (call_profile.transcoder.isActive()) {
    appendTranscoderCodecs(sdp);
    changed = true;
  }

  if (!prefer_existing_codecs) {
    // existing codecs are not preferred - reorder AFTER adding transcoder
    // codecs so it might happen that transcoding will be forced though relaying
    // would be possible
    if (call_profile.codec_prefs.shouldOrderPayloads(a_leg)) {
      call_profile.codec_prefs.orderSDP(sdp, a_leg);
      changed = true;
    }
  }

  // It doesn't make sense to filter out codecs allowed for transcoding and thus
  // if the filter filters them out it can be considered as configuration
  // problem, right?
  // => So we wouldn't try to avoid filtering out transcoder codecs what would
  // just complicate things.

  if (call_profile.sdpfilter.size()) {
    res = filterSDP(sdp, call_profile.sdpfilter);
    changed = true;
  }
  if (call_profile.sdpalinesfilter.size()) {
    // filter SDP "a=lines"
    filterSDPalines(sdp, call_profile.sdpalinesfilter);
    changed = true;
  }

  if (changed) {
    string n_body;
    sdp.print(n_body);
    sdp_body->setPayload((const unsigned char*)n_body.c_str(), n_body.length());
  }

  return res;
}

void SBCCallLeg::appendTranscoderCodecs(AmSdp &sdp)
{
  // append codecs for transcoding, remember the added ones to be able to filter
  // them out from relayed reply!

  // important: normalized SDP should get here

  TRACE("going to append transcoder codecs into SDP\n");
  const std::vector<SdpPayload> &transcoder_codecs = call_profile.transcoder.audio_codecs;

  unsigned stream_idx = 0;
  vector<SdpPayload>::const_iterator p;
  for (vector<SdpMedia>::iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) {

    // handle audio transcoder codecs
    if (m->type == MT_AUDIO) {
      // transcoder codecs can be added only if there are common payloads with
      // the remote (only those allowed for transcoder)
      // if there are no such common payloads adding transcoder codecs can't help
      // because we won't be able to transcode later on!
      // (we have to check for each media stream independently)

      // find first unused dynamic payload number & detect transcodable codecs
      // in original SDP
      int id = 96;
      bool transcodable = false;
      PayloadMask used_payloads;
      for (p = m->payloads.begin(); p != m->payloads.end(); ++p) {
        if (p->payload_type >= id) id = p->payload_type + 1;
        if (containsPayload(transcoder_codecs, *p, m->transport)) transcodable = true;
        used_payloads.set(p->payload_type);
      }

      if (transcodable) {
        // there are some transcodable codecs present in the SDP, we can safely
        // add the other transcoder codecs to the SDP
        unsigned idx = 0;
        for (p = transcoder_codecs.begin(); p != transcoder_codecs.end(); ++p, ++idx) {
          // add all payloads which are not already there
          if (!containsPayload(m->payloads, *p, m->transport)) {
            m->payloads.push_back(*p);
            int &pid = m->payloads.back().payload_type;
            if (pid < 0) {
              // try to use remembered ID
              pid = transcoder_payload_mapping.get(stream_idx, idx);
            }

            if ((pid < 0) || used_payloads.get(pid)) {
              // payload ID is not set or is already used in current SDP, we
              // need to assign a new one
              pid = id++;
            }
          }
        }
        if (id > 128) ERROR("assigned too high payload type number (%d), see RFC 3551\n", id);
      }
      else {
        // no compatible codecs found
        TRACE("can not transcode stream %d - no compatible codecs with transcoder_codecs found\n", stream_idx + 1);
      }

      stream_idx++; // count chosen media type only
    }
  }

  // remembered payload IDs should be used just once, in SDP answer
  // unfortunatelly the SDP answer might be present in 1xx and in 2xx as well so
  // we can't clear it here
  // on other hand it might be useful to use the same payload ID if offer/answer
  // is repeated in the other direction next time
}

void SBCCallLeg::savePayloadIDs(AmSdp &sdp)
{
  unsigned stream_idx = 0;
  std::vector<SdpPayload> &transcoder_codecs = call_profile.transcoder.audio_codecs;
  for (vector<SdpMedia>::iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) {
    if (m->type != MT_AUDIO) continue;

    unsigned idx = 0;
    for (vector<SdpPayload>::iterator p = transcoder_codecs.begin();
        p != transcoder_codecs.end(); ++p, ++idx)
    {
      if (p->payload_type < 0) {
        const SdpPayload *pp = findPayload(m->payloads, *p, m->transport);
        if (pp && (pp->payload_type >= 0))
          transcoder_payload_mapping.map(stream_idx, idx, pp->payload_type);
      }
    }

    stream_idx++; // count chosen media type only
  }
}

bool SBCCallLeg::reinvite(const AmSdp &sdp, unsigned &request_cseq)
{
  request_cseq = 0;

  AmMimeBody body;
  AmMimeBody *sdp_body = body.addPart(SIP_APPLICATION_SDP);
  if (!sdp_body) return false;

  string body_str;
  sdp.print(body_str);
  sdp_body->parse(SIP_APPLICATION_SDP, (const unsigned char*)body_str.c_str(), body_str.length());

  if (dlg->reinvite("", &body, SIP_FLAGS_VERBATIM) != 0) return false;
  request_cseq = dlg->cseq - 1;
  return true;
}

void SBCCallLeg::changeRtpMode(RTPRelayMode new_mode)
{
  if (new_mode == rtp_relay_mode) return; // requested mode is set already

  if (!((getCallStatus() == CallLeg::Connected) ||
        (getCallStatus() == CallLeg::Disconnecting) ||
        (getCallStatus() == CallLeg::Disconnected))) {
    ERROR("BUG: changeRtpMode supported for established/disconnecting/disconnected calls only\n");
    return;
  }

  clearRtpReceiverRelay();

  // we don't need to send reINVITE from here, expecting caller knows what is he
  // doing (it is probably processing or generating its own reINVITE)
  // Switch from RTP_Direct to RTP_Relay is safe (no audio loss), the other can
  // be lossy because already existing media object would be destroyed.
  // FIXME: use AmB2BMedia in all RTP relay modes to avoid these problems?

  switch (new_mode) {
  case RTP_Relay:
  case RTP_Transcoding:
      setMediaSession(new AmB2BMedia(a_leg ? this: NULL, a_leg ? NULL : this));
      break;

  case RTP_Direct:
      break;
  }

  if (!getOtherId().empty())
    relayEvent(new ChangeRtpModeEvent(new_mode, getMediaSession()));
  setRtpRelayMode(new_mode);
}

void SBCCallLeg::initCCExtModules()
{
  // init extended call control modules
  vector<AmDynInvoke*>::iterator cc_mod = cc_modules.begin();
  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++)
  {
    CCInterface& cc_if = *cc_it;
    string& cc_module = cc_it->cc_module;

    // get extended CC interface
    try {
      AmArg args, ret;
      (*cc_mod)->invoke("getExtendedInterfaceHandler", args, ret);
      ExtendedCCInterface *iface = dynamic_cast<ExtendedCCInterface*>(ret[0].asObject());
      if (iface) {
        DBG("extended CC interface offered by cc_module '%s'\n", cc_module.c_str());
        cc_ext.push_back(iface);

        // module initialization
        iface->init(this, cc_if.cc_values);
      }
      else WARN("BUG: returned invalid extended CC interface by cc_module '%s'\n", cc_module.c_str());
    }
    catch (...) {
      DBG("extended CC interface not supported by cc_module '%s'\n", cc_module.c_str());
    }

    ++cc_mod;
  }
}

void SBCCallLeg::onB2BEvent(B2BEvent* ev)
{
  if (ev->event_id == ChangeRtpModeEventId) {
    ChangeRtpModeEvent *e = dynamic_cast<ChangeRtpModeEvent*>(ev);
    if (e) {
      if (e->new_mode == rtp_relay_mode) return; // requested mode is set already

      clearRtpReceiverRelay();

      switch (e->new_mode) {
      case RTP_Relay:
      case RTP_Transcoding:
          setMediaSession(e->media);
          if (e->media) getMediaSession()->changeSession(a_leg, this);
          break;

      case RTP_Direct:
          break;
      }
      setRtpRelayMode(e->new_mode);
    }
  }

  CallLeg::onB2BEvent(ev);
}

void SBCCallLeg::putOnHold()
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->putOnHold(this) == StopProcessing) return;
  }
  CallLeg::putOnHold();
}

void SBCCallLeg::resumeHeld(bool send_reinvite)
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->resumeHeld(this, send_reinvite) == StopProcessing) return;
  }
  CallLeg::resumeHeld(send_reinvite);
}

void SBCCallLeg::handleHoldReply(bool succeeded)
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->handleHoldReply(this, succeeded) == StopProcessing) return;
  }
  CallLeg::handleHoldReply(succeeded);
}

void SBCCallLeg::createHoldRequest(AmSdp &sdp)
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->createHoldRequest(this, sdp) == StopProcessing) return;
  }
  CallLeg::createHoldRequest(sdp);
}

void SBCCallLeg::setMediaSession(AmB2BMedia *new_session)
{
  if (new_session) {
    if (call_profile.log_rtp) new_session->setRtpLogger(logger);
    else new_session->setRtpLogger(NULL);
  }
  CallLeg::setMediaSession(new_session);
}

bool SBCCallLeg::openLogger(const std::string &path)
{
  file_msg_logger *log = new pcap_logger();

  if(log->open(path.c_str()) != 0) {
    // open error
    delete log;
    return false;
  }

  // opened successfully
  setLogger(log);
  return true;
}

void SBCCallLeg::setLogger(msg_logger *_logger)
{
  if (logger) dec_ref(logger); // release the old one

  logger = _logger;
  if (logger) inc_ref(logger);
  if (call_profile.log_sip) dlg->setMsgLogger(logger);
  else dlg->setMsgLogger(NULL);

  AmB2BMedia *m = getMediaSession();
  if (m) {
    if (call_profile.log_rtp) m->setRtpLogger(logger);
    else m->setRtpLogger(NULL);
  }
}

void SBCCallLeg::logRequest(const AmSipRequest &req)
{
  if (!call_profile.log_sip || !logger) return;

  req.tt.lock_bucket();
  const sip_trans* t = req.tt.get_trans();
  if (t) {
    sip_msg* msg = t->msg;
    logger->log(msg->buf,msg->len,&msg->remote_ip,
        &msg->local_ip,msg->u.request->method_str);
  }
  req.tt.unlock_bucket();
}
