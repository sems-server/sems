/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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
#include "AnnounceTransfer.h"
#include "AmConfig.h"
#include "AmUtils.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "announce_transfer"

EXPORT_SESSION_FACTORY(AnnounceTransferFactory,MOD_NAME);

string AnnounceTransferFactory::AnnouncePath;
string AnnounceTransferFactory::AnnounceFile;

AnnounceTransferFactory::AnnounceTransferFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int AnnounceTransferFactory::onLoad()
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

AmSession* AnnounceTransferFactory::onInvite(const AmSipRequest& req, const string& app_name,
					     const map<string,string>& app_params)
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
  return new AnnounceTransferDialog(announce_file);
}

AnnounceTransferDialog::AnnounceTransferDialog(const string& filename)
  : filename(filename),
    status(Disconnected)
{
}

AnnounceTransferDialog::~AnnounceTransferDialog()
{
}

void AnnounceTransferDialog::onInvite(const AmSipRequest& req) {
  if (status == Disconnected) {
    callee_uri = get_session_param(req.hdrs, "Refer-To");
    if (!callee_uri.length()) {
      callee_uri = getHeader(req.hdrs, "P-Refer-To", true);
      if (callee_uri.length()) {
	INFO("Use of P-Refer-To header is deprecated. "
	     "Use '%s: Refer-To=<uri>' instead.\n",PARAM_HDR);
      }
    }
    if (!callee_uri.length())
      callee_uri = req.r_uri;
    DBG("transfer uri set to '%s'\n", callee_uri.c_str());
  }

  AmSession::onInvite(req);
}

void AnnounceTransferDialog::onSessionStart()
{
  // we can drop all received packets
  // this disables DTMF detection as well
  setReceiving(false);

  DBG("AnnounceTransferDialog::onSessionStart\n");
  if (status == Disconnected) {
    status = Announcing;
    startSession();
  }

  AmSession::onSessionStart();
}

void AnnounceTransferDialog::startSession() {
  if(wav_file.open(filename,AmAudioFile::Read))
    throw string("AnnounceTransferDialog::onSessionStart: Cannot open file\n");
    
  setOutput(&wav_file);
}

void AnnounceTransferDialog::onSipRequest(const AmSipRequest& req)
{
  if((status == Transfering || status == Hangup) && 
     (req.method == "NOTIFY")) {
    try {

      if (strip_header_params(getHeader(req.hdrs,"Event", "o", true)) != "refer") 
	throw AmSession::Exception(481, "Subscription does not exist");

      if (!req.body.isContentType("message/sipfrag"))
	throw AmSession::Exception(415, "Unsupported Media Type");

      string body((const char*)req.body.getPayload(),
		  req.body.getLen());

      if (body.length()<8)
	throw AmSession::Exception(400, "Short Body");
			
      string sipfrag_sline = body.substr(8, body.find("\n") - 8);
      DBG("extracted start line from sipfrag '%s'\n", sipfrag_sline.c_str());
      unsigned int code;
      string res_msg;
			
      if ((body.length() < 11)
	  || (parse_return_code(sipfrag_sline.c_str(), code, res_msg))) {
	throw AmSession::Exception(400, "Bad Request");				
      }

      if ((code >= 200)&&(code < 300)) {
	if (status != Hangup) {
	  status = Hangup;
	  dlg->bye();
	}
	DBG("refer succeeded... stop session\n");
	setStopped();
      } else if (code > 300) {
	DBG("refer failed...\n");
	if (status != Hangup) 
	  dlg->bye();
	setStopped();
      }
      dlg->reply(req, 200, "OK", NULL);
    } catch (const AmSession::Exception& e) {
      dlg->reply(req, e.code, e.reason, NULL);
    }
  } else {
    AmSession::onSipRequest(req);
  }
}

void AnnounceTransferDialog::onSipReply(const AmSipRequest& req, 
					const AmSipReply& rep, 
					AmBasicSipDialog::Status old_dlg_status)
{
  if ((status==Transfering ||status==Hangup)  && 
      req.method == SIP_METH_REFER) {
    if (rep.code >= 300) {
      DBG("refer not accepted, stop session.\n");
      dlg->bye();
      setStopped();
    }
  }

  AmSession::onSipReply(req, rep, old_dlg_status);
}

void AnnounceTransferDialog::onBye(const AmSipRequest& req)
{
  if (status == Transfering) {
    // don't stop session, wait for remote side REFER status 
    // (dialog stays open for the subscription created by REFER)
    status = Hangup; 
  } else {
    DBG("onBye: stopSession\n");
    setStopped();
  }
}

void AnnounceTransferDialog::process(AmEvent* event)
{

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
	
  if(audio_event && (audio_event->event_id == AmAudioEvent::cleared) 
     && (status == Announcing)){
    dlg->refer(callee_uri);
    status = Transfering;
    return;
  }

  AmSession::process(event);
}
