/*
 * SIPREC (RFC 7865/7866) Session Recording Client for SEMS SBC.
 */

#include "AmPlugIn.h"
#include "AmArg.h"
#include "AmConfig.h"
#include "AmConfigReader.h"
#include "AmRtpPacket.h"
#include "AmSdp.h"
#include "log.h"

#include "CCSiprec.h"
#include "SBCCallControlAPI.h"

#include <string.h>

#define SIPREC_CC_VARS "siprec::state"

class CCSiprecFactory : public AmDynInvokeFactory
{
public:
  CCSiprecFactory(const string& name)
    : AmDynInvokeFactory(name) {}

  AmDynInvoke* getInstance() {
    return CCSiprec::instance();
  }

  int onLoad() {
    if (CCSiprec::instance()->onLoad())
      return -1;
    DBG("SIPREC call control loaded.\n");
    return 0;
  }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCSiprecFactory, MOD_NAME);

CCSiprec* CCSiprec::_instance = 0;

CCSiprec* CCSiprec::instance() {
  if (!_instance)
    _instance = new CCSiprec();
  return _instance;
}

CCSiprec::CCSiprec()
  : recording_mandatory_(false),
    codec_name_("PCMA"),
    codec_payload_type_(8),
    codec_clock_rate_(8000),
    transport_(TP_RTPAVP)
{
}

CCSiprec::~CCSiprec() {
  // Cleanup remaining sessions
  AmLock lock(sessions_mut_);
  for (map<string, SiprecCallState*>::iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    delete it->second;
  }
  sessions_.clear();
}

int CCSiprec::onLoad() {
  AmConfigReader cfg;

  if (cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    INFO(MOD_NAME " configuration file (%s) not found, "
         "assuming default configuration\n",
         (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
    return 0;
  }

  srs_uri_ = cfg.getParameter("srs_uri", "");
  recording_mandatory_ = cfg.getParameter("recording_mandatory", "no") == "yes";

  string codec = cfg.getParameter("codec", "PCMA");
  if (codec == "PCMU") {
    codec_name_ = "PCMU";
    codec_payload_type_ = 0;
    codec_clock_rate_ = 8000;
  } else if (codec == "PCMA") {
    codec_name_ = "PCMA";
    codec_payload_type_ = 8;
    codec_clock_rate_ = 8000;
  } else {
    WARN("SIPREC: unknown codec '%s', falling back to PCMA\n", codec.c_str());
    codec_name_ = "PCMA";
    codec_payload_type_ = 8;
    codec_clock_rate_ = 8000;
  }

  // RTP transport profile (RFC 7866 Section 12.2)
  string transport = cfg.getParameter("rtp_transport", "RTP/AVP");
  if (transport == "RTP/SAVP") {
    transport_ = TP_RTPSAVP;
  } else if (transport == "RTP/SAVPF") {
    transport_ = TP_RTPSAVPF;
  } else if (transport == "RTP/AVPF") {
    transport_ = TP_RTPAVPF;
  } else {
    transport_ = TP_RTPAVP;
  }

  // Port range for SRC-side RTP (for symmetric RTP binding)
  unsigned short rtp_port_min = 50000;
  unsigned short rtp_port_max = 50999;
  string port_min_str = cfg.getParameter("rtp_port_min", "50000");
  string port_max_str = cfg.getParameter("rtp_port_max", "50999");
  if (!port_min_str.empty()) rtp_port_min = (unsigned short)atoi(port_min_str.c_str());
  if (!port_max_str.empty()) rtp_port_max = (unsigned short)atoi(port_max_str.c_str());
  SiprecPortAllocator::configure(rtp_port_min, rtp_port_max);

  if (srs_uri_.empty()) {
    ERROR("SIPREC: srs_uri is not configured in cc_siprec.conf\n");
    return -1;
  }

  INFO("SIPREC: loaded (srs_uri='%s', mandatory=%s, codec=%s, "
       "transport=%s, ports=%u-%u)\n",
       srs_uri_.c_str(), recording_mandatory_ ? "yes" : "no",
       codec_name_.c_str(), transport == "RTP/SAVP" ? "RTP/SAVP" : "RTP/AVP",
       rtp_port_min, rtp_port_max);

  return 0;
}

void CCSiprec::invoke(const string& method, const AmArg& args, AmArg& ret) {
  if (method == "start") {
    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    start(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
          args[CC_API_PARAMS_LTAG].asCStr(),
          call_profile,
          args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_SEC].asInt(),
          args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_USEC].asInt(),
          args[CC_API_PARAMS_CFGVALUES],
          args[CC_API_PARAMS_TIMERID].asInt(), ret);

  } else if (method == "connect") {
    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    connect(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
            args[CC_API_PARAMS_LTAG].asCStr(),
            call_profile,
            args[CC_API_PARAMS_OTHERID].asCStr(),
            args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
            args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());

  } else if (method == "end") {
    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    end(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
        args[CC_API_PARAMS_LTAG].asCStr(),
        call_profile,
        args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
        args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt());

  } else if (method == "route") {
    // SIPREC does not act on out-of-dialog requests (REGISTER, OPTIONS, etc.)

  } else if (method == "getExtendedInterfaceHandler") {
    ret.push((AmObject*)this);

  } else if (method == "_list") {
    ret.push("start");
    ret.push("connect");
    ret.push("end");
    ret.push("route");

  } else {
    throw AmDynInvoke::NotImplemented(method);
  }
}

// --- DynInvoke callbacks ---

void CCSiprec::start(const string& cc_name, const string& ltag,
                     SBCCallProfile* call_profile,
                     int start_ts_sec, int start_ts_usec,
                     const AmArg& values, int timer_id, AmArg& res) {
  if (!call_profile) return;

  // Check per-profile enable flag (default: yes)
  if (values.hasMember("enable") &&
      string(values["enable"].asCStr()) == "no") {
    DBG("SIPREC: recording disabled for this profile\n");
    return;
  }

  // Store SRS URI in cc_vars for the connect callback
  call_profile->cc_vars[string(SIPREC_CC_VARS) + "::srs_uri"] = srs_uri_;

  DBG("SIPREC: start - call %s, srs=%s\n", ltag.c_str(), srs_uri_.c_str());
}

void CCSiprec::connect(const string& cc_name, const string& ltag,
                       SBCCallProfile* call_profile,
                       const string& other_ltag,
                       int connect_ts_sec, int connect_ts_usec) {
  // Recording session is initiated on connect via onStateChange (ExtendedCC)
  // This callback is for DynInvoke compatibility only
  DBG("SIPREC: connect - call %s, other=%s\n", ltag.c_str(), other_ltag.c_str());
}

void CCSiprec::end(const string& cc_name, const string& ltag,
                   SBCCallProfile* call_profile,
                   int end_ts_sec, int end_ts_usec) {
  DBG("SIPREC: end - call %s\n", ltag.c_str());
  // Cleanup is handled by onDestroyLeg (ExtendedCC)
}

// --- Extended CC Interface ---

bool CCSiprec::init(SBCCallLeg *call, const map<string, string> &values) {
  DBG("SIPREC: init - call %p, isALeg=%s\n",
      call, call->isALeg() ? "true" : "false");
  return true;
}

CCChainProcessing CCSiprec::onInitialInvite(SBCCallLeg *call,
                                            InitialInviteHandlerParams &params) {
  if (!call->isALeg()) return ContinueProcessing;

  // Extract caller/callee URIs for metadata
  SBCCallProfile& prof = call->getCallProfile();
  prof.cc_vars[string(SIPREC_CC_VARS) + "::caller"] =
    params.from.empty() ? call->dlg->getLocalParty() : params.from;
  prof.cc_vars[string(SIPREC_CC_VARS) + "::callee"] =
    params.remote_party.empty() ? params.remote_uri : params.remote_party;

  // H8: Extract CS SDP from the initial INVITE for codec negotiation
  if (params.original_invite && params.original_invite->body.getLen()) {
    const AmMimeBody* sdp_body =
      params.original_invite->body.hasContentType(SIP_APPLICATION_SDP);
    if (sdp_body) {
      AmSdp cs_sdp;
      if (cs_sdp.parse((const char*)sdp_body->getPayload()) == 0) {
        // Store the first audio codec info for the recording SDP offer
        for (size_t i = 0; i < cs_sdp.media.size(); i++) {
          if (cs_sdp.media[i].type == MT_AUDIO &&
              !cs_sdp.media[i].payloads.empty()) {
            const SdpPayload& p = cs_sdp.media[i].payloads[0];
            prof.cc_vars[string(SIPREC_CC_VARS) + "::codec_pt"] =
              int2str(p.payload_type);
            if (!p.encoding_name.empty())
              prof.cc_vars[string(SIPREC_CC_VARS) + "::codec_name"] =
                p.encoding_name;
            if (p.clock_rate > 0)
              prof.cc_vars[string(SIPREC_CC_VARS) + "::codec_rate"] =
                int2str(p.clock_rate);
            DBG("SIPREC: CS SDP codec: %s/%d pt=%d\n",
                p.encoding_name.c_str(), p.clock_rate, p.payload_type);
            break;
          }
        }
      }
    }
  }

  // M10: Recording indication for B-leg (RFC 7866 Section 6.1.2)
  // Inject a=record:on into the SDP going to the B-leg (callee)
  if (params.modified_invite && params.modified_invite->body.getLen()) {
    AmMimeBody* sdp_body =
      params.modified_invite->body.hasContentType(SIP_APPLICATION_SDP);
    if (sdp_body) {
      AmSdp sdp;
      if (sdp.parse((const char*)sdp_body->getPayload()) == 0) {
        // Add session-level recording indication
        sdp.attributes.push_back(SdpAttribute("record", "on"));
        string sdp_str;
        sdp.print(sdp_str);
        sdp_body->setPayload((const unsigned char*)sdp_str.c_str(),
                             sdp_str.length());
        DBG("SIPREC: injected a=record:on into CS B-leg INVITE\n");
      }
    }
  }

  DBG("SIPREC: onInitialInvite - caller='%s', callee='%s'\n",
      prof.cc_vars[string(SIPREC_CC_VARS) + "::caller"].asCStr(),
      prof.cc_vars[string(SIPREC_CC_VARS) + "::callee"].asCStr());

  return ContinueProcessing;
}

CCChainProcessing CCSiprec::onInDialogReply(SBCCallLeg *call,
                                            const AmSipReply &reply) {
  // M10: Recording indication for A-leg (RFC 7866 Section 6.1.2)
  // When the 200 OK from B-leg is relayed to A-leg, we would inject
  // a=record:on. However, modifying the relayed reply's SDP requires
  // deeper B2BUA integration. For now, the A-leg recording indication
  // is handled by deployment policy (e.g., contractual agreement or
  // tone injection via a separate module).
  return ContinueProcessing;
}

void CCSiprec::onStateChange(SBCCallLeg *call,
                             const CallLeg::StatusChangeCause &cause) {
  CallLeg::CallStatus status = call->getCallStatus();

  if (status == CallLeg::Connected && call->isALeg()) {
    // Call just connected - start recording
    SBCCallProfile& prof = call->getCallProfile();

    string srs_uri;
    SBCVarMapIteratorT it = prof.cc_vars.find(string(SIPREC_CC_VARS) + "::srs_uri");
    if (it != prof.cc_vars.end())
      srs_uri = it->second.asCStr();

    if (srs_uri.empty()) {
      DBG("SIPREC: no SRS URI, skipping recording\n");
      return;
    }

    string caller_uri, callee_uri;
    it = prof.cc_vars.find(string(SIPREC_CC_VARS) + "::caller");
    if (it != prof.cc_vars.end()) caller_uri = it->second.asCStr();
    it = prof.cc_vars.find(string(SIPREC_CC_VARS) + "::callee");
    if (it != prof.cc_vars.end()) callee_uri = it->second.asCStr();

    string ltag = call->getLocalTag();
    string call_id = call->getCallID();

    // H8: Build CS SDP from stored codec info (extracted in onInitialInvite)
    AmSdp cs_sdp;
    it = prof.cc_vars.find(string(SIPREC_CC_VARS) + "::codec_pt");
    if (it != prof.cc_vars.end()) {
      SdpMedia m;
      m.type = MT_AUDIO;
      SdpPayload p;
      p.payload_type = atoi(it->second.asCStr());

      it = prof.cc_vars.find(string(SIPREC_CC_VARS) + "::codec_name");
      if (it != prof.cc_vars.end()) p.encoding_name = it->second.asCStr();

      it = prof.cc_vars.find(string(SIPREC_CC_VARS) + "::codec_rate");
      if (it != prof.cc_vars.end()) p.clock_rate = atoi(it->second.asCStr());

      m.payloads.push_back(p);
      cs_sdp.media.push_back(m);
      DBG("SIPREC: using CS codec %s/%d pt=%d for recording SDP\n",
          p.encoding_name.c_str(), p.clock_rate, p.payload_type);
    }

    // Create recording state
    SiprecCallState* state = new SiprecCallState();
    state->call_id = call_id;
    state->caller_uri = caller_uri;
    state->callee_uri = callee_uri;
    state->a_leg_ltag = ltag;
    state->b_leg_ltag = call->getOtherId();
    state->is_aleg = true;

    // Create SIPREC dialog
    state->dialog = new SiprecDialog(this, ltag);
    state->dialog->setCodec(codec_name_, codec_payload_type_, codec_clock_rate_);
    state->dialog->setTransport(transport_);

    {
      AmLock lock(sessions_mut_);
      // Remove old state if exists
      map<string, SiprecCallState*>::iterator old = sessions_.find(ltag);
      if (old != sessions_.end()) {
        delete old->second;
        sessions_.erase(old);
      }
      sessions_[ltag] = state;
    }

    // Start recording session
    int ret = state->dialog->startRecording(srs_uri, caller_uri, callee_uri,
                                            cs_sdp, call_id);
    if (ret != 0) {
      ERROR("SIPREC: failed to start recording for call %s\n", ltag.c_str());
      removeState(ltag);
    }
  }

  if (status == CallLeg::Disconnected) {
    string ltag = call->getLocalTag();
    SiprecCallState* state = getState(ltag);
    if (state && state->dialog) {
      state->dialog->stopRecording();
    }
  }
}

void CCSiprec::onDestroyLeg(SBCCallLeg *call) {
  string ltag = call->getLocalTag();
  DBG("SIPREC: onDestroyLeg - %s\n", ltag.c_str());
  removeState(ltag);
}

void CCSiprec::onAfterRTPRelay(SBCCallLeg *call, AmRtpPacket* p,
                                sockaddr_storage* remote_addr) {
  string ltag = call->getLocalTag();
  bool is_aleg = call->isALeg();

  // Look up state - try the A-leg tag first (recording state is keyed by A-leg)
  SiprecCallState* state = NULL;
  {
    AmLock lock(sessions_mut_);
    map<string, SiprecCallState*>::iterator it;

    if (is_aleg) {
      it = sessions_.find(ltag);
    } else {
      // For B-leg, search by b_leg_ltag
      for (it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (it->second->b_leg_ltag == ltag) break;
      }
    }
    if (it != sessions_.end()) state = it->second;
  }

  if (state && state->dialog) {
    state->dialog->forwardRtp(p, is_aleg);
  }
}

void CCSiprec::holdRequested(SBCCallLeg *call) {
  if (!call->isALeg()) return;
  string ltag = call->getLocalTag();
  SiprecCallState* state = getState(ltag);
  if (state && state->dialog) {
    AmSdp empty_sdp;
    state->dialog->updateRecording(empty_sdp, true);
  }
}

void CCSiprec::resumeAccepted(SBCCallLeg *call) {
  if (!call->isALeg()) return;
  string ltag = call->getLocalTag();
  SiprecCallState* state = getState(ltag);
  if (state && state->dialog) {
    AmSdp empty_sdp;
    state->dialog->updateRecording(empty_sdp, false);
  }
}

// --- Internal helpers ---

SiprecCallState* CCSiprec::getState(const string& ltag) {
  AmLock lock(sessions_mut_);
  map<string, SiprecCallState*>::iterator it = sessions_.find(ltag);
  if (it != sessions_.end()) return it->second;

  // Also search by B-leg tag
  for (it = sessions_.begin(); it != sessions_.end(); ++it) {
    if (it->second->b_leg_ltag == ltag) return it->second;
  }
  return NULL;
}

void CCSiprec::removeState(const string& ltag) {
  AmLock lock(sessions_mut_);
  map<string, SiprecCallState*>::iterator it = sessions_.find(ltag);
  if (it != sessions_.end()) {
    delete it->second;
    sessions_.erase(it);
    return;
  }

  // Also search by B-leg tag
  for (it = sessions_.begin(); it != sessions_.end(); ++it) {
    if (it->second->b_leg_ltag == ltag) {
      delete it->second;
      sessions_.erase(it);
      return;
    }
  }
}
