/*
 * $Id: Announcement.cpp,v 1.7.8.4 2005/08/31 13:54:29 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
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
#include "AmSessionScheduler.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "early_announce"

EXPORT_SESSION_FACTORY(AnnouncementFactory,MOD_NAME);

string AnnouncementFactory::AnnouncePath;
string AnnouncementFactory::AnnounceFile;

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
	ERROR("default file for ann_b2b module does not exist ('%s').\n",
	      announce_file.c_str());
	return -1;
    }

    return 0;
}


void AnnouncementDialog::onInvite(const AmSipRequest& req) 
{
    string sdp_reply;
    if(acceptAudio(req,sdp_reply)!=0)
	return;

    if(dlg.reply(req,183,"Session Progress",
		 "application/sdp",sdp_reply) != 0){

	//throw AmSession::Exception(500,"error while sending response");
	setStopped();
    }
    else {

	localreq = req;
    }

// reply_code = 183;                 // early dialog
// reply_reason = "Session Progress";

}


AmSession* AnnouncementFactory::onInvite(const AmSipRequest& req)
{
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
    return new AnnouncementDialog(announce_file);
}

AnnouncementDialog::AnnouncementDialog(const string& filename)
    : filename(filename)
{
}

AnnouncementDialog::~AnnouncementDialog()
{
}

void AnnouncementDialog::onSessionStart(const AmSipRequest& req)
{
    DBG("AnnouncementDialog::onSessionStart\n");
    if(wav_file.open(filename,AmAudioFile::Read))
	throw string("AnnouncementDialog::onSessionStart: Cannot open file\n");
    
    setOutput(&wav_file);
}

void AnnouncementDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}

void AnnouncementDialog::onCancel()
{
    dlg.reply(localreq,487,"Call terminated");
    setStopped();
}

void AnnouncementDialog::process(AmEvent* event)
{

    AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
    if(audio_event)
    {
          switch(audio_event->event_id)
          {

              case AmAudioEvent::cleared:
                    DBG("AmAudioEvent::cleared\n");
                    //dlg.bye();
                    setStopped();
                    return;
                    break;

              case AmAudioEvent::noAudio:
                    DBG("AmAudioEvent::noAudio\n");
                    unsigned int code_i;
  		    string code = getHeader(localreq.hdrs,"P-Final-Reply-Code");
		    string reason =  getHeader(localreq.hdrs,"P-Final-Reply-Reason");

                    if (code.length() && reason.length() && !str2i(code, code_i) ) {
                        DBG("Replying with code %d %s\n", code_i, reason.c_str());
                        dlg.reply(localreq, code_i, reason);
                    } else {
                        DBG("Replying with std code 404 Not found\n");
                        dlg.reply(localreq, 404, "Not Found");
                    }
                    setStopped();
                    return;
                    break;
          }
    }

    AmSession::process(event);
}

