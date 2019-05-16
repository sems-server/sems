/*
 * Copyright (C) 2008 iptego GmbH
 * Copyright (C) 2012 Stefan Sayer
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

#include "DSMCall.h"
#include "AmUtils.h"
#include "AmMediaProcessor.h"
#include "DSM.h"
#include "AmConferenceStatus.h"
#include "AmAdvancedAudio.h"
#include "AmRingTone.h"
#include "AmSipSubscription.h"

#include "../apps/jsonrpc/JsonRPCEvents.h" // todo!

DSMCall::DSMCall(const DSMScriptConfig& config,
		 AmPromptCollection* prompts,
		 DSMStateDiagramCollection& diags,
		 const string& startDiagName,
		 UACAuthCred* credentials)
  : 
  cred(credentials),
  prompts(prompts), default_prompts(prompts), startDiagName(startDiagName),
  playlist(this), run_invite_event(config.RunInviteEvent),
  process_invite(true),
  process_sessionstart(true), rec_file(NULL)
{
  diags.addToEngine(&engine);
  set_sip_relay_only(false);
}

DSMCall::~DSMCall()
{
  for (std::set<DSMDisposable*>::iterator it=
	 gc_trash.begin(); it != gc_trash.end(); it++)
    delete *it;

  for (vector<AmAudio*>::iterator it=
	 audiofiles.begin();it!=audiofiles.end();it++) 
    delete *it;

  used_prompt_sets.insert(prompts);
  for (set<AmPromptCollection*>::iterator it=
	 used_prompt_sets.begin(); it != used_prompt_sets.end(); it++)
    (*it)->cleanup((long)this);
}

/** returns whether var exists && var==value*/
bool DSMCall::checkVar(const string& var_name, const string& var_val) {
  map<string, string>::iterator it = var.find(var_name);
  return (it != var.end()) && (it->second == var_val);
}

string DSMCall::getVar(const string& var_name) {
  map<string, string>::iterator it = var.find(var_name);
  if (it != var.end())
    return it->second;
  return "";
}

/** returns whether params, param exists && param==value*/
bool checkParam(const string& par_name, const string& par_val, map<string, string>* params) {
  if (NULL == params)
    return false;

  map<string, string>::iterator it = params->find(par_name);
  return (it != params->end()) && (it->second == par_val);
}

void DSMCall::onStart()
{
  engine.init(this, this, startDiagName, DSMCondition::Start);
}

void DSMCall::onInvite(const AmSipRequest& req) {
  // make B2B dialogs work in onInvite as well
  invite_req = req;

  if (!process_invite) {
    // re-INVITEs
    AmB2BCallerSession::onInvite(req);
    return;
  }
  process_invite = false;
    
  bool run_session_invite = engine.onInvite(req, this);

  avar[DSM_AVAR_REQUEST] = AmArg((AmObject*)&req);

  DBG("before runEvent(this, this, DSMCondition::Invite);\n");
  AmSipDialog::Status old_st = dlg->getStatus();
  engine.runEvent(this, this, DSMCondition::Invite, NULL);
  avar.erase(DSM_AVAR_REQUEST);

  if ( old_st != dlg->getStatus()
       //checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE)
      ) {
    DBG("session choose to not connect media\n");
    run_session_invite = false;     // don't accept audio 
  }    

  if (run_session_invite) 
    AmB2BCallerSession::onInvite(req);
}

void DSMCall::onOutgoingInvite(const string& headers) {
  if (!process_invite) {
    // re-INVITE sent out
    return;
  }
  process_invite = false;

  // TODO: construct correct request of outgoing INVITE
  AmSipRequest req;
  req.hdrs = headers;

  bool run_session_invite = engine.onInvite(req, this);
  if (checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE)) {
    DBG("session choose to not connect media\n");
    // TODO: set flag to not connect RTP on session start
    run_session_invite = false;     // don't accept audio 
  }    
  
  if (checkVar(DSM_ACCEPT_EARLY_SESSION, DSM_ACCEPT_EARLY_SESSION_FALSE)) {
    DBG("session choose to not accept early session\n");
    accept_early_session = false;
  } else {
    DBG("session choose to accept early session\n");
    accept_early_session = true;
  }
}

void DSMCall::onRinging(const AmSipReply& reply) {
  map<string, string> params;
  params["code"] = int2str(reply.code);
  params["reason"] = reply.reason;
  params["has_body"] = reply.body.empty() ?
    "false" : "true";
  engine.runEvent(this, this, DSMCondition::Ringing, &params);
  // todo: local ringbacktone
}

void DSMCall::onEarlySessionStart() {
  engine.runEvent(this, this, DSMCondition::EarlySession, NULL);

  if (checkVar(DSM_CONNECT_EARLY_SESSION, DSM_CONNECT_EARLY_SESSION_FALSE)) {
    DBG("call does not connect early session\n");
  } else {
    if (!getInput())
      setInput(&playlist);

    if (!getOutput())
      setOutput(&playlist);

    AmB2BCallerSession::onEarlySessionStart();
  }
}

void DSMCall::onSessionStart()
{
  if (process_sessionstart) {
    process_sessionstart = false;

    DBG("DSMCall::onSessionStart\n");
    startSession();
  }

  AmB2BCallerSession::onSessionStart();
}

int DSMCall::onSdpCompleted(const AmSdp& offer, const AmSdp& answer)
{
  AmMimeBody* sdp_body = invite_req.body.hasContentType(SIP_APPLICATION_SDP);
  if(!sdp_body) {
    sdp_body = invite_req.body.addPart(SIP_APPLICATION_SDP);
  }

  if(sdp_body) {
    string sdp_buf;
    answer.print(sdp_buf);
    sdp_body->setPayload((const unsigned char*)sdp_buf.c_str(),
			 sdp_buf.length());
  }

  return AmB2BCallerSession::onSdpCompleted(offer,answer);
}

bool DSMCall::getSdpOffer(AmSdp& offer)
{
  if (!AmB2BCallerSession::getSdpOffer(offer)) {
    return false;
  }

  engine.processSdpOffer(offer);
  return true;
}

bool DSMCall::getSdpAnswer(const AmSdp& offer, AmSdp& answer)
{
  if (!AmB2BCallerSession::getSdpAnswer(offer, answer)) {
    return false;
  }

  engine.processSdpAnswer(offer, answer);
  return true;
}

void DSMCall::startSession(){

  engine.runEvent(this, this, DSMCondition::SessionStart, NULL);
  setReceiving(true);

  if (!checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE)) {
    if (!getInput())
      setInput(&playlist);

    setOutput(&playlist);
  }
}

void DSMCall::connectMedia() {
  if (!getInput())
    setInput(&playlist);

  setOutput(&playlist);
  AmMediaProcessor::instance()->addSession(this, callgroup);
}

void DSMCall::disconnectMedia() {
  AmMediaProcessor::instance()->removeSession(this);
}

void DSMCall::mute() {
  setMute(true);
}

void DSMCall::unmute() {
  setMute(false);
}


void DSMCall::onDtmf(int event, int duration_msec) {
  DBG("* Got DTMF key %d duration %d\n", 
      event, duration_msec);

  map<string, string> params;
  params["key"] = int2str(event);
  params["duration"] = int2str(duration_msec);
  engine.runEvent(this, this, DSMCondition::Key, &params);
}

void DSMCall::onBye(const AmSipRequest& req)
{
  DBG("onBye\n");
  map<string, string> params;
  params["headers"] = req.hdrs;
 
  engine.runEvent(this, this, DSMCondition::Hangup, &params);

  clearRtpReceiverRelay();
}

void DSMCall::onCancel(const AmSipRequest& cancel) {
  DBG("onCancel\n");
  if (dlg->getStatus() < AmSipDialog::Connected) {
    //TODO: pass the cancel request as a parameter?
    DBG("hangup event!!!\n");
    map<string, string> params;
    params["headers"] = cancel.hdrs;
    engine.runEvent(this, this, DSMCondition::Hangup, &params);
  }
  else {
    DBG("ignoring onCancel event in established dialog\n");
  }
}

void DSMCall::onSipRequest(const AmSipRequest& req) {

  if (checkVar(DSM_ENABLE_REQUEST_EVENTS, DSM_TRUE)) {
    map<string, string> params;
    params["method"] = req.method;
    params["r_uri"] = req.r_uri;
    params["from"] = req.from;
    params["to"] = req.to;
    params["hdrs"] = req.hdrs;
    params["cseq"] = int2str(req.cseq);

    // pass AmSipRequest for use by mod_dlg
    DSMSipRequest* sip_req = new DSMSipRequest(&req);
    avar[DSM_AVAR_REQUEST] = AmArg(sip_req);
    
    engine.runEvent(this, this, DSMCondition::SipRequest, &params);

    delete sip_req;
    avar.erase(DSM_AVAR_REQUEST);

    if (checkParam(DSM_PROCESSED, DSM_TRUE, &params)) {
      DBG("DSM script processed SIP request '%s', returning\n", 
	  req.method.c_str());
      return;
    }
  }

  AmB2BCallerSession::onSipRequest(req);  
}

void DSMCall::onSipReply(const AmSipRequest& req,
			 const AmSipReply& reply, 
			 AmBasicSipDialog::Status old_dlg_status) 
{

  if (checkVar(DSM_ENABLE_REPLY_EVENTS, DSM_TRUE)) {
    map<string, string> params;
    params["code"] = int2str(reply.code);
    params["reason"] = reply.reason;
    params["hdrs"] = reply.hdrs;
    params["cseq"] = int2str(reply.cseq);

    params["dlg_status"] = dlg->getStatusStr();
    params["old_dlg_status"] = AmBasicSipDialog::getStatusStr(old_dlg_status);

    // pass AmSipReply for use by mod_dlg (? sending ACK?)
    DSMSipReply* dsm_reply = new DSMSipReply(&reply);
    avar[DSM_AVAR_REPLY] = AmArg(dsm_reply);
    
    engine.runEvent(this, this, DSMCondition::SipReply, &params);

    delete dsm_reply;
    avar.erase(DSM_AVAR_REPLY);

    if (checkParam(DSM_PROCESSED, DSM_TRUE, &params)) {
      DBG("DSM script processed SIP reply '%u %s', returning\n", 
	  reply.code, reply.reason.c_str());
      return;
    }
  }

  AmB2BCallerSession::onSipReply(req, reply, old_dlg_status);

  if ((old_dlg_status < AmSipDialog::Connected) && 
      (dlg->getStatus() == AmSipDialog::Disconnected)) {
    DBG("Outbound call failed with reply %d %s.\n", 
	reply.code, reply.reason.c_str());
    map<string, string> params;
    params["code"] = int2str(reply.code);
    params["reason"] = reply.reason;
    engine.runEvent(this, this, DSMCondition::FailedCall, &params);
    setStopped();
  }
}

void DSMCall::onRemoteDisappeared(const AmSipReply& reply) {
  map<string, string> params;
  params["code"] = int2str(reply.code);
  params["reason"] = reply.reason;
  params["hdrs"] = reply.hdrs;
  params["cseq"] = int2str(reply.cseq);

  params["dlg_status"] = dlg->getStatusStr();

  // pass AmSipReply for use by modules
  DSMSipReply* dsm_reply = new DSMSipReply(&reply);
  avar[DSM_AVAR_REPLY] = AmArg(dsm_reply);

  engine.runEvent(this, this, DSMCondition::RemoteDisappeared, &params);

  delete dsm_reply;
  avar.erase(DSM_AVAR_REPLY);

  if (checkParam(DSM_PROCESSED, DSM_TRUE, &params)) {
    DBG("DSM script processed SIP onRemoteDisappeared reply '%u %s', returning\n",
	reply.code, reply.reason.c_str());
    return;
  }

  AmB2BCallerSession::onRemoteDisappeared(reply);
}

void DSMCall::onSessionTimeout() {
  map<string, string> params;

  engine.runEvent(this, this, DSMCondition::SessionTimeout, &params);

  if (checkParam(DSM_PROCESSED, DSM_TRUE, &params)) {
    DBG("DSM script processed onSessionTimeout, returning\n");
    return;
  }

  AmB2BCallerSession::onSessionTimeout();
}

void DSMCall::onRtpTimeout() {
  map<string, string> params;

  engine.runEvent(this, this, DSMCondition::RtpTimeout, &params);

  if (checkParam(DSM_PROCESSED, DSM_TRUE, &params)) {
    DBG("DSM script processed onRtpTimeout, returning\n");
    return;
  }

  AmB2BCallerSession::onRtpTimeout();
}

void DSMCall::onNoAck(unsigned int cseq)
{
  DBG("onNoAck\n");
  map<string, string> params;
  params["headers"] = "";
  params["reason"] = "onNoAck";

  engine.runEvent(this, this, DSMCondition::Hangup, &params);
  AmB2BCallerSession::onNoAck(cseq);
}

void DSMCall::onSystemEvent(AmSystemEvent* ev) {
  map<string, string> params;
  params["type"] = AmSystemEvent::getDescription(ev->sys_event);
  engine.runEvent(this, this, DSMCondition::System, &params);
  if (params["processed"] != DSM_TRUE) {
    AmB2BCallerSession::onSystemEvent(ev);
  }
}

void DSMCall::onBeforeDestroy() {
  map<string, string> params;
  engine.runEvent(this, this, DSMCondition::BeforeDestroy, &params);

  engine.onBeforeDestroy(this, this);
}

#ifdef WITH_ZRTP
void DSMCall::onZRTPProtocolEvent(zrtp_protocol_event_t event, zrtp_stream_t *stream_ctx) {
  DBG("DSMCall::onZRTPProtocolEvent: %s\n", zrtp_protocol_event_desc(event));

  if (checkVar(DSM_ENABLE_ZRTP_EVENTS, DSM_TRUE)) {
    map<string, string> params;
    params["event"] = zrtp_protocol_event_desc(event);
    params["event_id"] = int2str(event);
    engine.runEvent(this, this, DSMCondition::ZRTPProtocolEvent, &params);
  }

}

void DSMCall::onZRTPSecurityEvent(zrtp_security_event_t event, zrtp_stream_t *stream_ctx) {
  DBG("DSMCall::onZRTPSecurityEvent: %s\n", zrtp_security_event_desc(event));

  if (checkVar(DSM_ENABLE_ZRTP_EVENTS, DSM_TRUE)) {
    map<string, string> params;
    params["event"] = zrtp_security_event_desc(event);
    params["event_id"] = int2str(event);
    engine.runEvent(this, this, DSMCondition::ZRTPSecurityEvent, &params);
  }

}
#endif

void DSMCall::process(AmEvent* event)
{

  DBG("DSMCall::process\n");

  if (event->event_id == DSM_EVENT_ID) {
    DSMEvent* dsm_event = dynamic_cast<DSMEvent*>(event);
    if (dsm_event) {      
      engine.runEvent(this, this, DSMCondition::DSMEvent, &dsm_event->params);
      return;
    }  
  }

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && 
     ((audio_event->event_id == AmAudioEvent::cleared) || 
      (audio_event->event_id == AmAudioEvent::noAudio))){
    map<string, string> params;
    params["type"] = audio_event->event_id == AmAudioEvent::cleared?"cleared":"noAudio";
    engine.runEvent(this, this, DSMCondition::NoAudio, &params);
    return;
  }

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    map<string, string> params;
    params["id"] = int2str(timer_id);
    engine.runEvent(this, this, DSMCondition::Timer, &params);
  }

  AmPlaylistSeparatorEvent* sep_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(event);
  if (sep_ev) {
    map<string, string> params;
    params["id"] = int2str(sep_ev->event_id);
    engine.runEvent(this, this, DSMCondition::PlaylistSeparator, &params);
  }

  ConferenceEvent * conf_ev = dynamic_cast<ConferenceEvent*>(event);
  if (conf_ev) {
    map<string, string> params;
    params["type"] = "conference_event";
    params["id"] = int2str(conf_ev->event_id);
    engine.runEvent(this, this, DSMCondition::DSMEvent, &params);
  }

  // todo: give modules the possibility to define/process events
  JsonRpcEvent* jsonrpc_ev = dynamic_cast<JsonRpcEvent*>(event);
  if (jsonrpc_ev) { 
    DBG("received jsonrpc event\n");

    JsonRpcResponseEvent* resp_ev = 
      dynamic_cast<JsonRpcResponseEvent*>(jsonrpc_ev);
    if (resp_ev) {
      map<string, string> params;
      params["ev_type"] = "JsonRpcResponse";
      params["id"] = resp_ev->response.id;
      params["is_error"] = resp_ev->response.is_error ? 
	"true":"false";

      // decode result for easy use from script
      varPrintArg(resp_ev->response.data, params, resp_ev->response.is_error ? "error": "result");

      // decode udata for easy use from script
      varPrintArg(resp_ev->udata, params, "udata");

      // save reference to full parameters as avar
      avar[DSM_AVAR_JSONRPCRESPONSEDATA] = AmArg(&resp_ev->response.data);
      avar[DSM_AVAR_JSONRPCRESPONSEUDATA] = AmArg(&resp_ev->udata);

      engine.runEvent(this, this, DSMCondition::JsonRpcResponse, &params);

      avar.erase(DSM_AVAR_JSONRPCRESPONSEUDATA);
      avar.erase(DSM_AVAR_JSONRPCRESPONSEDATA);
      return;
    }

    JsonRpcRequestEvent* req_ev = 
      dynamic_cast<JsonRpcRequestEvent*>(jsonrpc_ev);
    if (req_ev) {
      map<string, string> params;
      params["ev_type"] = "JsonRpcRequest";
      params["is_notify"] = req_ev->isNotification() ? 
	"true" : "false";
      params["method"] = req_ev->method;
      if (!req_ev->id.empty())
	params["id"] = req_ev->id;

      // decode request params result for easy use from script
      varPrintArg(req_ev->params, params, "params");

      // save reference to full parameters
      avar[DSM_AVAR_JSONRPCREQUESTDATA] = AmArg(&req_ev->params);

      engine.runEvent(this, this, DSMCondition::JsonRpcRequest, &params);

      avar.erase(DSM_AVAR_JSONRPCREQUESTDATA);
      return;
    }

  }

  if (event->event_id == E_SIP_SUBSCRIPTION) {
    SIPSubscriptionEvent* sub_ev = dynamic_cast<SIPSubscriptionEvent*>(event);
    if (sub_ev) {
      DBG("DSM Call received SIP Subscription Event\n");
      map<string, string> params;
      params["status"] = sub_ev->getStatusText();
      params["code"] = int2str(sub_ev->code);
      params["reason"] = sub_ev->reason;
      params["expires"] = int2str(sub_ev->expires);
      params["has_body"] = sub_ev->notify_body.get()?"true":"false";
      if (sub_ev->notify_body.get()) {
  	avar[DSM_AVAR_SIPSUBSCRIPTION_BODY] = AmArg(sub_ev->notify_body.get());
      }
      engine.runEvent(this, this, DSMCondition::SIPSubscription, &params);
      avar.erase(DSM_AVAR_SIPSUBSCRIPTION_BODY);
    }
  }

  AmRtpTimeoutEvent* timeout_ev = dynamic_cast<AmRtpTimeoutEvent*>(event);
  if (timeout_ev) {
    map<string, string> params;
    params["type"] = "rtp_timeout";
    params["timeout_value"] = int2str(AmConfig::DeadRtpTime);
    engine.runEvent(this, this, DSMCondition::RTPTimeout, &params);
    return;
  }

  if (event->event_id ==  E_B2B_APP) {
    B2BEvent* b2b_ev = dynamic_cast<B2BEvent*>(event);
    if(b2b_ev && b2b_ev->ev_type == B2BEvent::B2BApplication) {
      engine.runEvent(this, this, DSMCondition::B2BEvent, &b2b_ev->params);
      return;
    }
  }

  AmB2BCallerSession::process(event);
}

inline UACAuthCred* DSMCall::getCredentials() {
  return cred.get();
}

void DSMCall::playPrompt(const string& name, bool loop, bool front) {
  DBG("playing prompt '%s'\n", name.c_str());
  if (prompts->addToPlaylist(name,  (long)this, playlist, 
			    front, loop))  {
    if ((var["prompts.default_fallback"] != "yes") ||
      default_prompts->addToPlaylist(name,  (long)this, playlist, 
				    front, loop)) {
      DBG("checked [%p]\n", default_prompts);
      throw DSMException("prompt", "name", name);
    } else {
      used_prompt_sets.insert(default_prompts);
      CLR_ERRNO;
    }      
  } else {
    CLR_ERRNO;
  }
}

void DSMCall::flushPlaylist() {
  DBG("flush playlist\n");
  playlist.flush();  
}

void DSMCall::addToPlaylist(AmPlaylistItem* item, bool front) {
  DBG("add item to playlist\n");
  if (front)
    playlist.addToPlayListFront(item);
  else
    playlist.addToPlaylist(item);
}

void DSMCall::playFile(const string& name, bool loop, bool front) {
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
    playlist.addToPlayListFront(new AmPlaylistItem(af, NULL));
  else
    playlist.addToPlaylist(new AmPlaylistItem(af, NULL));

  audiofiles.push_back(af);
  CLR_ERRNO;
}

void DSMCall::playSilence(unsigned int length, bool front) {
  AmNullAudio* af = new AmNullAudio();
  af->setReadLength(length);
  if (front)
    playlist.addToPlayListFront(new AmPlaylistItem(af, NULL));
  else
    playlist.addToPlaylist(new AmPlaylistItem(af, NULL));

  audiofiles.push_back(af);
  CLR_ERRNO;
}

void DSMCall::playRingtone(int length, int on, int off, int f, int f2, bool front) {
  AmRingTone* af = new AmRingTone(length, on, off, f, f2);
  if (front)
    playlist.addToPlayListFront(new AmPlaylistItem(af, NULL));
  else
    playlist.addToPlaylist(new AmPlaylistItem(af, NULL));

  audiofiles.push_back(af);
  CLR_ERRNO;
}

void DSMCall::recordFile(const string& name) {
  if (rec_file) 
    stopRecord();

  DBG("start record to '%s'\n", name.c_str());
  rec_file = new AmAudioFile();
  if(rec_file->open(name,AmAudioFile::Write)) {
    ERROR("audio file '%s' could not be opened for recording.\n", 
	  name.c_str());
    delete rec_file;
    rec_file = NULL;
    throw DSMException("file", "path", name);
    return;
  }
  setInput(rec_file); 
  CLR_ERRNO;
}

unsigned int DSMCall::getRecordLength() {
  if (!rec_file) {
    SET_ERRNO(DSM_ERRNO_SCRIPT);
    SET_STRERROR("getRecordLength used while not recording.");
    return 0;
  }
  CLR_ERRNO;
  return rec_file->getLength();
}

unsigned int DSMCall::getRecordDataSize() {
  if (!rec_file) {
    SET_ERRNO(DSM_ERRNO_SCRIPT);
    SET_STRERROR("getRecordDataSize used while not recording.");
    return 0;
  }
  CLR_ERRNO;
  return rec_file->getDataSize();
}

void DSMCall::stopRecord() {
  if (rec_file) {
    setInput(&playlist);
    rec_file->close();
    delete rec_file;
    rec_file = NULL;
    CLR_ERRNO;
  } else {
    WARN("stopRecord: we are not recording\n");
    SET_ERRNO(DSM_ERRNO_SCRIPT);
    SET_STRERROR("stopRecord used while not recording.");
    return;
  }
}

void DSMCall::setInOutPlaylist() {
  DBG("setting playlist as input and output\n");
  setInOut(&playlist, &playlist);
}

void DSMCall::setInputPlaylist() {
  DBG("setting playlist as input\n");
  setInput(&playlist);
}

void DSMCall::setOutputPlaylist() {
  DBG("setting playlist as output\n");
  setOutput(&playlist);
}

void DSMCall::addPromptSet(const string& name, 
			     AmPromptCollection* prompt_set) {
  if (prompt_set) {
    DBG("adding prompt set '%s'\n", name.c_str());
    prompt_sets[name] = prompt_set;
    CLR_ERRNO;
  } else {
    ERROR("trying to add NULL prompt set\n");
    SET_ERRNO(DSM_ERRNO_INTERNAL);
    SET_STRERROR("trying to add NULL prompt set\n");
  }
}

void DSMCall::setPromptSets(map<string, AmPromptCollection*>& 
			      new_prompt_sets) {
  prompt_sets = new_prompt_sets;
}

void DSMCall::setPromptSet(const string& name) {
  map<string, AmPromptCollection*>::iterator it = 
    prompt_sets.find(name);

  if (it == prompt_sets.end()) {
    ERROR("prompt set %s unknown\n", name.c_str());
    throw DSMException("prompt", "name", name);
    return;
  }

  DBG("setting prompt set '%s'\n", name.c_str());
  used_prompt_sets.insert(prompts);
  prompts = it->second;
  CLR_ERRNO;
}


void DSMCall::addSeparator(const string& name, bool front) {
  unsigned int id = 0;
  if (str2i(name, id)) {
    SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    SET_STRERROR("separator id '"+name+"' not a number");
    return;
  }

  AmPlaylistSeparator* sep = new AmPlaylistSeparator(this, id);
  if (front)
    playlist.addToPlayListFront(new AmPlaylistItem(sep, sep));
  else
    playlist.addToPlaylist(new AmPlaylistItem(sep, sep));
  // for garbage collector
  audiofiles.push_back(sep);
  CLR_ERRNO;
}

void DSMCall::transferOwnership(DSMDisposable* d) {
  if (d == NULL)
    return;
  gc_trash.insert(d);
}

void DSMCall::releaseOwnership(DSMDisposable* d) {
  if (d == NULL)
    return;
  gc_trash.erase(d);
}

// AmB2BSession methods
bool DSMCall::onOtherBye(const AmSipRequest& req) {
  DBG("* Got BYE from other leg\n");

  DSMSipRequest sip_req(&req);
  avar[DSM_AVAR_REQUEST] = AmArg((AmObject*)&sip_req);

  map<string, string> params;
  params["hdrs"] = req.hdrs; // todo: optimization - make this configurable
  engine.runEvent(this, this, DSMCondition::B2BOtherBye, &params);

  avar.erase(DSM_AVAR_REQUEST);

  return checkParam(DSM_PROCESSED, DSM_TRUE, &params);
}

bool DSMCall::onOtherReply(const AmSipReply& reply) {
  DBG("* Got reply from other leg: %u %s\n", 
      reply.code, reply.reason.c_str());

  map<string, string> params;
  params["code"] = int2str(reply.code);
  params["reason"] = reply.reason;
  params["hdrs"] = reply.hdrs; // todo: optimization - make this configurable

  engine.runEvent(this, this, DSMCondition::B2BOtherReply, &params);

  return false;
}

void DSMCall::B2BterminateOtherLeg() {
  terminateOtherLeg();
}

void DSMCall::B2BconnectCallee(const string& remote_party,
				 const string& remote_uri,
				 bool relayed_invite) {
  connectCallee(remote_party, remote_uri, relayed_invite);
}

AmB2BCalleeSession* DSMCall::newCalleeSession() {
  DSMCallCalleeSession* s = new DSMCallCalleeSession(this);
  s->dlg->setLocalParty(getVar(DSM_B2B_LOCAL_PARTY));
  s->dlg->setLocalUri(getVar(DSM_B2B_LOCAL_URI));

  string user = getVar(DSM_B2B_AUTH_USER);
  string pwd = getVar(DSM_B2B_AUTH_PWD);
  if (!user.empty() && !pwd.empty()) {
    s->setCredentials("", user, pwd);

    // adding auth handler
    AmSessionEventHandlerFactory* uac_auth_f = 
      AmPlugIn::instance()->getFactory4Seh("uac_auth");
    if (NULL == uac_auth_f)  {
      INFO("uac_auth module not loaded. uac auth NOT enabled for B2B b leg in DSM.\n");
    } else {
      AmSessionEventHandler* h = uac_auth_f->getHandler(s);
      
      // we cannot use the generic AmSessionEventHandler hooks, 
      // because the hooks don't work in AmB2BSession
      s->setAuthHandler(h);
      DBG("uac auth enabled for DSM callee session.\n");
    }
  }

  s->dlg->setCallid(getVar(DSM_B2B_CALLID));

  return s;
}

void DSMCall::B2BaddReceivedRequest(const AmSipRequest& req) {
  DBG("inserting request '%s' with CSeq %d in list of received requests\n", 
      req.method.c_str(), req.cseq);
  recvd_req.insert(std::make_pair(req.cseq, req));
}

void DSMCall::B2BsetRelayEarlyMediaSDP(bool enabled) {
  set_sip_relay_early_media_sdp(enabled);
}

void DSMCall::B2BsetHeaders(const string& hdr, bool replaceCRLF) {
  if (!replaceCRLF)  {
    invite_req.hdrs = hdr;
  } else {
    string hdr_crlf = hdr;
    DBG("hdr_crlf is '%s'\n", hdr_crlf.c_str());

    while (true) {
      size_t p = hdr_crlf.find("\\r\\n");
      if (p==string::npos)
	break;
      hdr_crlf.replace(p, 4, "\r\n");
    }
    DBG("-> hdr_crlf is '%s'\n", hdr_crlf.c_str());
    invite_req.hdrs += hdr_crlf;
  }
  // add \r\n if not in header
  if (invite_req.hdrs.length()>2 && 
      invite_req.hdrs.substr(invite_req.hdrs.length()-2) != "\r\n")
    invite_req.hdrs+="\r\n";
}

void DSMCall::B2BaddHeader(const string& hdr) {
  invite_req.hdrs +=hdr;
  // add \r\n if not in header
  if (invite_req.hdrs.length()>2 && 
      invite_req.hdrs.substr(invite_req.hdrs.length()-2) != "\r\n")
    invite_req.hdrs+="\r\n";
}

void DSMCall::B2BclearHeaders() {
  invite_req.hdrs.clear();
}

void DSMCall::B2BremoveHeader(const string& hdr) {
  removeHeader(invite_req.hdrs, hdr);
}

/* --- B2B second leg -------------------------------------------------- */

DSMCallCalleeSession::DSMCallCalleeSession(const string& other_local_tag)
  : AmB2BCalleeSession(other_local_tag) {
}

DSMCallCalleeSession::DSMCallCalleeSession(const AmB2BCallerSession* caller)
  : AmB2BCalleeSession(caller) {
}

void DSMCallCalleeSession::setCredentials(const string& realm,
					   const string& user,
					  const string& pwd) {
  cred.reset(new UACAuthCred(realm, user, pwd));
}

UACAuthCred* DSMCallCalleeSession::getCredentials() {
  return cred.get();
}

void DSMCallCalleeSession::setAuthHandler(AmSessionEventHandler* h) {
  auth.reset(h);
}

void DSMCallCalleeSession::onSendRequest(AmSipRequest& req, int& flags)
{
  if (NULL != auth.get()) {
    DBG("auth->onSendRequest cseq = %d\n", req.cseq);
    auth->onSendRequest(req, flags);
  }
  
  AmB2BCalleeSession::onSendRequest(req, flags);
}

void DSMCallCalleeSession::onSipReply(const AmSipRequest& req, const AmSipReply& reply, 
				      AmBasicSipDialog::Status old_dlg_status)
{
  // call event handlers where it is not done 
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();
  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.body.getCTStr().c_str());
  if(fwd) {
    CALL_EVENT_H(onSipReply, req, reply, old_dlg_status);
  }


  if (NULL == auth.get()) {
    AmB2BCalleeSession::onSipReply(req, reply, old_dlg_status);
    return;
  }
  
  unsigned int cseq_before = dlg->cseq;
  if (!auth->onSipReply(req, reply, old_dlg_status)) {
    AmB2BCalleeSession::onSipReply(req, reply, old_dlg_status);
  } else {
    if (cseq_before != dlg->cseq) {
      DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
          "updating relayed_req map\n", reply.cseq, cseq_before);
      updateUACTransCSeq(reply.cseq, cseq_before);
    }
  }
}
