/*
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "ServiceLine.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmApi.h"
#include "AmPlugIn.h"

#include "AmMediaProcessor.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "serviceline"

EXPORT_SESSION_FACTORY(ServiceLineFactory,MOD_NAME);

string ServiceLineFactory::AnnouncePath;
string ServiceLineFactory::AnnounceFile;

string ServiceLineFactory::callee_numbers[10];
	
string ServiceLineFactory::GWDomain;
string ServiceLineFactory::GWUser;
string ServiceLineFactory::GWDisplayname;
	
string ServiceLineFactory::GWAuthuser;
string ServiceLineFactory::GWAuthrealm;
string ServiceLineFactory::GWAuthpwd;

ServiceLineFactory::ServiceLineFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int ServiceLineFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  AnnounceFile = cfg.getParameter("prompt",ANNOUNCE_FILE);
  DBG("Prompt = %s\n",AnnounceFile.c_str());

  string announce_file = AnnounceFile;
  if(!file_exists(announce_file)){
    ERROR("prompt file for serviceline module does not exist ('%s').\n",
	  announce_file.c_str());
    return -1;
  }

  DBG("ServiceLine Connect DTMF Key Mapping:\n");
  for (unsigned int i=0;i<10;i++) {
    callee_numbers[i] = cfg.getParameter("callee_number"+int2str(i));
    DBG("Key %u -> Extension __%s__\n", 
	i, callee_numbers[i].empty()?
	"none":callee_numbers[i].c_str());
  }
  
  GWDomain= cfg.getParameter("gw_domain", "");;
  GWUser= cfg.getParameter("gw_user", "");;
  GWDisplayname= cfg.getParameter("gw_displayname", "");;
  
  GWAuthuser= cfg.getParameter("gw_authuser", "");
  GWAuthrealm=cfg.getParameter("gw_authrealm", ""); // actually unused
  GWAuthpwd=cfg.getParameter("gw_authpwd", "");

  return 0;
}

AmSession* ServiceLineFactory::onInvite(const AmSipRequest& req, const string& app_name,
					const map<string,string>& app_params)
{
  string announce_path = AnnouncePath;
  string announce_file = announce_path + req.domain
    + "/" + req.user + ".wav";
    
  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    new ServiceLineCallerDialog(announce_file);
    
  announce_file = announce_path + req.user + ".wav";
  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    new ServiceLineCallerDialog(announce_file);
    
  announce_file = AnnouncePath + AnnounceFile;
  return new ServiceLineCallerDialog(announce_file);
}

ServiceLineCallerDialog::ServiceLineCallerDialog(const string& filename)
  : filename(filename), 
    playlist(this),
    started(false),
    AmB2ABCallerSession()
{
}

void ServiceLineCallerDialog::onSessionStart()
{
  if (started) {
    // reinvite
    AmB2ABCallerSession::onSessionStart();
    return;
  }
  started = true;

  if(wav_file.open(filename,AmAudioFile::Read))
    throw string("AnnouncementDialog::onSessionStart: Cannot open file\n");
  
  setInOut(&playlist, &playlist);
  playlist.addToPlaylist(new AmPlaylistItem(&wav_file, NULL));

  AmB2ABCallerSession::onSessionStart();
}

void ServiceLineCallerDialog::process(AmEvent* event)
{
    
  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
	
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
    DBG("ignoring end of prompt.\n");
    return;
  }

  AmB2ABCallerSession::process(event);
}

void ServiceLineCallerDialog::onDtmf(int event, int duration)
{
  DBG("DTMF event %d duration %d\n", event, duration);
  if (getCalleeStatus() != None)
    return;
  
  if ((event < 10) && (event >= 0) && 
      (!ServiceLineFactory::callee_numbers[event].empty())) {
    connectCallee("sip:"+ServiceLineFactory::callee_numbers[event]+"@"+ServiceLineFactory::GWDomain, 
		  "sip:"+ServiceLineFactory::callee_numbers[event]+"@"+ServiceLineFactory::GWDomain, 
		  "sip:"+ServiceLineFactory::GWUser+"@"+ServiceLineFactory::GWDomain, 
		  "sip:"+ServiceLineFactory::GWUser+"@"+ServiceLineFactory::GWDomain);
  }

  return;
  
}

AmB2ABCalleeSession* ServiceLineCallerDialog::createCalleeSession() {
  ServiceLineCalleeDialog* sess = 
    new ServiceLineCalleeDialog(getLocalTag(), connector);

  AmUACAuth::enable(sess);

  return sess;
}

ServiceLineCalleeDialog::~ServiceLineCalleeDialog() {
}

ServiceLineCalleeDialog::ServiceLineCalleeDialog(const string& other_tag, 
						 AmSessionAudioConnector* connector) 
  : AmB2ABCalleeSession(other_tag, connector),
    cred(ServiceLineFactory::GWAuthrealm, 
	 ServiceLineFactory::GWAuthuser, 
	 ServiceLineFactory::GWAuthpwd)
{
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
  setDtmfDetectionEnabled(false);
}

UACAuthCred* ServiceLineCalleeDialog::getCredentials() {
  return &cred;
}
