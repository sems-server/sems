#include "SBCCallLeg.h"

#include "ampi/SBCCallControlAPI.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmSipHeaders.h"

#include "HeaderFilter.h"
#include "ParamReplacer.h"
#include "SDPFilter.h"
#include <algorithm>

using namespace std;

#define TRACE DBG

// helper functions

static const SdpPayload *findPayload(const std::vector<SdpPayload>& payloads, const SdpPayload &payload)
{
  string pname = payload.encoding_name;
  transform(pname.begin(), pname.end(), pname.begin(), ::tolower);

  for (vector<SdpPayload>::const_iterator p = payloads.begin(); p != payloads.end(); ++p) {
    string s = p->encoding_name;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s != pname) continue;
    if (p->clock_rate != payload.clock_rate) continue;
    if ((p->encoding_param >= 0) && (payload.encoding_param >= 0) && 
        (p->encoding_param != payload.encoding_param)) continue;
    return &(*p);
  }
  return NULL;
}

static bool containsPayload(const std::vector<SdpPayload>& payloads, const SdpPayload &payload)
{
  return findPayload(payloads, payload) != NULL;
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

    if (!containsPayload(norelay_payloads, *p)) {
      // this payload can be relayed

      TRACE("m1: marking payload %d for relay\n", p->payload_type);
      m1.set(p->payload_type);

      if (!use_m1 && containsPayload(transcoder_settings->audio_codecs, *p)) {
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
SBCCallLeg::SBCCallLeg(const SBCCallProfile& call_profile)
  : m_state(BB_Init),
    auth(NULL),
    call_profile(call_profile),
    cc_timer_id(SBC_TIMER_ID_CALL_TIMERS_START)
{
  set_sip_relay_only(false);
  dlg.setRel100State(Am100rel::REL100_IGNORED);

  // better here than in onInvite
  // or do we really want to start with OA when handling initial INVITE?
  dlg.setOAEnabled(false);

  memset(&call_connect_ts, 0, sizeof(struct timeval));
  memset(&call_end_ts, 0, sizeof(struct timeval));
}

// B leg constructor (from SBCCalleeSession)
SBCCallLeg::SBCCallLeg(SBCCallLeg* caller, const AmSipRequest &original_invite)
  : auth(NULL),
    call_profile(caller->getCallProfile()),
    CallLeg(caller)
{
  // FIXME: do we want to inherit cc_vars from caller?
  // Can be pretty dangerous when caller stored pointer to object - we should
  // not probably operate on it! But on other hand it could be handy for
  // something, so just take care when using stored objects...
  // call_profile.cc_vars.clear();

  dlg.setRel100State(Am100rel::REL100_IGNORED);
  dlg.setOAEnabled(false);

  // CC interfaces and variables should be already "evaluated" by A leg, we just
  // need to load the DI interfaces for us (later they will be initialized with
  // original INVITE so it must be done in A leg's thread!)
  if (!getCCInterfaces()) {
    throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  initCCModules(original_invite);
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

    if (call_profile.force_symmetric_rtp_value) {
      DBG("forcing symmetric RTP (passive mode)\n");
      rtp_relay_force_symmetric_rtp = true;
    }

    if (call_profile.aleg_rtprelay_interface_value >= 0) {
      setRtpRelayInterface(call_profile.aleg_rtprelay_interface_value);
      DBG("using RTP interface %i for A leg\n", rtp_interface);
    }

    setRtpRelayTransparentSeqno(call_profile.rtprelay_transparent_seqno);
    setRtpRelayTransparentSSRC(call_profile.rtprelay_transparent_ssrc);

    setRtpRelayMode(RTP_Relay);

    if(call_profile.transcoder.isActive()) {
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
  }
}

void SBCCallLeg::applyBProfile()
{
  if (!call_profile.contact.empty()) {
    dlg.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) + call_profile.contact + CRLF;
  }

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
    if (NULL == SBCFactory::session_timer_fact) {
      ERROR("session_timer module not loaded - unable to create call with SST\n");
      // FIXME: terminateOtherLeg?
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    AmSessionEventHandler* h = SBCFactory::session_timer_fact->getHandler(this);
    if(!h) {
      ERROR("could not get a session timer event handler\n");
      // FIXME: terminateOtherLeg?
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    if(h->configure(call_profile.sst_b_cfg)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      addHandler(h);
    }
  }

  if (!call_profile.outbound_proxy.empty()) {
    dlg.outbound_proxy = call_profile.outbound_proxy;
    dlg.force_outbound_proxy = call_profile.force_outbound_proxy;
  }

  if (!call_profile.next_hop.empty()) {
    dlg.next_hop = call_profile.next_hop;
  }

  // was read from caller but reading directly from profile now
  if (call_profile.outbound_interface_value >= 0)
    dlg.outbound_interface = call_profile.outbound_interface_value;

  // was read from caller but reading directly from profile now
  if (call_profile.rtprelay_enabled || call_profile.transcoder.isActive()) {
    if (call_profile.rtprelay_interface_value >= 0)
      setRtpRelayInterface(call_profile.rtprelay_interface_value);
  }

  setRtpRelayTransparentSeqno(call_profile.rtprelay_transparent_seqno);
  setRtpRelayTransparentSSRC(call_profile.rtprelay_transparent_ssrc);

  // was read from caller but reading directly from profile now
  if (!call_profile.callid.empty()) dlg.callid = call_profile.callid;
}

int SBCCallLeg::relayEvent(AmEvent* ev)
{
    switch (ev->event_id) {
      case B2BSipRequest:
        {
          B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);

          if (isActiveFilter(call_profile.headerfilter)) {
            //B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
            // header filter
            assert(req_ev);
            inplaceHeaderFilter(req_ev->req.hdrs,
                call_profile.headerfilter_list, call_profile.headerfilter);
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

          if (isActiveFilter(call_profile.headerfilter) ||
              call_profile.reply_translations.size()) {
            assert(reply_ev);
            // header filter
            if (isActiveFilter(call_profile.headerfilter)) {
              inplaceHeaderFilter(reply_ev->reply.hdrs,
                  call_profile.headerfilter_list,
                  call_profile.headerfilter);
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
  bool fwd = sip_relay_only &&
    //(req.method != "BYE") &&
    (req.method != "CANCEL");
  if (fwd) {
      CALL_EVENT_H(onSipRequest,req);
  }

  if (fwd && isActiveFilter(call_profile.messagefilter)) {
    bool is_filtered = (call_profile.messagefilter == Whitelist) ^ 
      (call_profile.messagefilter_list.find(req.method) != 
       call_profile.messagefilter_list.end());
    if (is_filtered) {
      DBG("replying 405 to filtered message '%s'\n", req.method.c_str());
      dlg.reply(req, 405, "Method Not Allowed", NULL, "", SIP_FLAGS_VERBATIM);
      return;
    }
  }

  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->onInDialogRequest(this, req) == StopProcessing) return;
  }

  CallLeg::onSipRequest(req);
}


void SBCCallLeg::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();

  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.body.getCTStr().c_str());
  if (fwd) {
      CALL_EVENT_H(onSipReply,reply, old_dlg_status);
  }

  if (NULL != auth) {
    // only for SIP authenticated
    unsigned int cseq_before = dlg.cseq;
    if (auth->onSipReply(reply, old_dlg_status)) {
      if (cseq_before != dlg.cseq) {
        DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
            "updating relayed_req map\n", reply.cseq, cseq_before);
        updateUACTransCSeq(reply.cseq, cseq_before);
      }
    }
  }

  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    if ((*i)->onInDialogReply(this, reply) == StopProcessing) return;
  }

  CallLeg::onSipReply(reply, old_dlg_status);
}

void SBCCallLeg::onSendRequest(AmSipRequest& req, int flags) {
  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", req.cseq);
    auth->onSendRequest(req, flags);
  }

  CallLeg::onSendRequest(req, flags);
}

void SBCCallLeg::onDtmf(int event, int duration)
{
  if(media_session) {
    DBG("received DTMF on %c-leg (%i;%i)\n",
	a_leg ? 'A': 'B', event, duration);
    media_session->sendDtmf(!a_leg,event,duration);
  }
}

bool SBCCallLeg::updateLocalSdp(AmSdp &sdp)
{
  // remember transcodable payload IDs
  if (call_profile.transcoder.isActive()) savePayloadIDs(sdp);
  return CallLeg::updateLocalSdp(sdp);
}


bool SBCCallLeg::updateRemoteSdp(AmSdp &sdp)
{
  SBCRelayController rc(&call_profile.transcoder, a_leg);
  if (call_profile.transcoder.isActive()) {
    if (media_session) return media_session->updateRemoteSdp(a_leg, sdp, &rc);
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


void SBCCallLeg::fixupCCInterface(const string& val, CCInterface& cc_if) {
  DBG("instantiating CC interface from '%s'\n", val.c_str());
  size_t spos, last = val.length() - 1;
  if (last < 0) {
      spos = string::npos;
      cc_if.cc_module = "";
  } else {
      spos = val.find(";", 0);
      cc_if.cc_module = val.substr(0, spos);
  }
  DBG("    module='%s'\n", cc_if.cc_module.c_str());
  while (spos < last) {
      size_t epos = val.find("=", spos + 1);
      if (epos == string::npos) {
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1), ""));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1).c_str(), "");
	  return;
      }
      if (epos == last) {
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), ""));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), "");
	  return;
      }
      // if value starts with " char, it continues until another " is found
      if (val[epos + 1] == '"') {
	  if (epos + 1 == last) {
	      cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), ""));
	      DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), "");
	      return;
	  }
	  size_t qpos = val.find('"', epos + 2);
	  if (qpos == string::npos) {
	      cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos -1), val.substr(epos + 2)));
	      DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 2).c_str());
	      return;
	  }
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), val.substr(epos + 2, qpos - epos - 2)));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 2, qpos - epos - 2).c_str());
	  if (qpos < last) {
	      spos = val.find(";", qpos + 1);
	  } else {
	      return;
	  }
      } else {
	  size_t new_spos = val.find(";", epos + 1);
	  if (new_spos == string::npos) {
	      cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), val.substr(epos + 1)));
	      DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 1).c_str());
	      return;
	  }
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), val.substr(epos + 1, new_spos - epos - 1)));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 1, new_spos - epos - 1).c_str());
	  spos = new_spos;
      }
  }
  return;
}

#define REPLACE_VALS req, app_param, ruri_parser, from_parser, to_parser

void SBCCallLeg::onInvite(const AmSipRequest& req)
{
  AmUriParser ruri_parser, from_parser, to_parser;

  DBG("processing initial INVITE %s\n", req.r_uri.c_str());

  string app_param = getHeader(req.hdrs, PARAM_HDR, true);

  // get start time for call control
  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_start_ts, NULL);
  }

  // process call control
  if (call_profile.cc_interfaces.size()) {
    unsigned int cc_dynif_count = 0;

    // fix up replacements in cc list
    CCInterfaceListIteratorT cc_rit = call_profile.cc_interfaces.begin();
    while (cc_rit != call_profile.cc_interfaces.end()) {
      CCInterfaceListIteratorT curr_if = cc_rit;
      cc_rit++;
      //      CCInterfaceListIteratorT next_cc = cc_rit+1;
      if (curr_if->cc_name.find('$') != string::npos) {
	vector<string> dyn_ccinterfaces =
	  explode(replaceParameters(curr_if->cc_name, "cc_interfaces", REPLACE_VALS), ",");
	if (!dyn_ccinterfaces.size()) {
	  DBG("call_control '%s' did not produce any call control instances\n",
	      curr_if->cc_name.c_str());
	  call_profile.cc_interfaces.erase(curr_if);
	} else {
	  // fill first CC interface (replacement item)
	  vector<string>::iterator it=dyn_ccinterfaces.begin();
	  curr_if->cc_name = "cc_dyn_"+int2str(cc_dynif_count++);
	  fixupCCInterface(trim(*it, " \t"), *curr_if);
	  it++;

	  // insert other CC interfaces (in order!)
	  while (it != dyn_ccinterfaces.end()) {
	    CCInterfaceListIteratorT new_cc =
	      call_profile.cc_interfaces.insert(cc_rit, CCInterface());
	    fixupCCInterface(trim(*it, " \t"), *new_cc);
	    new_cc->cc_name = "cc_dyn_"+int2str(cc_dynif_count++);
	    it++;
	  }
	}
      }
    }

    // fix up module names
    for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {
      cc_it->cc_module =
	replaceParameters(cc_it->cc_module, "cc_module", REPLACE_VALS);
    }

    if (!getCCInterfaces()) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    // fix up variables
    for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {
      CCInterface& cc_if = *cc_it;

      DBG("processing replacements for call control interface '%s'\n",
	  cc_if.cc_name.c_str());

      for (map<string, string>::iterator it = cc_if.cc_values.begin();
	   it != cc_if.cc_values.end(); it++) {
	it->second =
	  replaceParameters(it->second, it->first.c_str(), REPLACE_VALS);
      }
    }

    if (!CCStart(req)) {
      setStopped();
      return;
    }
  }

  if(dlg.reply(req, 100, "Connecting") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  if (!call_profile.evaluate(REPLACE_VALS)) {
    ERROR("call profile evaluation failed\n");
    throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  initCCModules(req);

  string ruri, to, from;
  ruri = call_profile.ruri.empty() ? req.r_uri : call_profile.ruri;
  if(!call_profile.ruri_host.empty()){
    ruri_parser.uri = ruri;
    if(!ruri_parser.parse_uri()) {
      WARN("Error parsing R-URI '%s'\n", ruri.c_str());
    }
    else {
      ruri_parser.uri_port.clear();
      ruri_parser.uri_host = call_profile.ruri_host;
      ruri = ruri_parser.uri_str();
    }
  }
  from = call_profile.from.empty() ? req.from : call_profile.from;
  to = call_profile.to.empty() ? req.to : call_profile.to;

  applyAProfile();

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

  inplaceHeaderFilter(invite_req.hdrs,
		      call_profile.headerfilter_list, call_profile.headerfilter);

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

void SBCCallLeg::connectCallee(const string& remote_party, const string& remote_uri,
    const string &from, const AmSipRequest &original_invite, const AmSipRequest &invite)
{
  // FIXME: no fork for now

  SBCCallLeg* callee_session = new SBCCallLeg(this, original_invite);
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
  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    string& cc_module = cc_it->cc_module;
    if (cc_module.empty()) {
      ERROR("using call control but empty cc_module for '%s'!\n", cc_it->cc_name.c_str());
      return false;
    }

    AmDynInvokeFactory* cc_fact = AmPlugIn::instance()->getFactory4Di(cc_module);
    if (NULL == cc_fact) {
      ERROR("cc_module '%s' not loaded\n", cc_module.c_str());
      return false;
    }

    AmDynInvoke* cc_di = cc_fact->getInstance();
    if(NULL == cc_di) {
      ERROR("could not get a DI reference\n");
      return false;
    }
    cc_modules.push_back(cc_di);

    // extended CC interface
    try {
      AmArg args, ret;
      cc_di->invoke("getExtendedInterfaceHandler", args, ret);
      ExtendedCCInterface *iface = dynamic_cast<ExtendedCCInterface*>(ret[0].asObject());
      if (iface) {
        DBG("extended CC interface offered by cc_module '%s'\n", cc_module.c_str());
        cc_ext.push_back(iface);
      }
      else WARN("BUG: returned invalid extended CC interface by cc_module '%s'\n", cc_module.c_str());
    }
    catch (...) {
      DBG("extended CC interface not supported by cc_module '%s'\n", cc_module.c_str());
    }
  }
  return true;
}

void SBCCallLeg::onCallConnected(const AmSipReply& reply) {
  if (a_leg) { // FIXME: really?
    m_state = BB_Connected;

    if (!startCallTimers())
      return;

    if (call_profile.cc_interfaces.size()) {
      gettimeofday(&call_connect_ts, NULL);
    }

    CCConnect(reply);
  }
}

void SBCCallLeg::onCallStopped() {
  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_end_ts, NULL);
  }

  if (m_state == BB_Connected) {
    stopCallTimers();
  }

  m_state = BB_Teardown;

  CCEnd();
}

void SBCCallLeg::onOtherBye(const AmSipRequest& req)
{
  onCallStopped();

  CallLeg::onOtherBye(req);
}

void SBCCallLeg::onSessionTimeout() {
  onCallStopped();

  CallLeg::onSessionTimeout();
}

void SBCCallLeg::onNoAck(unsigned int cseq) {
  onCallStopped();

  CallLeg::onNoAck(cseq);
}

void SBCCallLeg::onRemoteDisappeared(const AmSipReply& reply)  {
  DBG("Remote unreachable - ending SBC call\n");
  onCallStopped();

  CallLeg::onRemoteDisappeared(reply);
}

void SBCCallLeg::onBye(const AmSipRequest& req)
{
  DBG("onBye()\n");

  onCallStopped();

  CallLeg::onBye(req);
}

void SBCCallLeg::onCancel(const AmSipRequest& cancel)
{
  dlg.bye();
  stopCall();
}

void SBCCallLeg::onSystemEvent(AmSystemEvent* ev) {
  if (ev->sys_event == AmSystemEvent::ServerShutdown) {
    onCallStopped();
  }

  CallLeg::onSystemEvent(ev);
}

void SBCCallLeg::stopCall() {
  terminateOtherLeg();
  terminateLeg();
  onCallStopped();
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

    try {
      (*cc_mod)->invoke("start", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface start "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      dlg.reply(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);

      // call 'end' of call control modules up to here
      call_end_ts.tv_sec = call_start_ts.tv_sec;
      call_end_ts.tv_usec = call_start_ts.tv_usec;
      CCEnd(cc_it);

      return false;
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface start "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      dlg.reply(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);

      // call 'end' of call control modules up to here
      call_end_ts.tv_sec = call_start_ts.tv_sec;
      call_end_ts.tv_usec = call_start_ts.tv_usec;
      CCEnd(cc_it);

      return false;
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
	  dlg.setStatus(AmSipDialog::Disconnected);

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

	  dlg.reply(req,
		    ret[i][SBC_CC_REFUSE_CODE].asInt(), ret[i][SBC_CC_REFUSE_REASON].asCStr(),
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
  return true;
}

void SBCCallLeg::CCConnect(const AmSipReply& reply) {
  if (!a_leg) return; // preserve original behavior of the CC interface

  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);                // cc name
    di_args.push(getLocalTag());                 // call ltag
    di_args.push((AmObject*)&call_profile);     // call profile
    di_args.push(AmArg());                       // timestamps
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    di_args.back().push((int)call_connect_ts.tv_sec);
    di_args.back().push((int)call_connect_ts.tv_usec);
    for (int i=0;i<2;i++)
      di_args.back().push((int)0);
    di_args.push(other_id);                      // other leg ltag


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
  if (!a_leg) return; // preserve original behavior of the CC interface

  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != end_interface; cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);
    di_args.push(getLocalTag());                 // call ltag
    di_args.push((AmObject*)&call_profile);
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

void SBCCallLeg::onCallStatusChange()
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    (*i)->onStateChange(this);
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

  if (prefer_existing_codecs) {
    // We have to order payloads before adding transcoder codecs to leave
    // transcoding as the last chance (existing codecs are preferred thus
    // relaying will be used if possible).
    if (call_profile.codec_prefs.shouldOrderPayloads(a_leg)) {
      normalizeSDP(sdp, call_profile.anonymize_sdp);
      call_profile.codec_prefs.orderSDP(sdp, a_leg);
      changed = true;
    }
  }

  // Add transcoder codecs before filtering because otherwise SDP filter could
  // inactivate some media lines which shouldn't be inactivated.

  if (call_profile.transcoder.isActive()) {
    if (!changed) // otherwise already normalized
      normalizeSDP(sdp, call_profile.anonymize_sdp);
    appendTranscoderCodecs(sdp);
    changed = true;
  }

  if (!prefer_existing_codecs) {
    // existing codecs are not preferred - reorder AFTER adding transcoder
    // codecs so it might happen that transcoding will be forced though relaying
    // would be possible
    if (call_profile.codec_prefs.shouldOrderPayloads(a_leg)) {
      if (!changed) normalizeSDP(sdp, call_profile.anonymize_sdp);
      call_profile.codec_prefs.orderSDP(sdp, a_leg);
      changed = true;
    }
  }

  // It doesn't make sense to filter out codecs allowed for transcoding and thus
  // if the filter filters them out it can be considered as configuration
  // problem, right?
  // => So we wouldn't try to avoid filtering out transcoder codecs what would
  // just complicate things.

  if (call_profile.sdpfilter_enabled) {
    if (!changed) // otherwise already normalized
      normalizeSDP(sdp, call_profile.anonymize_sdp);
    if (isActiveFilter(call_profile.sdpfilter)) {
      res = filterSDP(sdp, call_profile.sdpfilter, call_profile.sdpfilter_list);
    }
    changed = true;
  }
  if (call_profile.sdpalinesfilter_enabled &&
      isActiveFilter(call_profile.sdpalinesfilter)) {
    // filter SDP "a=lines"
    filterSDPalines(sdp, call_profile.sdpalinesfilter, call_profile.sdpalinesfilter_list);
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
        if (containsPayload(transcoder_codecs, *p)) transcodable = true;
        used_payloads.set(p->payload_type);
      }

      if (transcodable) {
        // there are some transcodable codecs present in the SDP, we can safely
        // add the other transcoder codecs to the SDP
        unsigned idx = 0;
        for (p = transcoder_codecs.begin(); p != transcoder_codecs.end(); ++p, ++idx) {
          // add all payloads which are not already there
          if (!containsPayload(m->payloads, *p)) {
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
        const SdpPayload *pp = findPayload(m->payloads, *p);
        if (pp && (pp->payload_type >= 0))
          transcoder_payload_mapping.map(stream_idx, idx, pp->payload_type);
      }
    }

    stream_idx++; // count chosen media type only
  }
}

void SBCCallLeg::terminateLeg()
{
  CallLeg::terminateLeg();

  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    (*i)->onTerminateLeg(this);
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

  if (dlg.reinvite("", &body, SIP_FLAGS_VERBATIM) != 0) return false;
  request_cseq = dlg.cseq - 1;
  return true;
}

void SBCCallLeg::changeRtpMode(RTPRelayMode new_mode)
{
  if (new_mode == rtp_relay_mode) return; // requested mode is set already

  if (getCallStatus() != CallLeg::Connected /*other_id.empty()*/) {
    ERROR("BUG: changeRtpMode supported for established calls only\n");
    return;
  }

  // we don't need to send reINVITE from here, expecting caller knows what is he
  // doing (it is probably processing or generating its own reINVITE)
  // Switch from RTP_Direct to RTP_Relay is safe (no audio loss), the other can
  // be lossy because already existing media object would be destroyed.
  // FIXME: use AmB2BMedia in all RTP relay modes to avoid these problems?
  switch (rtp_relay_mode) {
    case RTP_Relay:
      clearRtpReceiverRelay();
      break;

    case RTP_Direct:
      // create new blablabla
      setMediaSession(new AmB2BMedia(a_leg ? this: NULL, a_leg ? NULL : this));
      break;
  }

  relayEvent(new ChangeRtpModeEvent(new_mode, getMediaSession()));
  setRtpRelayMode(new_mode);
}

void SBCCallLeg::initCCModules(const AmSipRequest &original_invite)
{
  for (vector<ExtendedCCInterface*>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    (*i)->init(this, original_invite);
  }
}

void SBCCallLeg::onB2BEvent(B2BEvent* ev)
{
  if (ev->event_id == ChangeRtpModeEventId) {
    INFO("*** B2B request to change RTP mode\n");
    ChangeRtpModeEvent *e = dynamic_cast<ChangeRtpModeEvent*>(ev);
    if (e) {
      if (e->new_mode == rtp_relay_mode) return; // requested mode is set already

      switch (rtp_relay_mode) {
        case RTP_Relay:
          clearRtpReceiverRelay();
          break;

        case RTP_Direct:
          // create new blablabla
          setMediaSession(e->media);
          media_session->changeSession(a_leg, this);
          break;
      }
      setRtpRelayMode(e->new_mode);
    }
  }

  CallLeg::onB2BEvent(ev);
}
