/*
 * Copyright (C) 2008 iptego GmbH
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

#include "AnnRecorder.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmPromptCollection.h"
#include "AmUriParser.h"
#include "../msg_storage/MsgStorageAPI.h"
#include "sems.h"
#include "log.h"

using std::map;

#define MOD_NAME "annrecorder"

#define DEFAULT_TYPE "vm"
#define DOMAIN_PROMPT_SUFFIX "-prompts"

#define TIMERID_START_TIMER   1
#define TIMERID_CONFIRM_TIMER 2

#define START_RECORDING_TIMEOUT    20
#define CONFIRM_RECORDING_TIMEOUT  20

#define SEP_CONFIRM_BEGIN  1
#define SEP_MSG_BEGIN      2

#define MAX_MESSAGE_TIME 120

EXPORT_SESSION_FACTORY(AnnRecorderFactory,MOD_NAME);

string AnnRecorderFactory::AnnouncePath;
string AnnRecorderFactory::DefaultAnnounce;
bool   AnnRecorderFactory::SimpleMode=false;
AmDynInvokeFactory* AnnRecorderFactory::message_storage_fact = NULL;

const char* MsgStrError(int e) {
  switch (e) {
  case MSG_OK: return "MSG_OK"; break;
  case MSG_EMSGEXISTS: return "MSG_EMSGEXISTS"; break;
  case MSG_EUSRNOTFOUND: return "MSG_EUSRNOTFOUND"; break;
  case MSG_EMSGNOTFOUND: return "MSG_EMSGNOTFOUND"; break;
  case MSG_EALREADYCLOSED: return "MSG_EALREADYCLOSED"; break;
  case MSG_EREADERROR: return "MSG_EREADERROR"; break;
  case MSG_ENOSPC: return "MSG_ENOSPC"; break;
  case MSG_ESTORAGE: return "MSG_ESTORAGE"; break;
  default: return "Unknown Error";
  }
}

AnnRecorderFactory::AnnRecorderFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int AnnRecorderFactory::onLoad()
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
  DefaultAnnounce = cfg.getParameter("default_announce");

  SimpleMode = (cfg.getParameter("simple_mode") == "yes");

  AM_PROMPT_START;
  AM_PROMPT_ADD(WELCOME, ANNREC_ANNOUNCE_PATH WELCOME".wav");
  AM_PROMPT_ADD(YOUR_PROMPT, ANNREC_ANNOUNCE_PATH YOUR_PROMPT".wav");
  AM_PROMPT_ADD(TO_RECORD, ANNREC_ANNOUNCE_PATH TO_RECORD".wav");
  AM_PROMPT_ADD(CONFIRM, ANNREC_ANNOUNCE_PATH CONFIRM".wav");
  AM_PROMPT_ADD(GREETING_SET, ANNREC_ANNOUNCE_PATH GREETING_SET".wav");
  AM_PROMPT_ADD(BYE, ANNREC_ANNOUNCE_PATH BYE".wav");
  AM_PROMPT_ADD(BEEP, ANNREC_ANNOUNCE_PATH BEEP".wav");
  AM_PROMPT_END(prompts, cfg, MOD_NAME);

  message_storage_fact = AmPlugIn::instance()->getFactory4Di("msg_storage");
  if(!message_storage_fact) {
    ERROR("sorry, could not get msg_storage, please load a suitable plug-in\n");
    return -1;
  }

  return 0;
}

void AnnRecorderFactory::getAppParams(const AmSipRequest& req, map<string, string>& params) {
  string language;
  string domain;
  string user;
  string typ;

  if (SimpleMode){
    AmUriParser p;
    p.uri = req.from_uri;
    if (!p.parse_uri()) {
      DBG("parsing From-URI '%s' failed\n", p.uri.c_str());
      throw AmSession::Exception(500, MOD_NAME ": could not parse From-URI");
    }
    user = p.uri_user;
    //domain = p.uri_domain;
    domain = "default";
    typ = DEFAULT_TYPE;
  }
  else {
    string iptel_app_param = getHeader(req.hdrs, PARAM_HDR, true);

    if (!iptel_app_param.length()) {
      throw AmSession::Exception(500, MOD_NAME ": parameters not found");
    }

    language = get_header_keyvalue(iptel_app_param, "lng", "Language");

    user = get_header_keyvalue(iptel_app_param,"uid", "UserID");
    if (user.empty()) {
      user = get_header_keyvalue(iptel_app_param,"usr", "User");
      if (!user.length())
	user = req.user;
    }

    domain = get_header_keyvalue(iptel_app_param, "did", "DomainID");
    if (domain.empty()){
      domain = get_header_keyvalue(iptel_app_param, "dom", "Domain");
      if (domain.empty())
	domain = req.domain;
    }

    typ = get_header_keyvalue(iptel_app_param, "typ", "Type");
    if (!typ.length())
      typ = DEFAULT_TYPE;
  }

  // checks
  if (user.empty())
    throw AmSession::Exception(500, MOD_NAME ": user missing");

  string announce_file = add2path(AnnouncePath,2, domain.c_str(), (user + ".wav").c_str());
  if (file_exists(announce_file)) goto announce_found;

  if (!language.empty()) {
    announce_file = add2path(AnnouncePath,3, domain.c_str(), language.c_str(), DefaultAnnounce.c_str());
    if (file_exists(announce_file)) goto announce_found;
  }

  announce_file = add2path(AnnouncePath,2, domain.c_str(), DefaultAnnounce.c_str());
  if (file_exists(announce_file)) goto announce_found;

  if (!language.empty()) {
    announce_file = add2path(AnnouncePath,2, language.c_str(),  DefaultAnnounce.c_str());
    if (file_exists(announce_file)) goto announce_found;
  }

  announce_file = add2path(AnnouncePath,1, DefaultAnnounce.c_str());
  if (!file_exists(announce_file))
    announce_file = "";

 announce_found:

  DBG(MOD_NAME " invocation parameters: \n");
  DBG(" User:     <%s> \n", user.c_str());
  DBG(" Domain:   <%s> \n", domain.c_str());
  DBG(" Language: <%s> \n", language.c_str());
  DBG(" Type:     <%s> \n", typ.c_str());
  DBG(" Def. File:<%s> \n", announce_file.c_str());

  params["domain"] = domain;
  params["user"] = user;
  params["defaultfile"] = announce_file;
  params["type"] = typ;
}

AmSession* AnnRecorderFactory::onInvite(const AmSipRequest& req, const string& app_name,
					const map<string,string>& app_params)
{
  map<string, string> params;
  getAppParams(req, params);
  return new AnnRecorderDialog(params, prompts, NULL);
}

AmSession* AnnRecorderFactory::onInvite(const AmSipRequest& req, const string& app_name,
					 AmArg& session_params)
{
  UACAuthCred* cred = AmUACAuth::unpackCredentials(session_params);

  map<string, string> params;
  getAppParams(req, params);
  AmSession* s = new AnnRecorderDialog(params, prompts, cred);

  if (nullptr == cred) {
    WARN("discarding unknown session parameters.\n");
  } else {
    AmUACAuth::enable(s);
  }

  return s;
}

AnnRecorderDialog::AnnRecorderDialog(const map<string, string>& params,
				     AmPromptCollection& prompts,
				     UACAuthCred* credentials)
  : prompts(prompts),
    playlist(this), params(params),
    cred(credentials)
{
  msg_storage = AnnRecorderFactory::message_storage_fact->getInstance();
  if(!msg_storage){
    ERROR("could not get a message storage reference\n");
    throw AmSession::Exception(500,"could not get a message storage reference");
  }
}

AnnRecorderDialog::~AnnRecorderDialog()
{
  prompts.cleanup((long)this);
  if (msg_filename.length())
    unlink(msg_filename.c_str());
}

void AnnRecorderDialog::onSessionStart()
{
  DBG("AnnRecorderDialog::onSessionStart\n");

  prompts.addToPlaylist(WELCOME,  (long)this, playlist);
  prompts.addToPlaylist(YOUR_PROMPT,  (long)this, playlist);
  enqueueCurrent();
  prompts.addToPlaylist(TO_RECORD,  (long)this, playlist);
  enqueueSeparator(SEP_MSG_BEGIN);

  // set the playlist as input and output
  setInOut(&playlist,&playlist);
  state = S_WAIT_START;

  AmSession::onSessionStart();
}

void AnnRecorderDialog::enqueueCurrent() {
  wav_file.close();
  FILE* fp = getCurrentMessage();
  if (!fp) {
    DBG("no recorded msg available, using default\n");
    if (wav_file.open(params["defaultfile"], AmAudioFile::Read)) {
      ERROR("opening default greeting file '%s'!\n", params["defaultfile"].c_str());
      return;
    }
  } else {
    if (wav_file.fpopen("aa.wav", AmAudioFile::Read, fp)) {
      ERROR("fpopen message file!\n");
      return;
    }
  }
  playlist.addToPlaylist(new AmPlaylistItem(&wav_file, NULL));
}

void AnnRecorderDialog::onDtmf(int event, int duration_msec) {
  DBG("DTMF %d, %d\n", event, duration_msec);
  // remove timer
  try {
    removeTimers();
  } catch(...) {
    ERROR("Exception caught calling mod api\n");
  }

  switch (state) {
  case S_WAIT_START: {
    DBG("received key %d in state S_WAIT_START: start recording\n", event);
    playlist.flush();

    wav_file.close();
    msg_filename = "/tmp/" + getLocalTag() + ".wav";
    if(wav_file.open(msg_filename,AmAudioFile::Write,false)) {
     ERROR("AnnRecorder: couldn't open %s for writing\n",
	   msg_filename.c_str());
     dlg->bye();
     setStopped();
    }
    wav_file.setRecordTime(MAX_MESSAGE_TIME*1000);
    prompts.addToPlaylist(BEEP,  (long)this, playlist);
    playlist.addToPlaylist(new AmPlaylistItem(NULL,&wav_file));

    state = S_RECORDING;
  } break;

  case S_RECORDING: {
    DBG("received key %d in state S_RECORDING: replay recording\n", event);
    prompts.addToPlaylist(BEEP,  (long)this, playlist);
    playlist.flush();
    replayRecording();

  } break;

  case S_CONFIRM: {
    DBG("received key %d in state S_CONFIRM save or redo\n", event);
    playlist.flush();

    wav_file.close();
    if (event == 1) {
      // save msg
      saveAndConfirm();
    } else {
      prompts.addToPlaylist(TO_RECORD,  (long)this, playlist);
      state = S_WAIT_START;
    }
  } break;

  default: {
    DBG("ignoring key %d in state %d\n",
		 event, state);
  }break;
  }

}

void AnnRecorderDialog::saveAndConfirm() {
//    wav_file.setCloseOnDestroy(false);
  wav_file.close();
  FILE* fp = fopen(msg_filename.c_str(), "r");
  if (fp) {
    saveMessage(fp);
    prompts.addToPlaylist(GREETING_SET,  (long)this, playlist);
  }
  prompts.addToPlaylist(BYE,  (long)this, playlist);
  state = S_BYE;
}

void AnnRecorderDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  setStopped();
}

void AnnRecorderDialog::process(AmEvent* event)
{

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    event->processed = true;
    int timer_id = plugin_event->data.get(0).asInt();
    if (timer_id == TIMERID_START_TIMER) {
      if (S_WAIT_START == state) {
	prompts.addToPlaylist(BYE,  (long)this, playlist);
	state = S_BYE;
      }
      return;
    }
    if (timer_id == TIMERID_CONFIRM_TIMER) {
      saveAndConfirm();
      return;
    }
    ERROR("unknown timer id!\n");
  }

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && (audio_event->event_id == AmAudioEvent::noAudio)){

    if (S_BYE == state) {
      dlg->bye();
      setStopped();
      return;
    }

    if (S_RECORDING == state) {
      replayRecording();
    }
  }

  AmPlaylistSeparatorEvent* pl_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(event);
  if (pl_ev) {
    if ((pl_ev->event_id == SEP_MSG_BEGIN) &&
	(S_WAIT_START == state)) {
      // start timer
      setTimer(TIMERID_START_TIMER, START_RECORDING_TIMEOUT);
      return;
    }

    if ((pl_ev->event_id == SEP_CONFIRM_BEGIN) &&
	(S_CONFIRM == state)) {
      // start timer
      setTimer(TIMERID_CONFIRM_TIMER, CONFIRM_RECORDING_TIMEOUT);
      return;
    }
    return;
  }
  AmSession::process(event);
}

void AnnRecorderDialog::replayRecording() {
  prompts.addToPlaylist(YOUR_PROMPT,  (long)this, playlist);
  DBG("msg_filename = '%s'\n", msg_filename.c_str());
  if (!wav_file.open(msg_filename, AmAudioFile::Read))
    playlist.addToPlaylist(new AmPlaylistItem(&wav_file, NULL));
  prompts.addToPlaylist(CONFIRM,  (long)this, playlist);
  enqueueSeparator(SEP_CONFIRM_BEGIN);
  state = S_CONFIRM;
}

inline UACAuthCred* AnnRecorderDialog::getCredentials() {
  return cred.get();
}

void AnnRecorderDialog::saveMessage(FILE* fp) {
  string msg_name = params["type"]+".wav";
  DBG("message name is '%s'\n", msg_name.c_str());

  AmArg di_args,ret;
  di_args.push((params["domain"]+DOMAIN_PROMPT_SUFFIX).c_str()); // domain
  di_args.push(params["user"].c_str());                          // user
  di_args.push(msg_name.c_str());                                // message name
  AmArg df;
  MessageDataFile df_arg(fp);
  df.setBorrowedPointer(&df_arg);
  di_args.push(df);
  try {
    msg_storage->invoke("msg_new",di_args,ret);
  } catch(string& s) {
    ERROR("invoking msg_new: '%s'\n", s.c_str());
  } catch(...) {
    ERROR("invoking msg_new.\n");
  }
  // TODO: evaluate ret return value
}

FILE* AnnRecorderDialog::getCurrentMessage() {
  string msgname = params["type"]+".wav";
  string& user = params["user"];
  string domain = params["domain"]+DOMAIN_PROMPT_SUFFIX;

  DBG("trying to get message '%s' for user '%s' domain '%s'\n",
      msgname.c_str(), user.c_str(), domain.c_str());
  AmArg di_args,ret;
  di_args.push(domain.c_str());  // domain
  di_args.push(user.c_str());    // user
  di_args.push(msgname.c_str()); // msg name

  msg_storage->invoke("msg_get",di_args,ret);
  if (!ret.size()
      || !isArgInt(ret.get(0))) {
    ERROR("msg_get for user '%s' domain '%s' msg '%s'"
	  " returned no (valid) result.\n",
	  user.c_str(), domain.c_str(),
	  msgname.c_str()
	  );
    return NULL;
  }
  int ecode = ret.get(0).asInt();
  if (MSG_OK != ecode) {
    DBG("msg_get for user '%s' domain '%s' message '%s': %s\n",
	  user.c_str(), domain.c_str(),
	  msgname.c_str(),
	  MsgStrError(ret.get(0).asInt()));

    if ((ret.size() > 1) && isArgAObject(ret.get(1))) {
      MessageDataFile* f =
	dynamic_cast<MessageDataFile*>(ret.get(1).asObject());
      if (NULL != f)
	delete f;
    }

    return NULL;
  }

  if ((ret.size() < 2) ||
      (!isArgAObject(ret.get(1)))) {
    ERROR("msg_get for user '%s' domain '%s' message '%s': "
	  "invalid return value\n",
	  user.c_str(), domain.c_str(),
	  msgname.c_str());
    return NULL;
  }
  MessageDataFile* f =
    dynamic_cast<MessageDataFile*>(ret.get(1).asObject());
  if (NULL == f)
    return NULL;

  FILE* fp = f->fp;
  delete f;
  return fp;
}

void AnnRecorderDialog::enqueueSeparator(int id) {
  playlist_separator.reset(new AmPlaylistSeparator(this, id));
  playlist.addToPlaylist(new AmPlaylistItem(playlist_separator.get(), NULL));
}
