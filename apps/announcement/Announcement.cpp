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

#include "Announcement.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmUACAuth.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "announcement"

EXPORT_SESSION_FACTORY(AnnouncementFactory,MOD_NAME);

string AnnouncementFactory::AnnouncePath;
string AnnouncementFactory::AnnounceFile;
bool   AnnouncementFactory::Loop = false;

AnnouncementFactory::AnnouncementFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int AnnouncementFactory::onLoad()
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

  Loop = cfg.getParameter("loop") == "true";

  return 0;
}

string AnnouncementFactory::getAnnounceFile(const AmSipRequest& req) {
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

AmSession* AnnouncementFactory::onInvite(const AmSipRequest& req, const string& app_name,
					 const map<string,string>& app_params)
{
  return new AnnouncementDialog(getAnnounceFile(req), NULL);
}

AmSession* AnnouncementFactory::onInvite(const AmSipRequest& req, const string& app_name,
					 AmArg& session_params)
{
  UACAuthCred* cred = AmUACAuth::unpackCredentials(session_params);
  AmSession* s = new AnnouncementDialog(getAnnounceFile(req), cred);
  
  if (NULL == cred) {
    WARN("discarding unknown session parameters.\n");
  } else {
    AmUACAuth::enable(s);
  }

  return s;
}

AnnouncementDialog::AnnouncementDialog(const string& filename, 
				       UACAuthCred* credentials)
  : filename(filename), cred(credentials)
{
  // we can drop all received packets
  // this disables DTMF detection as well
  setReceiving(false);
}

AnnouncementDialog::~AnnouncementDialog()
{
}

void AnnouncementDialog::onSessionStart() {
  DBG("AnnouncementDialog::onSessionStart()...\n");

  if(wav_file.open(filename,AmAudioFile::Read)) {
    ERROR("Couldn't open file %s.\n", filename.c_str());
    throw string("AnnouncementDialog::onSessionStart: Cannot open file\n");
  }

  if (AnnouncementFactory::Loop) 
    wav_file.loop.set(true);

  setOutput(&wav_file);

  AmSession::onSessionStart();
}

void AnnouncementDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  AmSession::onBye(req);
}


void AnnouncementDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    dlg->bye();
    setStopped();
    return;
  }

  AmSession::process(event);
}

inline UACAuthCred* AnnouncementDialog::getCredentials() {
  return cred.get();
}
