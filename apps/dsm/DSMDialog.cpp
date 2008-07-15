/*
 * $Id:  $
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

DSMDialog::DSMDialog(AmPromptCollection& prompts,
		     DSMStateDiagramCollection& diags,
		     const string& startDiagName,
		     UACAuthCred* credentials)
  : prompts(prompts), diags(diags), startDiagName(startDiagName), 
    playlist(this), cred(credentials), 
    rec_file(NULL)
{
  diags.addToEngine(&engine);
}

DSMDialog::~DSMDialog()
{
  for (vector<AmAudioFile*>::iterator it=
	 audiofiles.begin();it!=audiofiles.end();it++) 
    delete *it;
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
  engine.init(this, startDiagName);

  setReceiving(true);

  if (!getInput())
    setInput(&playlist);

  setOutput(&playlist);
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

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && 
     ((audio_event->event_id == AmAudioEvent::cleared) || 
      (audio_event->event_id == AmAudioEvent::noAudio))){
    // todo: run event
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

  AmSession::process(event);
}

inline UACAuthCred* DSMDialog::getCredentials() {
  return cred.get();
}

void DSMDialog::playPrompt(const string& name) {
  DBG("playing prompt '%s'\n", name.c_str());
  prompts.addToPlaylist(name,  (long)this, playlist);  
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
    return;
  }
  if (loop) 
    af->loop.set(true);

  playlist.addToPlaylist(new AmPlaylistItem(af, NULL));
  audiofiles.push_back(af);
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
    return;
  }
  setInput(rec_file); 
}

void DSMDialog::stopRecord() {
  if (rec_file) {
    setInput(&playlist);
    rec_file->close();
    delete rec_file;
    rec_file = NULL;
  } else {
    WARN("stopRecord: we are not recording\n");
    return;
  }
}
