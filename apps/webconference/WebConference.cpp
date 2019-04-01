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

#include "WebConference.h"
#include "AmConferenceStatus.h"
#include "AmUtils.h"
#include "log.h"
#include "AmUAC.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"
#include "AmMediaProcessor.h"

#include "WebConferenceDialog.h"

#include "sems.h" // DEFAULT_SIGNATURE

#include <stdlib.h>

#define APP_NAME "webconference"

EXPORT_SESSION_FACTORY(WebConferenceFactory,APP_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(WebConferenceFactory,APP_NAME);

WebConferenceFactory::WebConferenceFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    AmDynInvokeFactory(_app_name),
    session_timer_f(NULL),
    configured(false),
    use_direct_room(false),
    direct_room_strip(0),
    stats(NULL)
{
  if (NULL == _instance) {
    _instance = this;
  }
}

WebConferenceFactory* WebConferenceFactory::_instance=0;

string WebConferenceFactory::DigitsDir;
PlayoutType WebConferenceFactory::m_PlayoutType = ADAPTIVE_PLAYOUT;
string WebConferenceFactory::urlbase = "";

string WebConferenceFactory::MasterPassword;

int WebConferenceFactory::ParticipantExpiredDelay;
int WebConferenceFactory::RoomExpiredDelay;
int WebConferenceFactory::RoomSweepInterval;
bool WebConferenceFactory::ignore_pin = false;

bool WebConferenceFactory::PrivateRoomsMode = false;

bool WebConferenceFactory::LoopFirstParticipantPrompt = false;

unsigned int WebConferenceFactory::LonelyUserTimer = 0;

string WebConferenceFactory::participant_id_paramname; // default: param not used
string WebConferenceFactory::participant_id_hdr = "X-ParticipantID"; // default header

bool WebConferenceFactory::room_pin_split = false;
unsigned int WebConferenceFactory::room_pin_split_pos = 0;

int WebConferenceFactory::onLoad()
{
  return getInstance()->load();
}

int WebConferenceFactory::load()
{
  // only execute this once
  if (configured) 
    return 0;
  configured = true;

  if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  
  participant_id_paramname = cfg.getParameter("participant_id_param");
  if (cfg.hasParameter("participant_id_header"))
    participant_id_hdr = cfg.getParameter("participant_id_header");

  // get prompts
  AM_PROMPT_START;
  AM_PROMPT_ADD(FIRST_PARTICIPANT, WEBCONF_ANNOUNCE_PATH "first_paricipant.wav");
  AM_PROMPT_ADD(JOIN_SOUND,        WEBCONF_ANNOUNCE_PATH "beep.wav");
  AM_PROMPT_ADD(DROP_SOUND,        WEBCONF_ANNOUNCE_PATH "beep.wav");
  AM_PROMPT_ADD(ENTER_PIN,         WEBCONF_ANNOUNCE_PATH "enter_pin.wav");
  AM_PROMPT_ADD(WRONG_PIN,         WEBCONF_ANNOUNCE_PATH "wrong_pin.wav");
  AM_PROMPT_ADD(WRONG_PIN_BYE,     WEBCONF_ANNOUNCE_PATH "wrong_pin_bye.wav");
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

  string room_pin_split_s = cfg.getParameter("room_pin_split");
  if (!room_pin_split_s.empty()) {
    if (str2i(room_pin_split_s, room_pin_split_pos)) {
      ERROR("room_pin_split in webconference config not readable\n");
      return -1;
    }
    room_pin_split = true;
  } else {
    room_pin_split = false;
  }

  string feedback_filename = cfg.getParameter("feedback_file");
  if (!feedback_filename.empty()) {
    feedback_file.open(feedback_filename.c_str(), std::ios::out|std::ios::app);
    if (!feedback_file.good()) {
      WARN("opening feedback file '%s' failed\n", feedback_filename.c_str());
    } else {
      DBG("successfully opened feedback file '%s'\n", 
	    feedback_filename.c_str());
    }
  }

  string stats_dir = cfg.getParameter("stats_dir");
  if (stats_dir.empty()) 
    DBG("call statistics will not be persistent across restart.\n");
  stats = new WCCCallStats(stats_dir);

  urlbase = cfg.getParameter("webconference_urlbase");
  if (urlbase.empty())
    DBG("No urlbase set - SDP will not contain direct access URL.\n");

  MasterPassword  = cfg.getParameter("master_password");
  if (!MasterPassword.empty()) {
    DBG("Master password set.\n");
  }  

  if (cfg.getParameter("participants_expire") == "no") { 
    ParticipantExpiredDelay = -1;
  } else {
    // default: 10s
    ParticipantExpiredDelay = cfg.getParameterInt("participants_expire_delay", 10);
  }
  ignore_pin = cfg.getParameter("ignore_pin")=="yes";
  DBG("Ignore PINs  enabled: %s\n", ignore_pin?"yes":"no");


  if (cfg.getParameter("rooms_expire") == "no") { 
    RoomExpiredDelay = -1;
  } else {
    RoomExpiredDelay = cfg.getParameterInt("rooms_expire_delay", 7200); // default: 2h
  }

  // default: every 10 times
  RoomSweepInterval = cfg.getParameterInt("room_sweep_interval", 10);
 
  // seed the rng (at least a little)
  struct timeval now;
  gettimeofday(&now, NULL);    
  srandom(now.tv_usec + now.tv_sec);

  vector<string> predefined_rooms = explode(cfg.getParameter("predefined_rooms"), ";");
  for (vector<string>::iterator it =
	 predefined_rooms.begin(); it != predefined_rooms.end(); it++) {
    vector<string> room_pwd = explode(*it, ":");
    if (room_pwd.size()==2) {
      DBG("creating room '%s'\n",room_pwd[0].c_str());
      rooms[room_pwd[0]] = ConferenceRoom();
      rooms[room_pwd[0]].adminpin = room_pwd[1];
    } else {
      ERROR("wrong entry '%s' in predefined_rooms: should be <room>:<pwd>\n",
	    it->c_str());
      return -1;
    }
  }


  if (cfg.getParameter("private_rooms") == "yes") 
    PrivateRoomsMode = true;
  DBG("Private rooms mode %sabled.\n", PrivateRoomsMode ? "en":"dis");

  LoopFirstParticipantPrompt =
    cfg.getParameter("loop_first_participant_prompt") == "yes";

  LonelyUserTimer = cfg.getParameterInt("lonely_user_timer", 0);
  if (!LonelyUserTimer) {
    DBG("'lonely user' timer not used\n");
  } else {
    DBG("Timer for 'lonely user' used: %u seconds\n", LonelyUserTimer);
  }

  DBG("Looping first participant prompt: %s\n", LoopFirstParticipantPrompt ? "yes":"no");

  if (cfg.getParameter("support_rooms_timeout") == "yes") {
    cleaner = new WebConferenceCleaner(this);
    cleaner->start();
  }

  if(cfg.hasParameter("enable_session_timer") &&
     (cfg.getParameter("enable_session_timer") == string("yes")) ){
    DBG("enabling session timers\n");
    session_timer_f = AmPlugIn::instance()->getFactory4Seh("session_timer");
    if(session_timer_f == NULL){
      ERROR("Could not load the session_timer module: disabling session timers.\n");
    }
  }

  return 0;
}

bool WebConferenceFactory::isValidConference(const string& conf_id, const string& participant_id) {
  if (!PrivateRoomsMode)
    return true;

  bool res = false;
  rooms_mut.lock();
  map<string, ConferenceRoom>::iterator room = rooms.find(conf_id);
  if (room != rooms.end()) {
    if (!participant_id.size() || room->second.hasInvitedParticipant(participant_id)) {
      DBG("room '%s', participant_id '%s' -> valid\n", conf_id.c_str(), participant_id.c_str());
      res = true;
    }
  }
  rooms_mut.unlock();
  return res;
}

bool WebConferenceFactory::newParticipant(const string& conf_id, 
					  const string& localtag, 
					  const string& number,
					  const string& participant_id,
					  bool check_exisiting) {

  rooms_mut.lock();

  if (PrivateRoomsMode) {
    map<string, ConferenceRoom>::iterator room = rooms.find(conf_id);
    if (room == rooms.end()) {
      rooms_mut.unlock();
      return false;
    }
    DBG("found conference room '%s'\n", conf_id.c_str());
    if (check_exisiting && room_pin_split && !room->second.hasInvitedParticipant(participant_id)) {
      DBG("participant with ID '%s' not listed in invited participants for '%s'\n",
	  participant_id.c_str(), conf_id.c_str());
      rooms_mut.unlock();
      return false;
    }

  }

  rooms[conf_id].newParticipant(localtag, number, participant_id);
  rooms_mut.unlock();
  return true;
}

void WebConferenceFactory::updateStatus(const string& conf_id, 
					const string& localtag, 
					ConferenceRoomParticipant::ParticipantStatus status,
					const string& reason) {
  rooms_mut.lock();
  if (!PrivateRoomsMode || rooms.find(conf_id) != rooms.end()) {
    rooms[conf_id].updateStatus(localtag, status, reason);
  }
  rooms_mut.unlock();
}

string WebConferenceFactory::getAdminpin(const string& room) {
  string res = "";
  rooms_mut.lock();
  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it != rooms.end()) 
    res = it->second.adminpin;
  rooms_mut.unlock();
  return res;
}

ConferenceRoom* WebConferenceFactory::getRoom(const string& room, 
					      const string& adminpin,
					      bool ignore_adminpin = false) {
  ConferenceRoom* res = NULL;
  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it == rooms.end()) {
    if (PrivateRoomsMode)
      return NULL;

    // (re)open room
    rooms[room] = ConferenceRoom();
    rooms[room].adminpin = adminpin;   
    res = &rooms[room];
  } else {
    if ((!ignore_pin) &&
	(!ignore_adminpin) && 
	(!it->second.adminpin.empty()) && 
	(it->second.adminpin != adminpin)) {
      // wrong pin
    } else {
      // update adminpin if room was created by dialin
      if (it->second.adminpin.empty()) 
	it->second.adminpin = adminpin;
      res = &it->second;
      
      if (res->expired()) {
	DBG("clearing expired room '%s'\n", room.c_str());
	rooms.erase(it);
	res = NULL;
      }
    } 
  }

  return res;
}

string WebConferenceFactory::getAccessUri(const string& room) {
  string res = "";
  if (!WebConferenceFactory::urlbase.empty()) {
    res = WebConferenceFactory::urlbase;
    if (!room.empty()) {
      res+="&newRoomNumber="+room;
      
      string adminpin = getAdminpin(room);
      if (!adminpin.empty())
	res+="&roomAdminPassword="+adminpin;
    }
  }
  return res;
}

void WebConferenceFactory::setupSessionTimer(AmSession* s) {
  if (NULL != session_timer_f) {

    AmSessionEventHandler* h = session_timer_f->getHandler(s);
    if (NULL == h)
      return;

    if(h->configure(cfg)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      s->addHandler(h);
    }
  }
}

// incoming calls 
AmSession* WebConferenceFactory::onInvite(const AmSipRequest& req, const string& app_name,
					  const map<string,string>& app_params)
{
  if (NULL != session_timer_f) {
    if (!session_timer_f->onInvite(req, cfg))
      return NULL;
  }
  WebConferenceDialog* w;

  map<string,string>::const_iterator r_it = app_params.find("room");
  map<string,string>::const_iterator enter_room_it = app_params.find("enter_room");
  if (enter_room_it != app_params.end() && enter_room_it->second=="true") {
    // enter the room
    DBG("creating new Webconference call, room name to be entered via keypad\n");
    w = new WebConferenceDialog(prompts, getInstance(), NULL);    
  } else if (r_it != app_params.end()) {
    // use provided room name
    string room = r_it->second;
    DBG("creating new Webconference call, room name '%s'\n", room.c_str());
    w = new WebConferenceDialog(prompts, getInstance(), room);
    w->setUri(getAccessUri(room));    
  } else if (use_direct_room && !regexec(&direct_room_re, req.user.c_str(), 0,0,0)) {
    // regegex match
    string room = req.user;
    if (room.length() > direct_room_strip) 
      room = room.substr(direct_room_strip);
    DBG("direct room access match. connecting to room '%s'\n", room.c_str());

    w = new WebConferenceDialog(prompts, getInstance(), room);
    w->setUri(getAccessUri(room));
  } else {
    // enter the room
    w = new WebConferenceDialog(prompts, getInstance(), NULL);
  }

  setupSessionTimer(w);
  return w;
}

// outgoing calls 
AmSession* WebConferenceFactory::onInvite(const AmSipRequest& req, const string& app_name,
					  AmArg& session_params)
{
  UACAuthCred* cred = AmUACAuth::unpackCredentials(session_params);
  AmSession* s = new WebConferenceDialog(prompts, getInstance(), cred); 

  if (NULL == cred) {
    WARN("discarding unknown session parameters.\n");
  } else {
    AmUACAuth::enable(s);
  }

  s->setUri(getAccessUri(req.user));

  setupSessionTimer(s);

  return s;
}

void WebConferenceFactory::invoke(const string& method, 
				  const AmArg& args, 
				  AmArg& ret)
{
  

  if (method == "roomCreate"){
    args.assertArrayFmt("s");
    roomCreate(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "roomInfo"){
    args.assertArrayFmt("ss");
    roomInfo(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "roomDelete"){
    args.assertArrayFmt("ss");
    roomDelete(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "addParticipant"){
    args.assertArrayFmt("sss"); // conf_id, participant_id, number  
    roomAddParticipant(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "dialout"){
    args.assertArrayFmt("sssssss");
    dialout(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "mute"){
    args.assertArrayFmt("sss");
    mute(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "unmute"){
    args.assertArrayFmt("sss");
    unmute(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "kickout"){
    args.assertArrayFmt("sss");
    kickout(args, ret);
    ret.push(getServerInfoString().c_str());
  } else if(method == "changeRoomAdminpin"){
    args.assertArrayFmt("sss");
    changeRoomAdminpin(args, ret);
    ret.push(getServerInfoString().c_str());    
  } else if(method == "serverInfo"){
    serverInfo(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "vqRoomFeedback"){
    args.assertArrayFmt("ssi");
    vqRoomFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "vqCallFeedback"){
    args.assertArrayFmt("sssi");
    vqCallFeedback(args, ret);		
    ret.push(getServerInfoString().c_str());    
  } else if(method == "vqConferenceFeedback"){
    args.assertArrayFmt("ssssi");
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
  } else if(method == "getRoomPassword"){
    args.assertArrayFmt("ss");
    getRoomPassword(args, ret);
    ret.push(getServerInfoString().c_str());    
  } else if(method == "listRooms"){
    args.assertArrayFmt("s");
    listRooms(args, ret);
    ret.push(getServerInfoString().c_str());    
  } else if(method == "findParticipant"){
    args.assertArrayFmt("s");
    findParticipant(args, ret);
    ret.push(getServerInfoString().c_str());    
  } else if(method == "_list"){
    ret.push("roomCreate");
    ret.push("roomDelete");
    ret.push("roomInfo");
    ret.push("dialout");
    ret.push("mute");
    ret.push("unmute");
    ret.push("kickout");
    ret.push("changeRoomAdminpin");
    ret.push("serverInfo");
    ret.push("vqConferenceFeedback");
    ret.push("vqCallFeedback");
    ret.push("vqRoomFeedback");
    ret.push("getRoomPassword");
    ret.push("listRooms");
    ret.push("findParticipant");
  } else
    throw AmDynInvoke::NotImplemented(method);
}

string WebConferenceFactory::getServerInfoString() {
  string res = "Server: " 
    DEFAULT_SIGNATURE  " calls: " + 
      int2str(AmSession::getSessionNum())+
    " active";

  if (stats != NULL) {
    res += "/"+stats->getSummary();
  }
  return res;
}


string WebConferenceFactory::getRandomPin() {
  string res;
  for (int i=0;i<6;i++)
    res+=(char)('0'+random()%10);
  return res;
}

// possibly clear expired rooms. OJO: lock rooms_mut !
void WebConferenceFactory::sweepRooms() {
  if ((RoomSweepInterval>0) && (!((++room_sweep_cnt)%RoomSweepInterval))) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    map<string, ConferenceRoom>::iterator it=rooms.begin();
    while (it != rooms.end()) { 
      if (it->second.expired(now)) {
	map<string, ConferenceRoom>::iterator d_it = it;
	it++;       
	DBG("clearing expired room '%s'\n", d_it->first.c_str());
	rooms.erase(d_it);
      } else {
	it++;
      }
    }
  }
}

void WebConferenceFactory::roomCreate(const AmArg& args, AmArg& ret) {
  string room = args.get(0).asCStr();
  time_t expiry_time = 0;
  if (args.size() > 1 && (args.get(1).asInt() > 0)) {
    struct timeval now;
    gettimeofday(&now, NULL);
    expiry_time = now.tv_sec + args.get(1).asInt();
  }

  rooms_mut.lock();
  
  // sweep rooms (if necessary)
  sweepRooms();

  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it == rooms.end()) {
    rooms[room] = ConferenceRoom();
    rooms[room].adminpin = getRandomPin();
    rooms[room].expiry_time = expiry_time;
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
  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();

  rooms_mut.lock();
  ConferenceRoom* r = getRoom(room, adminpin);
  if (NULL == r) {
    ret.push(1);
    ret.push("wrong adminpin or inexisting room");
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

void WebConferenceFactory::roomDelete(const string& room, const string& adminpin,
				      AmArg& ret, bool ignore_adminpin = false) {
  rooms_mut.lock();

  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it == rooms.end()) {
    rooms_mut.unlock();
    ret.push(2);
    ret.push("room does not exist\n");
    return;
  }

  rooms_mut.unlock();   

  postAllConfEvent(room, adminpin, ret, 
		   WebConferenceEvent::Kick, ignore_adminpin);

  if (ret.get(0).asInt()==0) {
    DBG("erasing room '%s'\n", room.c_str());
    rooms_mut.lock();
    rooms.erase(room);
    rooms_mut.unlock();
  }
}

void WebConferenceFactory::roomDelete(const AmArg& args, AmArg& ret) {
  rooms_mut.lock();
  string room        = args.get(0).asCStr();
  string adminpin    = args.get(1).asCStr();

  roomDelete(room, adminpin, ret);
}

void WebConferenceFactory::closeExpiredRooms() {
  struct timeval now;
  gettimeofday(&now, NULL);

  vector<string> expired_rooms;

  rooms_mut.lock();
  for (map<string, ConferenceRoom>::iterator it = 
	 rooms.begin(); it !=rooms.end(); it++) {
    if (it->second.hard_expired(now))
      expired_rooms.push_back(it->first);
  }
  rooms_mut.unlock();

  for (vector<string>::iterator it=
	 expired_rooms.begin(); it != expired_rooms.end(); it++) {
    DBG("deleting expired room '%s'\n", it->c_str());
    AmArg ret;
    roomDelete(*it, "", ret, true);
  }
}

void WebConferenceFactory::roomAddParticipant(const AmArg& args, AmArg& ret) {
  string room                = args.get(0).asCStr();
  string participant_id      = args.get(1).asCStr();
  string number              = args.get(2).asCStr();
  if (newParticipant(room, /* ltag = */ "", number, participant_id,
		     /* check_exisiting = */ false)) {
    ret.push(200);
    ret.push("OK");
  } else {
    ret.push(400);
    ret.push("Failed"); // no info here
  }
}

void WebConferenceFactory::dialout(const AmArg& args, AmArg& ret) {
  string room        = args.get(0).asCStr();
  string adminpin    = args.get(1).asCStr();
  string callee      = args.get(2).asCStr();
  string from_user   = args.get(3).asCStr();
  string domain      = args.get(4).asCStr();
  string auth_user   = args.get(5).asCStr();
  string auth_pwd    = args.get(6).asCStr();
  string callee_domain = domain;
  string headers;
  string participant_id;

  if (args.size()>7) {
    try {
      assertArgCStr(args.get(7));
      headers = args.get(7).asCStr();
      int i, len;
      len = headers.length();
      for (i = 0; i < len; i++) {
	if (headers[i] == '|') headers[i] = '\n';
      }
      if (headers[len - 1] != '\n') {
	headers += '\n';
      }
    } catch (AmArg::TypeMismatchException &e) {
      headers = "";
    }

    if (args.size()>8) {
      try {
	assertArgCStr(args.get(8));
	callee_domain = args.get(8).asCStr();
      }
      catch (AmArg::TypeMismatchException &e) {
	callee_domain = domain;
      }
    }
    
    if (args.size()>9) {
      try {
	assertArgCStr(args.get(9));
	participant_id = args.get(9).asCStr();
      }
      catch (AmArg::TypeMismatchException &e) {
	participant_id = "";
      }
    }
  }

  string from = "sip:" + from_user + "@" + domain;
  string to   = "sip:" + callee + "@" + callee_domain;

  // check adminpin
  rooms_mut.lock();
  
  // sweep rooms (if necessary)
  sweepRooms();

  bool is_valid_room = getRoom(room, adminpin) != NULL;
  rooms_mut.unlock();

  if (!is_valid_room) {
    ret.push(1);
    ret.push("wrong adminpin or inexisting room");
    ret.push("");
    return;
  }

  DBG("dialout webconference room '%s', from '%s', to '%s'", 
      room.c_str(), from.c_str(), to.c_str());

  AmArg* a = new AmArg();
  a->setBorrowedPointer(new UACAuthCred("", auth_user, auth_pwd));

  string app_name = APP_NAME;
  string localtag = AmUAC::dialout(room.c_str(), app_name,  to,  
				   "<" + from +  ">", from, "<" + to + ">", 
				   string(""), // callid
				   headers,    // headers
				   a);
  if (!localtag.empty()) {
    ret.push(0);
    ret.push("OK");
    ret.push(localtag.c_str());
    newParticipant(room, localtag, to, participant_id);
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

void WebConferenceFactory::postAllConfEvent(const string& room, const string& adminpin, 
					    AmArg& ret, int id, bool ignore_adminpin = false) {
  vector<string> ltags;
  rooms_mut.lock(); {
    ConferenceRoom* r = getRoom(room, adminpin, ignore_adminpin);
    if (NULL == r) {
      rooms_mut.unlock();  
      return;
    }
    
    ltags = r->participantLtags();
  } rooms_mut.unlock();

  for (vector<string>::iterator it = 
	 ltags.begin(); it != ltags.end(); it++) {
    AmSessionContainer::instance()->postEvent(*it,
					      new WebConferenceEvent(id));
  }

  ret.push(0);
  ret.push("OK");
}

void WebConferenceFactory::postConfEvent(const AmArg& args, AmArg& ret,
					 int id, int mute) {
  string room        = args.get(0).asCStr();
  string adminpin    = args.get(1).asCStr();
  string call_tag    = args.get(2).asCStr();

  // check adminpin
  
  rooms_mut.lock();
  ConferenceRoom* r = getRoom(room, adminpin);
  if (NULL == r) {
    ret.push(1);
    ret.push("wrong adminpin or inexisting room");
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

void WebConferenceFactory::getRoomPassword(const AmArg& args, AmArg& ret) {

  string pwd  = args.get(0).asCStr();
  string room = args.get(1).asCStr();

  if ((!MasterPassword.length()) || 
      pwd != MasterPassword) {
    ret.push(403);
    ret.push("Wrong Master Password.");
    return;
  }
  int res_code = 404;
  string res = "Room does not exist.";
  rooms_mut.lock();
  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it != rooms.end())  {
    res = it->second.adminpin; 
    res_code = 0;
  }
  rooms_mut.unlock();

  ret.push(res_code);  
  ret.push(res.c_str());  
}

void WebConferenceFactory::changeRoomAdminpin(const AmArg& args, AmArg& ret) {
  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  string new_adminpin = args.get(2).asCStr();

  rooms_mut.lock();
  ConferenceRoom* r = getRoom(room, adminpin);
  if (NULL == r) {
    ret.push(1);
    ret.push("wrong adminpin or inexisting room");
  } else {
    r->adminpin = new_adminpin;
    ret.push(0);
    ret.push("OK");
  }
  rooms_mut.unlock();
}

void WebConferenceFactory::listRooms(const AmArg& args, AmArg& ret) {

  string pwd  = args.get(0).asCStr();

  if ((!MasterPassword.length()) || 
      pwd != MasterPassword) {
    ret.push(407);
    AmArg res;
    res.push("Wrong Master Password.");
    ret.push(res);
    return;
  }

  AmArg room_list;
  room_list.assertArray();

  rooms_mut.lock();
  for (map<string, ConferenceRoom>::iterator it = 
	 rooms.begin(); it != rooms.end(); it++) {
    if (!it->second.expired()) {
      room_list.push(it->first.c_str());
    }
  }
  rooms_mut.unlock();

  ret.push(200);  
  ret.push(room_list);  
}

void WebConferenceFactory::findParticipant(const AmArg& args, AmArg& ret) {
  string participant_id = args.get(0).asCStr();
  AmArg r_ret;
  r_ret.assertArray();
  rooms_mut.lock();
  for (map<string, ConferenceRoom>::iterator it = 
	 rooms.begin(); it != rooms.end(); it++) {
    for (list<ConferenceRoomParticipant>::iterator p_it=
	   it->second.participants.begin(); p_it != it->second.participants.end(); p_it++) {
      if (p_it->participant_id == participant_id) {
	r_ret.push(it->first);
	break;
      }
    }
  }
  rooms_mut.unlock();
  ret.push(r_ret);
}

void WebConferenceFactory::vqRoomFeedback(const AmArg& args, AmArg& ret) {  
  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  int opinion = args.get(2).asInt();

  saveFeedback(string("RO "+ room + "|||" + adminpin + "|||" + 
		      int2str(opinion) + "|||" + int2str((unsigned int)time(NULL)) + "|||\n"));

  ret.push(0);
  ret.push("OK");
}

void WebConferenceFactory::vqCallFeedback(const AmArg& args, AmArg& ret) {
  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  string tag = args.get(2).asCStr();
  int opinion = args.get(3).asInt();

  saveFeedback("CA|||"+ room + "|||" + adminpin + "|||" + tag + "|||" + 
	       int2str(opinion) + "|||" + int2str((unsigned int)time(NULL)) + "|||\n");

  ret.push(0);
  ret.push("OK");
}

void WebConferenceFactory::vqConferenceFeedback(const AmArg& args, AmArg& ret) {
  string room = args.get(0).asCStr();
  string adminpin = args.get(1).asCStr();
  string sender = args.get(2).asCStr();
  string comment = args.get(3).asCStr();
  int opinion = args.get(4).asInt();

  saveFeedback("CO|||"+ room + "|||" + adminpin + "|||" + int2str(opinion) + "|||" + 
	       sender + "|||" + comment +"|||" + int2str((unsigned int)time(NULL)) + "|||\n");

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

void WebConferenceFactory::callStats(bool success, unsigned int connect_t) {
  if (stats != NULL) {
    stats->addCall(success, connect_t);
  }
}

void WebConferenceCleaner::on_stop() {
  is_stopped.set(true);
}

void WebConferenceCleaner::run(){
  sleep(1);
  while (!is_stopped.get()) {
    factory->closeExpiredRooms();
    sleep(1);
  }
}
