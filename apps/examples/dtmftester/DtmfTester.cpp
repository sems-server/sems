/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "DtmfTester.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "dtmftester"

EXPORT_SESSION_FACTORY(DtmfTesterFactory,MOD_NAME);

string DtmfTesterFactory::AnnouncePath;
string DtmfTesterFactory::AnnounceFile;

DtmfTesterFactory::DtmfTesterFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int DtmfTesterFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  AnnouncePath = cfg.getParameter("announce_path",ANNOUNCE_PATH);
  if( !AnnouncePath.empty() 
      && AnnouncePath[AnnouncePath.length()-1] != '/' )
    AnnouncePath += "/";

  AnnounceFile = cfg.getParameter("default_announce",ANNOUNCE_FILE);

  string announce_file = AnnouncePath + AnnounceFile;
  if(!file_exists(announce_file)){
    ERROR("default file for announcement module does not exist ('%s').\n",
	  announce_file.c_str());
    return -1;
  }

  return 0;
}

string DtmfTesterFactory::getAnnounceFile(const AmSipRequest& req) {
  string announce_path = AnnouncePath;
  string announce_file = announce_path + req.domain 
    + "/" + req.user + ".wav";

  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    goto end;

  announce_file = announce_path + req.user + ".wav";
  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    goto end;

  announce_file = AnnouncePath + AnnounceFile;
    
 end:
  return announce_file;
}

AmSession* DtmfTesterFactory::onInvite(const AmSipRequest& req, const string& app_name,
				       const map<string,string>& app_params)
{
  return new DtmfTesterDialog(getAnnounceFile(req), NULL);
}

AmSession* DtmfTesterFactory::onInvite(const AmSipRequest& req, const string& app_name,
				       AmArg& session_params)
{
  UACAuthCred* cred = AmUACAuth::unpackCredentials(session_params);

  AmSession* s = new DtmfTesterDialog(getAnnounceFile(req), cred); 
  
  if (NULL == cred) {
    WARN("discarding unknown session parameters.\n");
  } else {
    AmUACAuth::enable(s);
  }

  return s;
}

DtmfTesterDialog::DtmfTesterDialog(const string& filename, 
				       UACAuthCred* credentials)
  : filename(filename), cred(credentials), play_list(this)
{
  // try out this inband detector
  setInbandDetector(Dtmf::SpanDSP);
}

DtmfTesterDialog::~DtmfTesterDialog()
{  
  for (vector<AmAudioFile*>::iterator it=del_files.begin(); 
       it != del_files.end(); it++)
    delete *it;

}

void DtmfTesterDialog::onSessionStart()
{
  DBG("DtmfTesterDialog::onSessionStart\n");
  startSession();
  
  AmSession::onSessionStart();
}

void DtmfTesterDialog::startSession(){
 
  if(wav_file.open(filename,AmAudioFile::Read))
    throw string("DtmfTesterDialog::onSessionStart: Cannot open file\n");

  string rec_fname = "/tmp/dtmftest_"+getLocalTag()+".wav";
  if(rec_file.open(rec_fname,AmAudioFile::Write))
    throw string("DtmfTesterDialog::onSessionStart: Cannot open rec_file\n");

  play_list.addToPlaylist(new AmPlaylistItem(&wav_file, NULL));
  setInOut(&rec_file, &play_list);
}

void DtmfTesterDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  setStopped();
}


void DtmfTesterDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    dlg->bye();
    setStopped();
    return;
  }

  AmSession::process(event);
}

inline UACAuthCred* DtmfTesterDialog::getCredentials() {
  return cred.get();
}

void DtmfTesterDialog::onDtmf(int event, int duration) {
  AmAudioFile* f = new AmAudioFile();
  if(f->open(DtmfTesterFactory::AnnouncePath+"/"+int2str(event)+".wav", 
		   AmAudioFile::Read)) {
    ERROR("Cannot open file %s\n", 
	  (DtmfTesterFactory::AnnouncePath+"/"+int2str(event)+".wav").c_str());
  }

  del_files.push_back(f);
  play_list.addToPlaylist(new AmPlaylistItem(f, NULL));
}

