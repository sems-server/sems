/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

DSMCall::DSMCall(AmPromptCollection* prompts,
		     DSMStateDiagramCollection& diags,
		     const string& startDiagName,
		     UACAuthCred* credentials)
  : prompts(prompts), default_prompts(prompts), diags(diags), startDiagName(startDiagName), 
    playlist(this), cred(credentials), 
    rec_file(NULL),
    process_invite(true), process_sessionstart(true)
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

//   for (map<string, AmPromptCollection*>::iterator it=
// 	 prompt_sets.begin(); it != prompt_sets.end(); it++)
//     it->second->cleanup((long)this);
}

/** returns whether var exists && var==value*/
bool DSMCall::checkVar(const string& var_name, const string& var_val) {
  map<string, string>::iterator it = var.find(var_name);
  if ((it != var.end()) && (it->second == var_val)) 
    return true;

  return false;
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

  if (DSMFactory::RunInviteEvent) {
    if (!engine.init(this, startDiagName, DSMCondition::Invite))
      run_session_invite =false;

    if (checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE)) {
      DBG("session choose to not connect media\n");
      run_session_invite = false;     // don't accept audio 
    }    
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

  if (DSMFactory::RunInviteEvent) {
    if (!engine.init(this, startDiagName, DSMCondition::Invite))
      run_session_invite =false;

    if (checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE)) {
      DBG("session choose to not connect media\n");
      // TODO: set flag to not connect RTP on session start
      run_session_invite = false;     // don't accept audio 
    }    
  }
}

void DSMCall::onSessionStart(const AmSipRequest& req)
{
  if (process_sessionstart) {
    process_sessionstart = false;
    AmB2BCallerSession::onSessionStart(req);

    DBG("DSMCall::onSessionStart\n");
    startSession();
  }
}

void DSMCall::onSessionStart(const AmSipReply& rep)
{
  if (process_sessionstart) {
    process_sessionstart = false;
    DBG("DSMCall::onSessionStart (SEMS originator mode)\n");
    invite_req.body = rep.body;
 
    startSession();    
  }
}

void DSMCall::startSession(){
  engine.init(this, startDiagName, DSMCondition::SessionStart);

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
  engine.runEvent(this, DSMCondition::Key, &params);
}

void DSMCall::onBye(const AmSipRequest& req)
{
  DBG("onBye\n");
  engine.runEvent(this, DSMCondition::Hangup, NULL);
}

void DSMCall::process(AmEvent* event)
{

  if (event->event_id == DSM_EVENT_ID) {
    DSMEvent* dsm_event = dynamic_cast<DSMEvent*>(event);
    if (dsm_event) {      
      engine.runEvent(this, DSMCondition::DSMEvent, &dsm_event->params);
      return;
    }
  
  }

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && 
     ((audio_event->event_id == AmAudioEvent::cleared) || 
      (audio_event->event_id == AmAudioEvent::noAudio))){
    map<string, string> params;
    params["type"] = audio_event->event_id == AmAudioEvent::cleared?"cleared":"noAudio";
    engine.runEvent(this, DSMCondition::NoAudio, &params);
    return;
  }

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    map<string, string> params;
    params["id"] = int2str(timer_id);
    engine.runEvent(this, DSMCondition::Timer, &params);
  }

  AmPlaylistSeparatorEvent* sep_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(event);
  if (sep_ev) {
    map<string, string> params;
    params["id"] = int2str(sep_ev->event_id);
    engine.runEvent(this, DSMCondition::PlaylistSeparator, &params);
  }

  AmB2BCallerSession::process(event);
}

inline UACAuthCred* DSMCall::getCredentials() {
  return cred.get();
}

void DSMCall::playPrompt(const string& name, bool loop) {
  DBG("playing prompt '%s'\n", name.c_str());
  if (prompts->addToPlaylist(name,  (long)this, playlist, 
			    /*front =*/ false, loop))  {
    if ((var["prompts.default_fallback"] != "yes") ||
      default_prompts->addToPlaylist(name,  (long)this, playlist, 
				    /*front =*/ false, loop)) {
	DBG("checked [%p]\n", default_prompts);
      SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    } else {
      used_prompt_sets.insert(default_prompts);
      SET_ERRNO(DSM_ERRNO_OK);    
    }      
  } else {
    SET_ERRNO(DSM_ERRNO_OK);
  }
}

void DSMCall::closePlaylist(bool notify) {
  DBG("close playlist\n");
  playlist.close(notify);  
}

void DSMCall::addToPlaylist(AmPlaylistItem* item) {
  DBG("add item to playlist\n");
  playlist.addToPlaylist(item);
}

void DSMCall::playFile(const string& name, bool loop, bool front) {
  AmAudioFile* af = new AmAudioFile();
  if(af->open(name,AmAudioFile::Read)) {
    ERROR("audio file '%s' could not be opened for reading.\n", 
	  name.c_str());
    delete af;
    SET_ERRNO(DSM_ERRNO_FILE);
    return;
  }
  if (loop) 
    af->loop.set(true);

  if (front)
    playlist.addToPlayListFront(new AmPlaylistItem(af, NULL));
  else
    playlist.addToPlaylist(new AmPlaylistItem(af, NULL));

  audiofiles.push_back(af);
  SET_ERRNO(DSM_ERRNO_OK);
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
    SET_ERRNO(DSM_ERRNO_FILE);
    return;
  }
  setInput(rec_file); 
  SET_ERRNO(DSM_ERRNO_OK);
}

unsigned int DSMCall::getRecordLength() {
  if (!rec_file) {
    SET_ERRNO(DSM_ERRNO_FILE);
    return 0;
  }
  SET_ERRNO(DSM_ERRNO_OK);
  return rec_file->getLength();
}

unsigned int DSMCall::getRecordDataSize() {
  if (!rec_file) {
    SET_ERRNO(DSM_ERRNO_FILE);
    return 0;
  }
  SET_ERRNO(DSM_ERRNO_OK);
  return rec_file->getDataSize();
}

void DSMCall::stopRecord() {
  if (rec_file) {
    setInput(&playlist);
    rec_file->close();
    delete rec_file;
    rec_file = NULL;
    SET_ERRNO(DSM_ERRNO_OK);
  } else {
    WARN("stopRecord: we are not recording\n");
    SET_ERRNO(DSM_ERRNO_FILE);
    return;
  }
}

void DSMCall::addPromptSet(const string& name, 
			     AmPromptCollection* prompt_set) {
  if (prompt_set) {
    DBG("adding prompt set '%s'\n", name.c_str());
    prompt_sets[name] = prompt_set;
  } else {
    ERROR("trying to add NULL prompt set\n");
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
    SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return;
  }

  DBG("setting prompt set '%s'\n", name.c_str());
  used_prompt_sets.insert(prompts);
  prompts = it->second;
  SET_ERRNO(DSM_ERRNO_OK);
}


void DSMCall::addSeparator(const string& name, bool front) {
  unsigned int id = 0;
  if (str2i(name, id)) {
    SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return;
  }

  AmPlaylistSeparator* sep = new AmPlaylistSeparator(this, id);
  if (front)
    playlist.addToPlayListFront(new AmPlaylistItem(sep, sep));
  else
    playlist.addToPlaylist(new AmPlaylistItem(sep, sep));
  // for garbage collector
  audiofiles.push_back(sep);
  SET_ERRNO(DSM_ERRNO_OK);
}

void DSMCall::transferOwnership(DSMDisposable* d) {
  gc_trash.insert(d);
}

// AmB2BSession methods
void DSMCall::onOtherBye(const AmSipRequest& req) {
  DBG("* Got BYE from other leg\n");

  map<string, string> params;
  params["hdrs"] = req.hdrs; // todo: optimization - make this configurable
  engine.runEvent(this, DSMCondition::B2BOtherBye, &params);
}

bool DSMCall::onOtherReply(const AmSipReply& reply) {
  DBG("* Got reply from other leg: %u %s\n", 
      reply.code, reply.reason.c_str());

  map<string, string> params;
  params["code"] = int2str(reply.code);
  params["reason"] = reply.reason;
  params["hdrs"] = reply.hdrs; // todo: optimization - make this configurable

  engine.runEvent(this, DSMCondition::B2BOtherReply, &params);

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

void DSMCall::B2BaddReceivedRequest(const AmSipRequest& req) {
  DBG("inserting request '%s' with CSeq %d in list of received requests\n", 
      req.method.c_str(), req.cseq);
  recvd_req.insert(std::make_pair(req.cseq, req));
}
