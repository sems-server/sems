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

#include "AnnounceB2B.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmApi.h"

#include "AmMediaProcessor.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "ann_b2b"

EXPORT_SESSION_FACTORY(AnnounceB2BFactory,MOD_NAME);

string AnnounceB2BFactory::AnnouncePath;
string AnnounceB2BFactory::AnnounceFile;

AnnounceB2BFactory::AnnounceB2BFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int AnnounceB2BFactory::onLoad()
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
  DBG("AnnounceFile = %s\n",AnnounceFile.c_str());

  string announce_file = AnnouncePath + AnnounceFile;
  if(!file_exists(announce_file)){
    ERROR("default file for ann_b2b module does not exist ('%s').\n",
	  announce_file.c_str());
    return -1;
  }

  return 0;
}

AmSession* AnnounceB2BFactory::onInvite(const AmSipRequest& req, const string& app_name,
					const map<string,string>& app_params)
{
  string announce_path = AnnouncePath;
  string announce_file = announce_path + req.domain
    + "/" + req.user + ".wav";
    
  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    return new AnnounceCallerDialog(announce_file);
    
  announce_file = announce_path + req.user + ".wav";
  DBG("trying '%s'\n",announce_file.c_str());
  if(file_exists(announce_file))
    return new AnnounceCallerDialog(announce_file);
    
  announce_file = AnnouncePath + AnnounceFile;
  return new AnnounceCallerDialog(announce_file);
}

AnnounceCallerDialog::AnnounceCallerDialog(const string& filename)
  : AmB2BCallerSession(),
    filename(filename)
{
  // we want to answer
  //  the call ourself
  set_sip_relay_only(false);
}

void AnnounceCallerDialog::onInvite(const AmSipRequest& req)
{
  callee_addr = req.to;
  callee_uri  = req.r_uri;

  AmB2BCallerSession::onInvite(req);
}

void AnnounceCallerDialog::onSessionStart()
{
  // we can drop all received packets
  // this disables DTMF detection as well
  setReceiving(false);

  if(wav_file.open(filename,AmAudioFile::Read))
    throw string("AnnouncementDialog::onSessionStart: Cannot open file\n");
    
  setOutput(&wav_file);

  AmB2BCallerSession::onSessionStart();
}

void AnnounceCallerDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){

    if (getCalleeStatus() != None)
      return;
	
    // detach this session from the media
    // because we will stay in signaling only
    AmMediaProcessor::instance()->removeSession(this);

    connectCallee(callee_addr, callee_uri);
    return;
  }
    
  AmB2BCallerSession::process(event);
}
