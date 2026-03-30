/*
 * ED-137B Ground Radio Station (GRS) Simulator for SEMS
 */

#include "Ed137bGrsSim.h"
#include "Ed137bGrsSipHelper.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmSdp.h"
#include "sems.h"
#include "log.h"

#define MOD_NAME "ed137b_grs_sim"

EXPORT_SESSION_FACTORY(Ed137bGrsSimFactory, MOD_NAME);

// --- Static members ---

Ed137bGrsState     Ed137bGrsSimFactory::DefaultState;
string             Ed137bGrsSimFactory::AudioMode = "tone";
int                Ed137bGrsSimFactory::ToneFreq  = 400;
Ed137bGrsChangeLogger Ed137bGrsSimFactory::Logger;

// --- Factory ---

Ed137bGrsSimFactory::Ed137bGrsSimFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int Ed137bGrsSimFactory::onLoad()
{
  AmConfigReader cfg;
  if (cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    WARN("ED137B-GRS: no config file found, using defaults\n");
  }

  configureModule(cfg);

  // Load GRS defaults
  DefaultState.frequency       = cfg.getParameter("frequency", "118.000");
  DefaultState.channel_spacing = cfg.getParameter("channel_spacing", "25kHz");
  DefaultState.txrx_mode       = cfg.getParameter("txrx_mode", "RxTx");
  DefaultState.radio_type      = cfg.getParameter("radio_type", "Radio");
  DefaultState.wg67_version    = cfg.getParameter("wg67_version", "radio.01.00");
  DefaultState.priority        = cfg.getParameter("priority", "normal");
  DefaultState.bss             = cfg.getParameter("bss", "");
  DefaultState.squelch_ctrl    = cfg.getParameter("squelch_control", "");
  DefaultState.climax          = (cfg.getParameter("climax", "no") == "yes");

  // Audio config
  AudioMode = cfg.getParameter("audio_mode", "tone");
  string tf = cfg.getParameter("tone_freq", "400");
  ToneFreq  = atoi(tf.c_str());
  if (ToneFreq <= 0) ToneFreq = 400;

  // Open log file
  string log_path = cfg.getParameter("log_file", "/var/log/sems/ed137b_grs_sim.log");
  if (!Logger.open(log_path)) {
    WARN("ED137B-GRS: file logging disabled\n");
  }

  INFO("ED137B-GRS: loaded -- freq=%s mode=%s spacing=%s wg67=%s audio=%s\n",
       DefaultState.frequency.c_str(),
       DefaultState.txrx_mode.c_str(),
       DefaultState.channel_spacing.c_str(),
       DefaultState.wg67_version.c_str(),
       AudioMode.c_str());

  return 0;
}

AmSession* Ed137bGrsSimFactory::onInvite(const AmSipRequest& req,
                                         const string& app_name,
                                         const map<string, string>& app_params)
{
  return new Ed137bGrsSimSession();
}

// --- Session ---

Ed137bGrsSimSession::Ed137bGrsSimSession()
  : tone(NULL), session_started(false)
{
  state = Ed137bGrsSimFactory::DefaultState;
}

Ed137bGrsSimSession::~Ed137bGrsSimSession()
{
  delete tone;
}

void Ed137bGrsSimSession::onInvite(const AmSipRequest& req)
{
  // Parse incoming ED-137B parameters and log changes
  updateStateFromRequest(req);

  // Accept the call
  dlg->reply(req, 200, "OK");
}

void Ed137bGrsSimSession::onSessionStart()
{
  DBG("ED137B-GRS: session start\n");

  // R2S audio: continuous side-tone or silence
  if (Ed137bGrsSimFactory::AudioMode == "tone") {
    tone = new AmRingTone(0, 1000, 0, Ed137bGrsSimFactory::ToneFreq);
    setOutput(tone);
  }

  // GRS sessions are always-on -- disable RTP timeout
  RTPStream()->setMonitorRTPTimeout(false);

  session_started = true;

  Ed137bGrsSimFactory::Logger.logSessionStart(dlg->getCallid(), state);
  INFO("ED137B-GRS: [%s] session started freq=%s mode=%s\n",
       dlg->getCallid().c_str(),
       state.frequency.c_str(),
       state.txrx_mode.c_str());

  AmSession::onSessionStart();
}

void Ed137bGrsSimSession::onBye(const AmSipRequest& req)
{
  Ed137bGrsSimFactory::Logger.logSessionEnd(dlg->getCallid());
  INFO("ED137B-GRS: [%s] session ended\n", dlg->getCallid().c_str());

  AmSession::onBye(req);
}

void Ed137bGrsSimSession::onSipRequest(const AmSipRequest& req)
{
  // Handle UPDATE with body (GRS parameter changes)
  if (req.method == "UPDATE" && !req.body.empty()) {
    updateStateFromRequest(req);
    dlg->reply(req, 200, "OK");
    return;
  }

  AmSession::onSipRequest(req);
}

// --- SDP hooks ---

bool Ed137bGrsSimSession::getSdpOffer(AmSdp& offer)
{
  if (!AmSession::getSdpOffer(offer))
    return false;

  applyEd137bSdp(offer);
  return true;
}

bool Ed137bGrsSimSession::getSdpAnswer(const AmSdp& offer, AmSdp& answer)
{
  if (!AmSession::getSdpAnswer(offer, answer))
    return false;

  applyEd137bSdp(answer);
  return true;
}

// --- SIP send hooks ---

void Ed137bGrsSimSession::onSendRequest(AmSipRequest& req, int& flags)
{
  req.hdrs += Ed137bGrsSipHelper::buildHeaders(state.wg67_version, state.priority);
  AmSession::onSendRequest(req, flags);
}

void Ed137bGrsSimSession::onSendReply(const AmSipRequest& req,
                                      AmSipReply& reply, int& flags)
{
  if (reply.code >= 200 && reply.code < 300) {
    reply.hdrs += Ed137bGrsSipHelper::buildHeaders(state.wg67_version, state.priority);
  }
  AmSession::onSendReply(req, reply, flags);
}

// --- Private helpers ---

void Ed137bGrsSimSession::updateStateFromRequest(const AmSipRequest& req)
{
  Ed137bGrsState new_state = state;

  // Parse SIP headers
  string wg67 = Ed137bGrsSipHelper::extractHeader(req.hdrs, ED137B_HDR_WG67_VERSION);
  if (!wg67.empty()) new_state.wg67_version = wg67;

  string prio = Ed137bGrsSipHelper::extractHeader(req.hdrs, ED137B_HDR_PRIORITY);
  if (!prio.empty()) new_state.priority = prio;

  // Parse SDP body for ED-137B attributes
  AmSdp sdp;
  const AmMimeBody* sdp_body = req.body.hasContentType("application/sdp");
  if (sdp_body) {
    string sdp_str((const char*)sdp_body->getPayload(), sdp_body->getLen());
    if (sdp.parse(sdp_str.c_str()) == 0) {
      map<string, string> attrs = Ed137bGrsSipHelper::parseSdpAttributes(sdp);
      new_state.fromSdpMap(attrs);
    } else {
      WARN("ED137B-GRS: failed to parse SDP body\n");
    }
  }

  if (session_started) {
    Ed137bGrsSimFactory::Logger.logChanges(dlg->getCallid(), state, new_state);
  }

  state = new_state;
}

void Ed137bGrsSimSession::applyEd137bSdp(AmSdp& sdp)
{
  Ed137bGrsSipHelper::addSdpAttributes(
    sdp,
    state.radio_type,
    state.frequency,
    state.txrx_mode,
    state.channel_spacing,
    state.bss,
    state.squelch_ctrl,
    state.climax
  );
  Ed137bGrsSipHelper::addR2sFmtp(sdp);
}

void Ed137bGrsSimSession::process(AmEvent* event)
{
  // GRS sessions stay alive -- ignore audio cleared events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(event);
  if (audio_ev && audio_ev->event_id == AmAudioEvent::cleared) {
    return;
  }

  AmSession::process(event);
}
