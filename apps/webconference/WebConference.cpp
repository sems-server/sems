/*
 * $Id: WebConference.cpp 288 2007-03-28 16:32:02Z sayer $
 *
 * Copyright (C) 2007 iptego GmbH
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

#include "WebConference.h"
#include "AmConferenceStatus.h"
#include "AmUtils.h"
#include "log.h"
#include "AmUAC.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"

#include "sems.h" // DEFAULT_SIGNATURE

#include <stdlib.h>

#define APP_NAME "webconference"

EXPORT_SESSION_FACTORY(WebConferenceFactory,APP_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(WebConferenceFactory,APP_NAME);

WebConferenceFactory::WebConferenceFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    AmDynInvokeFactory(_app_name),
    configured(false),
    use_direct_room(false),
    direct_room_strip(0)
{
  if (NULL == _instance) {
    _instance = this;
  }
}

WebConferenceFactory* WebConferenceFactory::_instance=0;

string WebConferenceFactory::DigitsDir;
PlayoutType WebConferenceFactory::m_PlayoutType = ADAPTIVE_PLAYOUT;

int WebConferenceFactory::onLoad()
{
  // only execute this once
  if (configured) 
    return 0;
  configured = true;

  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  // get prompts
  AM_PROMPT_START;
  AM_PROMPT_ADD(FIRST_PARTICIPANT, WEBCONF_ANNOUNCE_PATH "first_paricipant.wav");
  AM_PROMPT_ADD(JOIN_SOUND,        WEBCONF_ANNOUNCE_PATH "beep.wav");
  AM_PROMPT_ADD(DROP_SOUND,        WEBCONF_ANNOUNCE_PATH "beep.wav");
  AM_PROMPT_ADD(ENTER_PIN,         WEBCONF_ANNOUNCE_PATH "enter_pin.wav");
  AM_PROMPT_ADD(WRONG_PIN,         WEBCONF_ANNOUNCE_PATH "wrong_pin.wav");
  AM_PROMPT_ADD(ENTERING_CONFERENCE, WEBCONF_ANNOUNCE_PATH "entering_conference.wav");
  AM_PROMPT_END(prompts, cfg, APP_NAME);

  DigitsDir = cfg.getParameter("digits_dir");
  if (DigitsDir.length() && DigitsDir[DigitsDir.length()-1]!='/')
    DigitsDir+='/';

  if (!DigitsDir.length()) {
    WARN("No digits_dir specified in configuration.\n");
  }
  for (int i=0;i<10;i++) 
    prompts.setPrompt(int2str(i), DigitsDir+int2str(i)+".wav", APP_NAME);

  string playout_type = cfg.getParameter("playout_type");
  if (playout_type == "simple") {
    m_PlayoutType = SIMPLE_PLAYOUT;
    DBG("Using simple (fifo) buffer as playout technique.\n");
  } else 	if (playout_type == "adaptive_jb") {
    m_PlayoutType = JB_PLAYOUT;
    DBG("Using adaptive jitter buffer as playout technique.\n");
  } else {
    DBG("Using adaptive playout buffer as playout technique.\n");
  }
  
  string direct_room_re_str = cfg.getParameter("direct_room_re");
  if (!direct_room_re_str.length()) {
    DBG("no direct room access prefixes set.\n");
  } else {
    if (regcomp(&direct_room_re, direct_room_re_str.c_str(), 
		 REG_EXTENDED|REG_NOSUB)) {
      ERROR("unable to compile direct room RE '%s'.\n",
	    direct_room_re_str.c_str());
      return -1;
    }
    use_direct_room = true;
    string direct_room_strip_str = cfg.getParameter("direct_room_strip");
    if (direct_room_strip_str.length() &&
	str2i(direct_room_strip_str, direct_room_strip)) {
      ERROR("unable to decipher direct_room_strip amount '%s'\n",
	    direct_room_strip_str.c_str());
      return -1;
    }
    DBG("Webconference will strip %d leading characters from "
	"direct room access usernames\n",
	direct_room_strip);
  }

  string feedback_filename = cfg.getParameter("feedback_file");
  if (!feedback_filename.empty()) {
    feedback_file.open(feedback_filename.c_str(), std::ios::out);
    if (!feedback_file.good()) {
      ERROR("opening feedback file '%s'\n", 
	    feedback_filename.c_str());
    } else {
      DBG("successfully opened feedback file '%s'\n", 
	    feedback_filename.c_str());
    }
  }

  return 0;
}

void WebConferenceFactory::newParticipant(const string& conf_id, 
					  const string& localtag, 
					  const string& number) {
  rooms_mut.lock();
  rooms[conf_id].newParticipant(localtag, number);
  rooms_mut.unlock();
}

void WebConferenceFactory::updateStatus(const string& conf_id, 
					const string& localtag, 
					ConferenceRoomParticipant::ParticipantStatus status,
					const string& reason) {
  rooms_mut.lock();
  rooms[conf_id].updateStatus(localtag, status, reason);
  rooms_mut.unlock();
}

ConferenceRoom* WebConferenceFactory::getRoom(const string& room, 
					      const string& adminpin) {
  ConferenceRoom* res = NULL;
  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it == rooms.end()) {
    // (re)open room
    rooms[room] = ConferenceRoom();
    rooms[room].adminpin = adminpin;   
    res = &rooms[room];
  } else {
    if (!it->second.adminpin.empty() && 
	(it->second.adminpin != adminpin)) {
      // wrong pin
    } else {
      // update adminpin if room was created by dialin
      if (it->second.adminpin.empty()) 
	it->second.adminpin = adminpin;
      res = &it->second;
    } 
  }

  return res;
}

// incoming calls 
AmSession* WebConferenceFactory::onInvite(const AmSipRequest& req)
{
  if (use_direct_room) {
    if (!regexec(&direct_room_re, req.to.c_str(), 0,0,0)) {
      string room = req.user;
      if (room.length() > direct_room_strip) 
 	room = room.substr(direct_room_strip);
      DBG("direct room access match. connecting to room '%s'\n", 
	  room.c_str());
      return new WebConferenceDialog(prompts, getInstance(), room);
    }
  } 
  return new WebConferenceDialog(prompts, getInstance(), NULL);
}

// outgoing calls 
AmSession* WebConferenceFactory::onInvite(const AmSipRequest& req,
					  AmArg& session_params)
{
  UACAuthCred* cred = NULL;
  if (session_params.getType() == AmArg::AObject) {
    ArgObject* cred_obj = session_params.asObject();
    if (cred_obj)
      cred = dynamic_cast<UACAuthCred*>(cred_obj);
  }

  AmSession* s = new WebConferenceDialog(prompts, getInstance(), cred); 
  
  AmSessionEventHandlerFactory* uac_auth_f = 
    AmPlugIn::instance()->getFactory4Seh("uac_auth");
  if (uac_auth_f != NULL) {
    DBG("UAC Auth enabled for new announcement session.\n");
    AmSessionEventHandler* h = uac_auth_f->getHandler(s);
    if (h != NULL )
      s->addHandler(h);
  } else {
    ERROR("uac_auth interface not accessible. Load uac_auth for authenticated dialout.\n");
  }		

  return s;
}

void WebConferenceFactory::invoke(const string& method, 
				  const AmArg& args, 
				  AmArg& ret)
{
  

  if(method == "roomCreate"){
    roomCreate(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "roomInfo"){
    roomInfo(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "dialout"){
    dialout(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "mute"){
    mute(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "unmute"){
    unmute(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "kickout"){
    kickout(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "serverInfo"){
    serverInfo(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "vqRoomFeedback"){
    vqRoomFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "vqCallFeedback"){
    vqCallFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "vqConferenceFeedback"){
    vqConferenceFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "help"){
    ret.push("help text goes here");
    ret.push(getServerInfoString().c_str());
  } else if(method == "resetFeedback"){
    resetFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "flushFeedback"){
    flushFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "_list"){
    ret.push("roomCreate");
    ret.push("roomInfo");
    ret.push("dialout");
    ret.push("mute");
    ret.push("unmute");
    ret.push("kickout");
    ret.push("serverInfo");
    ret.push("vqConferenceFeedback");
    ret.push("vqCallFeedback");
    ret.push("vqRoomFeedback");
  } else
    throw AmDynInvoke::NotImplemented(method);
}

string WebConferenceFactory::getServerInfoString() {
  return DEFAULT_SIGNATURE  " calls: " + 
    int2str(AmSessionContainer::instance()->getSize()); 
}


string WebConferenceFactory::getRandomPin() {
  string res;
  for (int i=0;i<6;i++)
    res+=(char)('0'+random()%10);
  return res;
}

void WebConferenceFactory::roomCreate(const AmArg& args, AmArg& ret) {
  assertArgCStr(args.get(0));
  string room = args.get(0).asCStr();
  rooms_mut.lock();
  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it == rooms.end()) {
    rooms[room] = ConferenceRoom();
    rooms[room].adminpin = getRandomPin();
    ret.push(0);
    ret.push("OK");
    ret.push(rooms[room].adminpin.c_str());
  } else {
    if (rooms[room].adminpin.empty()) {
      rooms[room].adminpin = getRandomPin();
      ret.push(0);
      ret.push("OK");
      ret.push(rooms[room].adminpin.c_str());
    } else {
      ret.push(1);
      ret.push("room already opened");
      ret.push("");
    }
  }
  rooms_mut.unlock();
}

void WebConferenceFactory::roomInfo(const AmArg& args, AmArg& ret) {
  assertArgCStr(args.get(0));
  assertArgCStr(args.get(1));
  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();;

   rooms_mut.lock();
   ConferenceRoom* r = getRoom(room, adminpin);
   if (NULL == r) {
    ret.push(1);
    ret.push("wrong adminpin");
    // for consistency, add an empty array
    AmArg a;
    a.assertArray(0);
    ret.push(a);
   } else {
     ret.push(0);
     ret.push("OK");
     ret.push(r->asArgArray());
   }
   rooms_mut.unlock();
}

void WebConferenceFactory::dialout(const AmArg& args, AmArg& ret) {
  for (int i=0;i<6;i++)
    assertArgCStr(args.get(1));

  string room        = args.get(0).asCStr();
  string adminpin    = args.get(1).asCStr();
  string callee      = args.get(2).asCStr();
  string from_user   = args.get(3).asCStr();
  string domain      = args.get(4).asCStr();
  string auth_user   = args.get(5).asCStr();
  string auth_pwd    = args.get(6).asCStr();

  string from = "sip:" + from_user + "@" + domain;
  string to   = "sip:" + callee + "@" + domain;

  // check adminpin
  rooms_mut.lock();
  ConferenceRoom* r = getRoom(room, adminpin);
  rooms_mut.unlock();
  if (NULL == r) {
      ret.push(1);
      ret.push("wrong adminpin");
      ret.push("");
      return;
  }

  DBG("dialout webconference room '%s', from '%s', to '%s'", 
      room.c_str(), from.c_str(), to.c_str());

  AmArg* a = new AmArg();
  a->setBorrowedPointer(new UACAuthCred("", auth_user, auth_pwd));

  AmSession* s = AmUAC::dialout(room.c_str(), APP_NAME,  to,  
				"<" + from +  ">", from, "<" + to + ">", 
				string(""), // local tag
				string(""), // hdrs
				a);
  if (s) {
    string localtag = s->getLocalTag();
    ret.push(0);
    ret.push("OK");
    ret.push(localtag.c_str());
    newParticipant(room, localtag, to);
    updateStatus(room, localtag,
		 ConferenceRoomParticipant::Connecting,
		 "INVITE");
  }
  else {
    ret.push(1);
    ret.push("internal error");
    ret.push("");
  }
}

void WebConferenceFactory::postConfEvent(const AmArg& args, AmArg& ret,
					 int id, int mute) {
  for (int i=0;i<2;i++)
    assertArgCStr(args.get(1));
  string room        = args.get(0).asCStr();
  string adminpin    = args.get(1).asCStr();
  string call_tag    = args.get(2).asCStr();

  // check adminpin
  
  rooms_mut.lock();
  ConferenceRoom* r = getRoom(room, adminpin);
  if (NULL == r) {
      ret.push(1);
      ret.push("wrong adminpin");
      rooms_mut.unlock();  
      return;
  } 
  bool p_exists = r->hasParticipant(call_tag);  
  if (p_exists && (mute >= 0))
    r->setMuted(call_tag, mute);

  rooms_mut.unlock();  

  if (p_exists) {
    AmSessionContainer::instance()->postEvent(call_tag, 
					      new WebConferenceEvent(id));
    ret.push(0);
    ret.push("OK");
  } else {
    ret.push(2);
    ret.push("call does not exist");
  }
}

void WebConferenceFactory::kickout(const AmArg& args, AmArg& ret) {
  postConfEvent(args, ret, WebConferenceEvent::Kick, -1);
}

void WebConferenceFactory::mute(const AmArg& args, AmArg& ret) {
  postConfEvent(args, ret, WebConferenceEvent::Mute, 1);
}

void WebConferenceFactory::unmute(const AmArg& args, AmArg& ret) {
  postConfEvent(args, ret, WebConferenceEvent::Unmute, 0);
}

void WebConferenceFactory::serverInfo(const AmArg& args, AmArg& ret) {
  ret.push(getServerInfoString().c_str());
}

void WebConferenceFactory::vqRoomFeedback(const AmArg& args, AmArg& ret) {
  
  assertArgCStr(args.get(0));
  assertArgCStr(args.get(1));
  assertArgInt(args.get(2));

  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  int opinion = args.get(2).asInt();

  saveFeedback(string("RO "+ room + "|||" + adminpin + "|||" + 
	       int2str(opinion) + "|||" + int2str(time(NULL)) + "|||\n"));

  ret.push(0);
  ret.push("OK");
}

void WebConferenceFactory::vqCallFeedback(const AmArg& args, AmArg& ret) {
  assertArgCStr(args.get(0));
  assertArgCStr(args.get(1));
  assertArgCStr(args.get(2));
  assertArgInt(args.get(3));

  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  string tag = args.get(2).asCStr();
  int opinion = args.get(3).asInt();

  saveFeedback("CA|||"+ room + "|||" + adminpin + "|||" + tag + "|||" + 
	       int2str(opinion) + "|||" + int2str(time(NULL)) + "|||\n");

  ret.push(0);
  ret.push("OK");
}

void WebConferenceFactory::vqConferenceFeedback(const AmArg& args, AmArg& ret) {
  assertArgCStr(args.get(0));
  assertArgCStr(args.get(1));
  assertArgCStr(args.get(2));
  assertArgCStr(args.get(3));
  assertArgInt(args.get(4));

  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  string sender = args.get(2).asCStr();
  string comment = args.get(3).asCStr();
  int opinion = args.get(4).asInt();

  saveFeedback("CO|||"+ room + "|||" + adminpin + "|||" + int2str(opinion) + "|||" + 
	       sender + "|||" + comment +"|||" + int2str(time(NULL)) + "|||\n");

  ret.push(0);
  ret.push("OK");
}

void WebConferenceFactory::resetFeedback(const AmArg& args, AmArg& ret) {
  assertArgCStr(args.get(0));
  string feedback_filename = args.get(0).asCStr();

  feedback_file.close();
  if (!feedback_filename.empty()) {
    feedback_file.open(feedback_filename.c_str(), std::ios::out);
    if (!feedback_file.good()) {
      ERROR("opening new feedback file '%s'\n", 
	    feedback_filename.c_str());
      ret.push(-1);
      ret.push("error opening new feedback file");

    } else {
      DBG("successfully opened new feedback file '%s'\n", 
	  feedback_filename.c_str());
      ret.push(0);
      ret.push("OK");
    }
  } else {
    ret.push(-2);
    ret.push("no filename given");
  }
}

void WebConferenceFactory::saveFeedback(const string& s) {
  if (feedback_file.good()){
    feedback_file << s;
  }
}

void WebConferenceFactory::flushFeedback(const AmArg& args, AmArg& ret) {
  feedback_file.flush();
}

WebConferenceDialog::WebConferenceDialog(AmPromptCollection& prompts,
					 WebConferenceFactory* my_f,
					 UACAuthCred* cred)
  : play_list(this), separator(this, 0), prompts(prompts), state(None),
    factory(my_f), cred(cred)
{
  is_dialout = (cred != NULL);
  // set configured playout type
  rtp_str.setPlayoutType(WebConferenceFactory::m_PlayoutType);
}

WebConferenceDialog::WebConferenceDialog(AmPromptCollection& prompts,
					 WebConferenceFactory* my_f,
					 const string& room)
  : play_list(this), separator(this, 0), prompts(prompts), state(None),
    factory(my_f)
{
  conf_id = room;
  DBG("set conf_id to %s\n", conf_id.c_str());
  is_dialout = false;
  // set configured playout type
  rtp_str.setPlayoutType(WebConferenceFactory::m_PlayoutType);
}

WebConferenceDialog::~WebConferenceDialog()
{
  prompts.cleanup((long)this);
  play_list.close(false);
  if (is_dialout || (InConference == state)) {
    factory->updateStatus(is_dialout?dlg.user:conf_id, 
			  getLocalTag(), 
			  ConferenceRoomParticipant::Finished,
			  "");
  }
}

void WebConferenceDialog::connectConference(const string& room) {
  // set the conference id ('conference room') 
  conf_id = room;

  // disconnect in/out for safety 
  setInOut(NULL, NULL);

  // we need to be in the same callgroup as the other 
  // people in the conference (important if we have multiple
  // MediaProcessor threads
  changeCallgroup(conf_id);

  // get a channel from the status 
  channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag()));

  // clear the playlist
  play_list.close();

  // add the channel to our playlist
  play_list.addToPlaylist(new AmPlaylistItem(channel.get(),
					     channel.get()));

  // set the playlist as input and output
  setInOut(&play_list,&play_list);

}

void WebConferenceDialog::onSessionStart(const AmSipRequest& req) { 
  // direct room access?
  if (conf_id.empty()) {
    state = EnteringPin;    
    prompts.addToPlaylist(ENTER_PIN,  (long)this, play_list);
    // set the playlist as input and output
    setInOut(&play_list,&play_list);
  } else {
    DBG("########## direct connect conference #########\n"); 
    factory->newParticipant(conf_id, 
			    getLocalTag(), 
			    dlg.remote_party);
    factory->updateStatus(conf_id, 
			  getLocalTag(), 
			  ConferenceRoomParticipant::Connected,
			  "direct access: entered");
    state = InConference;
    connectConference(conf_id);
  }
}

void WebConferenceDialog::onSessionStart(const AmSipReply& rep) { 
  DBG("########## dialout: connect conference #########\n"); 
  state = InConference;
  connectConference(dlg.user);
}

void WebConferenceDialog::onSipReply(const AmSipReply& reply) {
  AmSession::onSipReply(reply);

  if (is_dialout) {
    // map AmSipDialog state to WebConferenceState
    ConferenceRoomParticipant::ParticipantStatus rep_st = ConferenceRoomParticipant::Connecting;
    switch (dlg.getStatus()) {
    case AmSipDialog::Pending: {
      rep_st = ConferenceRoomParticipant::Connecting;
      if (reply.code == 180) 
	rep_st  = ConferenceRoomParticipant::Ringing;
    } break;
    case AmSipDialog::Connected: 
      rep_st = ConferenceRoomParticipant::Connected; break;
    case AmSipDialog::Disconnecting: 
      rep_st = ConferenceRoomParticipant::Disconnecting; break;    
    }
    DBG("is dialout: updateing status\n");
    factory->updateStatus(dlg.user, getLocalTag(), 
			  rep_st, int2str(reply.code) + " " + reply.reason);
  }
}
 
void WebConferenceDialog::onBye(const AmSipRequest& req)
{
  if (InConference == state) {
    factory->updateStatus(conf_id, 
			  getLocalTag(), 
			  ConferenceRoomParticipant::Disconnecting,
			  req.method);
  }

  disconnectConference();
}

void WebConferenceDialog::disconnectConference() {
  play_list.close();
  setInOut(NULL,NULL);
  channel.reset(NULL);
  setStopped();
}

void WebConferenceDialog::process(AmEvent* ev)
{
  // check conference events 
  ConferenceEvent* ce = dynamic_cast<ConferenceEvent*>(ev);
  if(ce && (conf_id == ce->conf_id)){
    switch(ce->event_id){

    case ConfNewParticipant: {
      DBG("########## new participant #########\n");
      if(ce->participants == 1){
	prompts.addToPlaylist(FIRST_PARTICIPANT, (long)this, play_list, true);
      } else {
	prompts.addToPlaylist(JOIN_SOUND, (long)this, play_list, true);
      }
    } break;
    
    case ConfParticipantLeft: {
      DBG("########## participant left ########\n");
      prompts.addToPlaylist(DROP_SOUND, (long)this, play_list, true);
    } break;

    default:
      break;
    }
    return;
  }

  // our item will fire this event
  AmPlaylistSeparatorEvent* sep_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(ev);
  if (NULL != sep_ev) {
    // don't care for the id here
    if (EnteringConference == state) {
      state = InConference;
      DBG("########## connectConference after pin entry #########\n");
      connectConference(pin_str);
      factory->newParticipant(pin_str, 
			      getLocalTag(), 
			      dlg.remote_party);
      factory->updateStatus(pin_str, 
			    getLocalTag(), 
			    ConferenceRoomParticipant::Connected,
			    "entered");
    }    
  }
  // audio events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if (audio_ev  && 
      audio_ev->event_id == AmAudioEvent::noAudio) {
    DBG("########## noAudio event #########\n");
    return;
  }

  WebConferenceEvent* webconf_ev = dynamic_cast<WebConferenceEvent*>(ev);
  if (NULL != webconf_ev) {

    if (InConference != state) {

      if (webconf_ev->event_id == WebConferenceEvent::Kick) {
	DBG("########## WebConferenceEvent::Kick #########\n");
	dlg.bye(); 
	setInOut(NULL, NULL);
	setStopped();
	factory->updateStatus(conf_id, 
			      getLocalTag(), 
			      ConferenceRoomParticipant::Disconnecting,
			      "disconnect");
      }
    } else {
      
      switch(webconf_ev->event_id) {
      case WebConferenceEvent::Kick:  { 
	DBG("########## WebConferenceEvent::Kick #########\n");
	dlg.bye(); 
	disconnectConference();
	factory->updateStatus(conf_id, 
			      getLocalTag(), 
			      ConferenceRoomParticipant::Disconnecting,
			      "disconnect");
      } break;
      case WebConferenceEvent::Mute:  {  
	DBG("########## WebConferenceEvent::Mute #########\n"); 
	setInOut(NULL, &play_list);  
      } break;
      case WebConferenceEvent::Unmute: {  
	DBG("########## WebConferenceEvent::Unmute #########\n"); 
	setInOut(&play_list, &play_list);  
      } break;
      default: { WARN("ignoring unknown webconference event %d\n", webconf_ev->event_id); 
      } break;	
      }
    }
    return;
  }
  
  AmSession::process(ev);
}

void WebConferenceDialog::onDtmf(int event, int duration)
{
  DBG("WebConferenceDialog::onDtmf: event %d duration %d\n", 
      event, duration);

  if (EnteringPin == state) {
    // not yet in conference
    if (event<10) {
      pin_str += int2str(event);
      DBG("added '%s': PIN is now '%s'.\n", 
	  int2str(event).c_str(), pin_str.c_str());
    } else if (event==10 || event==11) {
      // pound and star key
      // if required add checking of pin here...
      if (!pin_str.length()) {

	prompts.addToPlaylist(WRONG_PIN, (long)this, play_list, true);
      } else {
	state = EnteringConference;
	setInOut(NULL, NULL);
	play_list.close();
	for (size_t i=0;i<pin_str.length();i++) {
	  string num = "";
	  num[0] = pin_str[i];
	  DBG("adding '%s' to playlist.\n", num.c_str());

	  prompts.addToPlaylist(num,
				(long)this, play_list);
	}

       	setInOut(&play_list,&play_list);
	prompts.addToPlaylist(ENTERING_CONFERENCE,
			      (long)this, play_list);
	play_list.addToPlaylist(new AmPlaylistItem(&separator, NULL));
      }
    }
  }
}


