/*
 * Copyright (C) 2013 Stefan Sayer
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

#include "SBCDSMInstance.h"
#include "SBCCallLeg.h"
#include "SBCSimpleRelay.h"

#include "DSM.h"
#include "SBCDSMParams.h"

#include "AmAdvancedAudio.h"

#include <algorithm>

using namespace std;

SBCDSMInstance::SBCDSMInstance(SBCCallLeg *call, const VarMapT& values)
  : call(call), local_media_connected(false)
{
  DBG("SBCDSMInstance::SBCDSMInstance()\n");
  var = values;

  startDiagName = var[DSM_SBC_CCVAR_START_DIAG];
  appBundle = var[DSM_SBC_CCVAR_APP_BUNDLE];

  if (startDiagName.empty()) {
    throw string("DSM SBC call control "DSM_SBC_CCVAR_START_DIAG" parameter not set (see call profile)'");
  }
  
  map<string,string> config_vars;
  bool SetParamVariables; // unused

  if (!DSMFactory::instance()->
      addScriptDiagsToEngine(appBundle,
			     &engine,
			     config_vars,
			     SetParamVariables)) {
    ERROR("initializing call with DSM app bundle '%s'\n", appBundle.c_str());
    throw string("initializing call with DSM app bundle '" +appBundle);
  }

 for (map<string, string>::const_iterator it = 
	 config_vars.begin(); it != config_vars.end(); it++) 
    var["config."+it->first] = it->second;

  DBG("Running init of SBCDSMInstance...\n");
  if (!engine.init(call, this, startDiagName, DSMCondition::Start)) {
    WARN("Initialization failed for SBCDSMInstance\n");
    // TODO: mark this as not running!
    return;
  }
}

SBCDSMInstance::~SBCDSMInstance()
{
  DBG("SBCDSMInstance::~SBCDSMInstance()\n");
  for (std::set<DSMDisposable*>::iterator it=
	 gc_trash.begin(); it != gc_trash.end(); it++)
    delete *it;

  for (vector<AmAudio*>::iterator it=
	 audiofiles.begin();it!=audiofiles.end();it++) 
    delete *it;

  AmMediaProcessor::instance()->removeSession(call);
}

#define RETURN_CONTINUE_OR_STOP_PROCESSING			     \
  if (event_params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)	     \
    return StopProcessing;					     \
  return ContinueProcessing;

/** @return whether to continue processing */
    /** called from A/B leg when in-dialog request comes in */
CCChainProcessing SBCDSMInstance::onInitialInvite(SBCCallLeg *call, InitialInviteHandlerParams &params)
{
  DBG("SBCDSMInstance::onInitialInvite()\n");

  VarMapT event_params;
  event_params["remote_party"] = params.remote_party;
  event_params["remote_uri"] = params.remote_party;
  event_params["from"] = params.remote_party;

  avar[DSM_AVAR_REQUEST] = AmArg(params.original_invite);
  avar[DSM_SBC_AVAR_MODIFIED_INVITE] = AmArg(params.modified_invite);

  engine.runEvent(call, this, DSMCondition::Invite, &event_params);

  avar.erase(DSM_SBC_AVAR_MODIFIED_INVITE);

  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

void extractRequestParameters(VarMapT& event_params, AVarMapT& avar, DSMSipRequest* request) {
  if (NULL == request)
    return;

  if (NULL != request) {
    event_params["method"] = request->req->method;
    event_params["r_uri"] = request->req->r_uri;
    event_params["from"] = request->req->from;
    event_params["to"] = request->req->to;
    event_params["hdrs"] = request->req->hdrs;
    event_params["from_tag"] = request->req->from_tag;
    event_params["to_tag"] = request->req->to_tag;
    event_params["callid"] = request->req->callid;

    vector<string> hdrs = explode(request->req->hdrs, CRLF);
    for (vector<string>::iterator it=hdrs.begin(); it!=hdrs.end();it++) {
      size_t p = it->find(":");
      if (p==string::npos)
	continue;
      size_t p1=p;
      if (++p>=it->size())
	continue;
      while (p<it->size() && ((*it)[p] == ' ' || (*it)[p] == '\t'))
	p++;
      event_params["hdr."+it->substr(0,p1)]=it->substr(p);
    }

    // avar[DSM_AVAR_REQUEST] = AmArg(const_cast<AmSipRequest*>(request));
    avar[DSM_AVAR_REQUEST] = AmArg(request);
  }
}

void clearRequestParameters(AVarMapT& avar) {
  avar.erase(DSM_AVAR_REQUEST);
}

void extractReplyParameters(VarMapT& event_params, AVarMapT& avar, DSMSipReply* reply) {
  if (NULL == reply)
    return;

  event_params["sip_reason"] = reply->reply->reason;
  event_params["sip_code"] = int2str(reply->reply->code);
  event_params["from"] = reply->reply->from;
  event_params["from_tag"] = reply->reply->from_tag;
  event_params["to"] = reply->reply->to;
  event_params["to_tag"] = reply->reply->to_tag;
  event_params["callid"] = reply->reply->callid;
  event_params["hdrs"] = reply->reply->hdrs;
#ifdef PROPAGATE_UNPARSED_REPLY_HEADERS
  for (list<AmSipHeader>::const_iterator it = reply->reply->unparsed_headers.begin();
       it != reply->reply->unparsed_headers.end(); it++) {
    event_params["hdr."+it->name] = it->value;
  }
#else
  vector<string> hdrs = explode(reply->reply->hdrs, CRLF);
  for (vector<string>::iterator it=hdrs.begin(); it!=hdrs.end();it++) {
    size_t p = it->find(":");
    if (p==string::npos)
      continue;
    size_t p1=p;
    if (++p>=it->size())
      continue;
    while (p<it->size() && ((*it)[p] == ' ' || (*it)[p] == '\t'))
      p++;
    event_params["hdr."+it->substr(0,p1)]=it->substr(p);
  }
#endif
  avar[DSM_AVAR_REPLY] = AmArg(reply);
}

void clearReplyParameters(AVarMapT& avar) {
  avar.erase(DSM_AVAR_REPLY);
}

void SBCDSMInstance::onStateChange(SBCCallLeg *call, const CallLeg::StatusChangeCause &cause) {
  DBG("SBCDSMInstance::onStateChange()\n");
  VarMapT event_params;

  event_params["SBCCallStatus"] = call->getCallStatusStr();
  auto_ptr<DSMSipRequest> dsm_request;
  auto_ptr<DSMSipReply> dsm_reply;

  switch (cause.reason) {
  case CallLeg::StatusChangeCause::SipReply:
    event_params["reason"] = "SipReply";
    dsm_reply.reset(new DSMSipReply(cause.param.reply));
    extractReplyParameters(event_params, avar, dsm_reply.get());
    break;
  case CallLeg::StatusChangeCause::SipRequest:
    event_params["reason"] = "SipRequest";
    dsm_request.reset(new DSMSipRequest(cause.param.request));
    extractRequestParameters(event_params, avar, dsm_request.get());
    break;
  case CallLeg::StatusChangeCause::Other:
    event_params["reason"] = "other";
    if (NULL != cause.param.desc) 
      event_params["desc"] = string(cause.param.desc);
    break;
  case CallLeg::StatusChangeCause::Canceled: event_params["reason"] = "Canceled"; break;
  case CallLeg::StatusChangeCause::NoAck: event_params["reason"] = "NoAck"; break;
  case CallLeg::StatusChangeCause::NoPrack: event_params["reason"] = "NoPrack"; break;
  case CallLeg::StatusChangeCause::RtpTimeout: event_params["reason"] = "RtpTimeout"; break;
  case CallLeg::StatusChangeCause::SessionTimeout: event_params["reason"] = "SessionTimeout"; break;
  case CallLeg::StatusChangeCause::InternalError: event_params["reason"] = "InternalError"; break;
  defaut: break;
  };

  engine.runEvent(call, this, DSMCondition::LegStateChange, &event_params);

  switch (cause.reason) {
  case CallLeg::StatusChangeCause::SipReply: clearReplyParameters(avar); break;
  case CallLeg::StatusChangeCause::SipRequest: clearRequestParameters(avar); break;
  default: break;
  }

}

/** called from A/B leg when in-dialog request comes in */
CCChainProcessing SBCDSMInstance::onInDialogRequest(SBCCallLeg* call, const AmSipRequest& req) {
  DBG("SBCDSMInstance::onInDialogRequest()\n");
  VarMapT event_params;
  DSMSipRequest dsm_request(&req);

  extractRequestParameters(event_params, avar, &dsm_request);

  engine.runEvent(call, this, DSMCondition::SipRequest, &event_params);

  clearRequestParameters(avar);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

CCChainProcessing SBCDSMInstance::onInDialogReply(SBCCallLeg* call, const AmSipReply& reply) {
  DBG("SBCDSMInstance::onInDialogReply()\n");
  VarMapT event_params;
  DSMSipReply dsm_reply(&reply);
  extractReplyParameters(event_params, avar, &dsm_reply);

  engine.runEvent(call, this, DSMCondition::SipReply, &event_params);

  clearReplyParameters(avar);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

CCChainProcessing SBCDSMInstance::onEvent(SBCCallLeg* call, AmEvent* event) {
  DBG("SBCDSMInstance::onEvent()\n");

  if (event->event_id == DSM_EVENT_ID) {
    DSMEvent* dsm_event = dynamic_cast<DSMEvent*>(event);
    if (dsm_event) {
      DBG("SBCDSMInstance processing DSM event\n");

      engine.runEvent(call, this, DSMCondition::DSMEvent, &dsm_event->params);

      if (dsm_event->params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
	return StopProcessing;
      return ContinueProcessing;
    }
  }

  B2BEvent* b2b_ev = dynamic_cast<B2BEvent*>(event);
  if(b2b_ev && b2b_ev->ev_type == B2BEvent::B2BApplication) {
    engine.runEvent(call, this, DSMCondition::B2BEvent, &b2b_ev->params);

    if (b2b_ev->params[DSM_SBC_PARAM_PROCESSED] == DSM_TRUE) {
      ReliableB2BEvent* rel_b2b_ev = dynamic_cast<ReliableB2BEvent*>(b2b_ev);
      if (NULL != rel_b2b_ev) {
	rel_b2b_ev->markAsProcessed();
      } else {
	DBG("possible script writer error: marked #processed on non-reliable B2BEvent");
      }
    }

    if (b2b_ev->params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
      return StopProcessing;
    return ContinueProcessing;
  }

  if(b2b_ev && b2b_ev->ev_type == B2BEvent::B2BCore) {
    B2BSipRequestEvent* b2b_req_ev = dynamic_cast<B2BSipRequestEvent*>(b2b_ev);
    if (b2b_req_ev) {
      VarMapT event_params;
      DSMSipRequest sip_req(&b2b_req_ev->req);
      extractRequestParameters(event_params, avar, &sip_req);
      event_params["forward"] = b2b_req_ev->forward?"true":"false";
      engine.runEvent(call, this, DSMCondition::B2BOtherRequest, &event_params);
      avar.erase(DSM_AVAR_REQUEST);
      if (event_params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
	return StopProcessing;
    } else {
      B2BSipReplyEvent* b2b_reply_ev = dynamic_cast<B2BSipReplyEvent*>(b2b_ev);
      if (b2b_reply_ev) {
	VarMapT event_params;
	DSMSipReply dsm_reply(&b2b_reply_ev->reply);
	extractReplyParameters(event_params, avar, &dsm_reply);
	event_params["forward"] = b2b_reply_ev->forward?"true":"false";
	event_params["trans_method"] = b2b_reply_ev->trans_method;
	engine.runEvent(call, this, DSMCondition::B2BOtherReply, &event_params);
	if (event_params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
	  return StopProcessing;
      }
    }

  }

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    map<string, string> params;
    params["id"] = int2str(timer_id);
    engine.runEvent(call, this, DSMCondition::Timer, &params);

    if (params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
      return StopProcessing;
    return ContinueProcessing;
  }

  AmPlaylistSeparatorEvent* sep_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(event);
  if (sep_ev) {
    map<string, string> params;
    params["id"] = int2str(sep_ev->event_id);
    engine.runEvent(call, this, DSMCondition::PlaylistSeparator, &params);

    if (params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
      return StopProcessing;
    return ContinueProcessing;
  }

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event &&
     ((audio_event->event_id == AmAudioEvent::cleared) ||
      (audio_event->event_id == AmAudioEvent::noAudio))){
    map<string, string> params;
    params["type"] = audio_event->event_id == AmAudioEvent::cleared?"cleared":"noAudio";
    engine.runEvent(call, this, DSMCondition::NoAudio, &params);

    if (params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
      return StopProcessing;
    return ContinueProcessing;
  }

  // todo: process JsonRPCEvents (? see DSMCall::process)

  return ContinueProcessing;
}

CCChainProcessing SBCDSMInstance::onDtmf(SBCCallLeg *call, int event, int duration) {
  DBG("* Got DTMF key %d duration %d\n",
      event, duration);

  map<string, string> params;
  params["key"] = int2str(event);
  params["duration"] = int2str(duration);

  engine.runEvent(call, this, DSMCondition::Key, &params);

  if (params[DSM_SBC_PARAM_STOP_PROCESSING]==DSM_TRUE)
    return StopProcessing;
  return ContinueProcessing;
}

/** @return whether to continue processing */
CCChainProcessing SBCDSMInstance::onBLegRefused(SBCCallLeg* call, const AmSipReply& reply)
{
  DBG("SBCDSMInstance::onBLegRefused()\n");
  VarMapT event_params;
  DSMSipReply dsm_reply(&reply);
  extractReplyParameters(event_params, avar, &dsm_reply);

  engine.runEvent(call, this, DSMCondition::BLegRefused, &event_params);

  clearRequestParameters(avar);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

// --- hold related ------------------------

CCChainProcessing SBCDSMInstance::putOnHold(SBCCallLeg* call) {
  DBG("SBCDSMInstance::putOnHold()\n");
  VarMapT event_params;
  engine.runEvent(call, this, DSMCondition::PutOnHold, &event_params);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

CCChainProcessing SBCDSMInstance::resumeHeld(SBCCallLeg* call, bool send_reinvite) {
  DBG("SBCDSMInstance::resumeHeld()\n");
  VarMapT event_params;
  event_params["send_reinvite"] = send_reinvite?"true":"false";
  engine.runEvent(call, this, DSMCondition::ResumeHeld, &event_params);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

CCChainProcessing SBCDSMInstance::createHoldRequest(SBCCallLeg* call, AmSdp& sdp) {
  DBG("SBCDSMInstance::createHoldRequest()\n");
  VarMapT event_params;
  // TODO: encapsulate SDP so actions can manipulate Hold request (?)
  engine.runEvent(call, this, DSMCondition::CreateHoldRequest, &event_params);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

CCChainProcessing SBCDSMInstance::handleHoldReply(SBCCallLeg* call, bool succeeded) {
  DBG("SBCDSMInstance::handleHoldReply()\n");
  VarMapT event_params;
  event_params["succeeded"] = succeeded?"true":"false";
  engine.runEvent(call, this, DSMCondition::HandleHoldReply, &event_params);
  RETURN_CONTINUE_OR_STOP_PROCESSING;
}

// pretty much nonsense, but necessary because DSM is passing around AmSession
// everywhere; so we need this for non-call relays
void SBCDSMInstance::resetDummySession(SimpleRelayDialog *relay) {
  if (NULL == dummy_session.get())  {
    dummy_session.reset(new AmSession());
    // copy the most important things
    // TODO: initialize stuff from relay dialog in dummy session to be visible in DSM 
    dummy_session->dlg->setCallid(relay->getCallid());
    dummy_session->dlg->setLocalTag(relay->getLocalTag());
    dummy_session->dlg->setRemoteTag(relay->getRemoteTag());
    dummy_session->dlg->setLocalUri(relay->getLocalUri());
    dummy_session->dlg->setRemoteUri(relay->getRemoteUri());
  }
}

AmPlaylist* SBCDSMInstance::getPlaylist() {
  if (NULL == playlist.get())
    playlist.reset(new AmPlaylist(call));

  return playlist.get();
}

// ------------ simple relay interface --------------------------------------- */
bool SBCDSMInstance::init(SBCCallProfile &profile, SimpleRelayDialog *relay) {
  DBG("SBCDSMInstance::init() - simple relay\n");
  resetDummySession(relay);

  VarMapT event_params;
  event_params["relay_event"] = "init";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayInit, &event_params);
  avar.erase(DSM_SBC_AVAR_PROFILE);

  return true;
}

void SBCDSMInstance::initUAC(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest &req) {
  DBG("SBCDSMInstance::initUAC() - simple relay\n");
  resetDummySession(relay);

  VarMapT event_params;
  event_params["relay_event"] = "initUAC";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  DSMSipRequest sip_req(&req);
  extractRequestParameters(event_params, avar, &sip_req);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayInitUAC, &event_params);
  clearRequestParameters(avar);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

void SBCDSMInstance::initUAS(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest &req) {
  DBG("SBCDSMInstance::initUAS() - simple relay\n");
  resetDummySession(relay);
  VarMapT event_params;
  event_params["relay_event"] = "initUAS";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  DSMSipRequest sip_req(&req);
  extractRequestParameters(event_params, avar, &sip_req);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayInitUAS, &event_params);
  clearRequestParameters(avar);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

void SBCDSMInstance::finalize(SBCCallProfile &profile, SimpleRelayDialog *relay) {
  DBG("SBCDSMInstance::finalize() - relay\n");
  resetDummySession(relay);
  VarMapT event_params;
  event_params["relay_event"] = "finalize";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayFinalize, &event_params);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

void SBCDSMInstance::onSipRequest(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest& req) {
  DBG("SBCDSMInstance::onSipRequest() - simple relay\n");
  resetDummySession(relay);
  VarMapT event_params;
  event_params["relay_event"] = "onSipRequest";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  DSMSipRequest sip_req(&req);
  extractRequestParameters(event_params, avar, &sip_req);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayOnSipRequest, &event_params);
  clearRequestParameters(avar);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

void SBCDSMInstance::onSipReply(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest& req,
				const AmSipReply& reply,
				AmBasicSipDialog::Status old_dlg_status) {
  DBG("SBCDSMInstance::onSipReply() - simple relay\n");
  resetDummySession(relay);
  VarMapT event_params;
  event_params["relay_event"] = "onSipReply";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  DSMSipRequest sip_req(&req);
  extractRequestParameters(event_params, avar, &sip_req);
  DSMSipReply dsm_reply(&reply);
  extractReplyParameters(event_params, avar, &dsm_reply); // TODO: shadows request
  event_params["old_dlg_status"] = AmBasicSipDialog::getStatusStr(old_dlg_status);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayOnSipReply, &event_params);
  clearReplyParameters(avar);
  clearRequestParameters(avar);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

void SBCDSMInstance::onB2BRequest(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest& req) {
  DBG("SBCDSMInstance::onB2BRequest() - relay\n");
  resetDummySession(relay);
  VarMapT event_params;
  event_params["relay_event"] = "onB2BRequest";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  DSMSipRequest sip_req(&req);
  extractRequestParameters(event_params, avar, &sip_req);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayOnB2BRequest, &event_params);
  clearRequestParameters(avar);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

void SBCDSMInstance::onB2BReply(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipReply& reply) {
  DBG("SBCDSMInstance::onB2BReply() - relay\n");
  resetDummySession(relay);
  VarMapT event_params;
  event_params["relay_event"] = "onB2BReply";
  avar[DSM_SBC_AVAR_PROFILE] = AmArg(&profile);
  DSMSipReply dsm_reply(&reply);
  extractReplyParameters(event_params, avar, &dsm_reply);
  engine.runEvent(dummy_session.get(), this, DSMCondition::RelayOnB2BReply, &event_params);
  clearReplyParameters(avar);
  avar.erase(DSM_SBC_AVAR_PROFILE);
}

// --- garbage collector related ------------------------

void SBCDSMInstance::transferOwnership(DSMDisposable* d) {
  gc_trash.insert(d);
}

void SBCDSMInstance::releaseOwnership(DSMDisposable* d) {
  gc_trash.erase(d);
}

// --- DSM session API  -------------------------------------------

#define NOT_IMPLEMENTED_UINT(_func)					\
  unsigned int SBCDSMInstance::_func {					\
    throw DSMException("core", "cause", "not implemented in DSM SBC"); \
  }

#define NOT_IMPLEMENTED(_func)						\
  void SBCDSMInstance::_func {						\
    throw DSMException("core", "cause", "not implemented in DSM SBC"); \
  }

NOT_IMPLEMENTED(playPrompt(const string& name, bool loop, bool front));
NOT_IMPLEMENTED(setPromptSet(const string& name));

void SBCDSMInstance::playFile(const string& name, bool loop, bool front) {
  AmAudioFile* af = new AmAudioFile();
  if(af->open(name,AmAudioFile::Read)) {
    ERROR("audio file '%s' could not be opened for reading.\n",
	  name.c_str());
    delete af;

    throw DSMException("file", "path", name);

    return;
  }
  if (loop)
    af->loop.set(true);

  if (front)
    getPlaylist()->addToPlayListFront(new AmPlaylistItem(af, NULL));
  else
    getPlaylist()->addToPlaylist(new AmPlaylistItem(af, NULL));

  audiofiles.push_back(af);
  CLR_ERRNO;
}

void SBCDSMInstance::playSilence(unsigned int length, bool front) {
  AmNullAudio* af = new AmNullAudio();
  af->setReadLength(length);
  if (front)
    getPlaylist()->addToPlayListFront(new AmPlaylistItem(af, NULL));
  else
    getPlaylist()->addToPlaylist(new AmPlaylistItem(af, NULL));

  audiofiles.push_back(af);
  CLR_ERRNO;
}

NOT_IMPLEMENTED(recordFile(const string& name));
NOT_IMPLEMENTED_UINT(getRecordLength());
NOT_IMPLEMENTED_UINT(getRecordDataSize());
NOT_IMPLEMENTED(stopRecord());

NOT_IMPLEMENTED(setInOutPlaylist());

// void SBCDSMInstance::setInOutPlaylist() {
//   AmB2BMedia *media = call->getMediaSession();
//   if (NULL == media) {
//     ERROR("could not set InOutPlaylist - no media session!\n");
//     return;
//   }
//   media->setFirstStreamInOut(call->isALeg(), getPlaylist(), getPlaylist());
// }

void SBCDSMInstance::setInputPlaylist() {
  AmB2BMedia *media = call->getMediaSession();
  if (NULL == media) {
    ERROR("could not setInputPlaylist - no media session!\n");
    return;
  }

  media->setFirstStreamInput(call->isALeg(), getPlaylist());
}

NOT_IMPLEMENTED(setOutputPlaylist());
// void SBCDSMInstance::setOutputPlaylist() {
//   AmB2BMedia *media = call->getMediaSession();
//   if (NULL == media) {
//     ERROR("could not setOutputPlaylist - no media session!\n");
//     return;
//   }  media->setFirstStreamOutput(call->isALeg(), getPlaylist());
// }

void SBCDSMInstance::addToPlaylist(AmPlaylistItem* item, bool front) {
  DBG("add item to playlist\n");
  if (front)
    getPlaylist()->addToPlayListFront(item);
  else
    getPlaylist()->addToPlaylist(item);
}


void SBCDSMInstance::flushPlaylist() {
  DBG("flush playlist\n");
  getPlaylist()->flush(); 
}

void SBCDSMInstance::addSeparator(const string& name, bool front) {
  unsigned int id = 0;
  if (str2i(name, id)) {
    SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    SET_STRERROR("separator id '"+name+"' not a number");
    return;
  }

  AmPlaylistSeparator* sep = new AmPlaylistSeparator(call, id);
  if (front)
    getPlaylist()->addToPlayListFront(new AmPlaylistItem(sep, sep));
  else
    getPlaylist()->addToPlaylist(new AmPlaylistItem(sep, sep));
  // for garbage collector
  audiofiles.push_back(sep);
  CLR_ERRNO;
}

void SBCDSMInstance::connectMedia() {
  AmB2BMedia *media = call->getMediaSession();
  if (NULL == media) {
    DBG("media session was not set, creating new one\n");
    media = new AmB2BMedia(call->isALeg() ? call : NULL , call->isALeg() ? NULL : call);
    call->setMediaSession(media);
    // TODO: media stream initialization here (does changeRtpMode help?)
  } else {
    media->pauseRelay();
  }

  media->addToMediaProcessor();

  local_media_connected = true;
}

void SBCDSMInstance::disconnectMedia() {
  if (!local_media_connected) {
    DBG("local media not connected, not disconnecting\n");
    return;
  }
  DBG("disconnecting from local media processing, enabling Relay...\n");
  local_media_connected = false;

  AmB2BMedia *media = call->getMediaSession();
  if (NULL == media) {
    DBG("media session not set, not disconnecting\n");
    return;
  }
  AmMediaProcessor::instance()->removeSession(media);
  media->restartRelay();
}

NOT_IMPLEMENTED(mute());
NOT_IMPLEMENTED(unmute());

/** B2BUA functions */
NOT_IMPLEMENTED(B2BconnectCallee(const string& remote_party,
				 const string& remote_uri,
				 bool relayed_invite));
NOT_IMPLEMENTED(B2BterminateOtherLeg());
NOT_IMPLEMENTED(B2BaddReceivedRequest(const AmSipRequest& req));
NOT_IMPLEMENTED(B2BsetRelayEarlyMediaSDP(bool enabled));
NOT_IMPLEMENTED(B2BsetHeaders(const string& hdr, bool replaceCRLF));
NOT_IMPLEMENTED(B2BclearHeaders());
NOT_IMPLEMENTED(B2BaddHeader(const string& hdr));
NOT_IMPLEMENTED(B2BremoveHeader(const string& hdr));

#undef NOT_IMPLEMENTED
#undef NOT_IMPLEMENTED_UINT
