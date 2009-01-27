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

#include "DSMDialog.h"
#include "AmUtils.h"
#include "AmMediaProcessor.h"
#include "DSM.h"

DSMDialog::DSMDialog(AmPromptCollection& prompts,
		     DSMStateDiagramCollection& diags,
		     const string& startDiagName,
		     UACAuthCred* credentials)
  : prompts(prompts), default_prompts(prompts), diags(diags), startDiagName(startDiagName), 
    playlist(this), cred(credentials), 
    rec_file(NULL)
{
  diags.addToEngine(&engine);
}

DSMDialog::~DSMDialog()
{
  for (vector<AmAudio*>::iterator it=
	 audiofiles.begin();it!=audiofiles.end();it++) 
    delete *it;

  used_prompt_sets.insert(&prompts);
  for (set<AmPromptCollection*>::iterator it=
	 used_prompt_sets.begin(); it != used_prompt_sets.end(); it++)
    (*it)->cleanup((long)this);

//   for (map<string, AmPromptCollection*>::iterator it=
// 	 prompt_sets.begin(); it != prompt_sets.end(); it++)
//     it->second->cleanup((long)this);
}

/** returns whether var exists && var==value*/
bool DSMDialog::checkVar(const string& var_name, const string& var_val) {
  map<string, string>::iterator it = var.find(var_name);
  if ((it != var.end()) && (it->second == var_val)) 
    return false;

  return true;
}

void DSMDialog::onInvite(const AmSipRequest& req) {
  bool run_session_invite = engine.onInvite(req, this);

  if (DSMFactory::RunInviteEvent) {
    if (!engine.init(this, startDiagName, DSMCondition::Invite))
      run_session_invite =false;

    run_session_invite &= 
      !checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE);
  }

  if (run_session_invite) 
    AmSession::onInvite(req);
}

void DSMDialog::onSessionStart(const AmSipRequest& req)
{
  DBG("DSMDialog::onSessionStart\n");
  startSession();
}

void DSMDialog::onSessionStart(const AmSipReply& rep)
{
  DBG("DSMDialog::onSessionStart (SEMS originator mode)\n");
  startSession();
}

void DSMDialog::startSession(){
  engine.init(this, startDiagName, DSMCondition::SessionStart);

  setReceiving(true);

  if (checkVar(DSM_CONNECT_SESSION, DSM_CONNECT_SESSION_FALSE)) {
    if (!getInput())
      setInput(&playlist);

    setOutput(&playlist);
  }
}

void DSMDialog::connectMedia() {
  if (!getInput())
    setInput(&playlist);

  setOutput(&playlist);
  AmMediaProcessor::instance()->addSession(this, callgroup);
}


void DSMDialog::onDtmf(int event, int duration_msec) {
  DBG("* Got DTMF key %d duration %d\n", 
      event, duration_msec);

  map<string, string> params;
  params["key"] = int2str(event);
  params["duration"] = int2str(duration_msec);
  engine.runEvent(this, DSMCondition::Key, &params);
}

void DSMDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye\n");
  engine.runEvent(this, DSMCondition::Hangup, NULL);
}

void DSMDialog::process(AmEvent* event)
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
    engine.runEvent(this, DSMCondition::NoAudio, NULL);
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

  AmSession::process(event);
}

inline UACAuthCred* DSMDialog::getCredentials() {
  return cred.get();
}

void DSMDialog::playPrompt(const string& name, bool loop) {
  DBG("playing prompt '%s'\n", name.c_str());
  if (prompts.addToPlaylist(name,  (long)this, playlist, 
			    /*front =*/ false, loop))  {
    if ((var["prompts.default_fallback"] != "yes") ||
      default_prompts.addToPlaylist(name,  (long)this, playlist, 
				    /*front =*/ false, loop)) {
      SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    } else {
      used_prompt_sets.insert(&default_prompts);
      SET_ERRNO(DSM_ERRNO_OK);    
    }      
  } else {
    SET_ERRNO(DSM_ERRNO_OK);
  }
}

void DSMDialog::closePlaylist(bool notify) {
  DBG("close playlist\n");
  playlist.close(notify);  
}

void DSMDialog::playFile(const string& name, bool loop) {
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

  playlist.addToPlaylist(new AmPlaylistItem(af, NULL));
  audiofiles.push_back(af);
  SET_ERRNO(DSM_ERRNO_OK);
}

void DSMDialog::recordFile(const string& name) {
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

void DSMDialog::stopRecord() {
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

void DSMDialog::addPromptSet(const string& name, 
			     AmPromptCollection* prompt_set) {
  if (prompt_set) {
    DBG("adding prompt set '%s'\n", name.c_str());
    prompt_sets[name] = prompt_set;
  } else {
    ERROR("trying to add NULL prompt set\n");
  }
}

void DSMDialog::setPromptSets(map<string, AmPromptCollection*>& 
			      new_prompt_sets) {
  prompt_sets = new_prompt_sets;
}

void DSMDialog::setPromptSet(const string& name) {
  map<string, AmPromptCollection*>::iterator it = 
    prompt_sets.find(name);

  if (it == prompt_sets.end()) {
    ERROR("prompt set %s unknown\n", name.c_str());
    SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return;
  }

  DBG("setting prompt set '%s'\n", name.c_str());
  used_prompt_sets.insert(&prompts);
  prompts = *it->second;
  SET_ERRNO(DSM_ERRNO_OK);
}


void DSMDialog::addSeparator(const string& name) {
  unsigned int id = 0;
  if (str2i(name, id)) {
    SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return;
  }

  AmPlaylistSeparator* sep = new AmPlaylistSeparator(this, id);
  playlist.addToPlaylist(new AmPlaylistItem(sep, sep));
  // for garbage collector
  audiofiles.push_back(sep);
  SET_ERRNO(DSM_ERRNO_OK);
}
