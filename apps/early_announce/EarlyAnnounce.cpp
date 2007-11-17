/*
 * $Id$
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

#include "EarlyAnnounce.h"
#include "AmConfig.h"
#include "AmUtils.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "early_announce"

EXPORT_SESSION_FACTORY(EarlyAnnounceFactory,MOD_NAME);

string EarlyAnnounceFactory::AnnouncePath;
string EarlyAnnounceFactory::AnnounceFile;

EarlyAnnounceFactory::EarlyAnnounceFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int EarlyAnnounceFactory::onLoad()
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
    ERROR("default file for " MOD_NAME " module does not exist ('%s').\n",
	  announce_file.c_str());
    return -1;
  }

  return 0;
}


void EarlyAnnounceDialog::onInvite(const AmSipRequest& req) 
{
  try {

    string sdp_reply;
    acceptAudio(req.body,req.hdrs,&sdp_reply);

    if(dlg.reply(req,183,"Session Progress",
		 "application/sdp",sdp_reply) != 0){

      throw AmSession::Exception(500,"could not reply");
    }
    else {
	    
      localreq = req;
    }

  } catch(const AmSession::Exception& e) {

    ERROR("%i %s\n",e.code,e.reason.c_str());
    setStopped();
    AmSipDialog::reply_error(req,e.code,e.reason);
  }
}


AmSession* EarlyAnnounceFactory::onInvite(const AmSipRequest& req)
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
  return new EarlyAnnounceDialog(announce_file);
}

EarlyAnnounceDialog::EarlyAnnounceDialog(const string& filename)
  : filename(filename)
{
}

EarlyAnnounceDialog::~EarlyAnnounceDialog()
{
}

void EarlyAnnounceDialog::onSessionStart(const AmSipRequest& req)
{
  // we can drop all received packets
  // this disables DTMF detection as well
  setReceiving(false);

  DBG("EarlyAnnounceDialog::onSessionStart\n");
  if(wav_file.open(filename,AmAudioFile::Read))
    throw string("EarlyAnnounceDialog::onSessionStart: Cannot open file\n");
    
  setOutput(&wav_file);
}

void EarlyAnnounceDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  setStopped();
}

void EarlyAnnounceDialog::onCancel()
{
  dlg.reply(localreq,487,"Call terminated");
  setStopped();
}

void EarlyAnnounceDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && 
     (audio_event->event_id == AmAudioEvent::cleared))
    {
      DBG("AmAudioEvent::cleared\n");
      unsigned int code_i = 404;
      string reason = "Not Found";

      string iptel_app_param = getHeader(localreq.hdrs, PARAM_HDR);
      if (iptel_app_param.length()) {
	string code = get_header_keyvalue(iptel_app_param,"Final-Reply-Code");
	if (code.length() && str2i(code, code_i)) {
	  ERROR("while parsing Final-Reply-Code parameter\n");
	}
	reason = get_header_keyvalue(iptel_app_param,"Final-Reply-Reason");
      } else {
	string code = getHeader(localreq.hdrs,"P-Final-Reply-Code");
	if (code.length() && str2i(code, code_i)) {
	  ERROR("while parsing P-Final-Reply-Code\n");
	}

	string h_reason =  getHeader(localreq.hdrs,"P-Final-Reply-Reason");
	if (h_reason.length()) {
	  INFO("Use of P-Final-Reply-Code/P-Final-Reply-Reason is deprecated. ");
	  INFO("Use '%s: Final-Reply-Code=<code>;"
	       "Final-Reply-Reason=<rs>' instead.\n",PARAM_HDR);
	  reason = h_reason;
	}
      }

      DBG("Replying with code %d %s\n", code_i, reason.c_str());
      dlg.reply(localreq, code_i, reason);
	
      setStopped();
	
      return;
    }

  AmSession::process(event);
}

