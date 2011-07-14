/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2007 iptego GmbH
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

#include "CacheAnnounce.h"
#include "AmConfig.h"
#include "AmUtils.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "cacheannounce"

EXPORT_SESSION_FACTORY(CacheAnnounceFactory,MOD_NAME);

string CacheAnnounceFactory::AnnouncePath;
string CacheAnnounceFactory::AnnounceFile;

CacheAnnounceFactory::CacheAnnounceFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int CacheAnnounceFactory::onLoad()
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
		ERROR("default file for cacheannounce module does not exist ('%s').\n",
			  announce_file.c_str());
		return -1;
    }

	if (ann_cache.load(announce_file)) {
		ERROR("file '%s' could not be cached.\n",
			  announce_file.c_str());
		return -1;
	}

    return 0;
}

AmSession* CacheAnnounceFactory::onInvite(const AmSipRequest& req, const string& app_name,
					  const map<string,string>& app_params)
{
    return new CacheAnnounceDialog(&ann_cache);
}

CacheAnnounceDialog::CacheAnnounceDialog(AmFileCache* announce)
    : announce(announce)
{
}

CacheAnnounceDialog::~CacheAnnounceDialog()
{
}

void CacheAnnounceDialog::onSessionStart()
{
    DBG("CacheAnnounceDialog::onSessionStart\n");
    startSession();

    AmSession::onSessionStart();
}

void CacheAnnounceDialog::startSession(){
    setDtmfDetectionEnabled(false);

    wav_file.reset(new AmCachedAudioFile(announce));
    if (!wav_file->is_good())
      throw AmSession::Exception(500, "Internal Err");

    setOutput(wav_file.get());
}

void CacheAnnounceDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}


void CacheAnnounceDialog::process(AmEvent* event)
{

    AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
    if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
	dlg.bye();
	setStopped();
	return;
    }

    AmSession::process(event);
}
