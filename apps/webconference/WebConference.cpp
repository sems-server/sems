/*
 * $Id$
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
    feedback_file.open(feedback_filename.c_str(), std::ios::out|std::ios::app);
    if (!feedback_file.good()) {
      ERROR("opening feedback file '%s'\n", 
	    feedback_filename.c_str());
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
					      const string& adminpin) {
  ConferenceRoom* res = NULL;
  map<string, ConferenceRoom>::iterator it = rooms.find(room);
  if (it == rooms.end()) {
    // (re)open room
    rooms[room] = ConferenceRoom();
    rooms[room].adminpin = adminpin;   
    res = &rooms[room];
  } else {
    if ((!ignore_pin) &&
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

// incoming calls 
AmSession* WebConferenceFactory::onInvite(const AmSipRequest& req)
{
  if (use_direct_room) {
    if (!regexec(&direct_room_re, req.user.c_str(), 0,0,0)) {
      string room = req.user;
      if (room.length() > direct_room_strip) 
 	room = room.substr(direct_room_strip);
      DBG("direct room access match. connecting to room '%s'\n", 
	  room.c_str());
      WebConferenceDialog* w = 
	new WebConferenceDialog(prompts, getInstance(), room);

      w->setUri(getAccessUri(room));
      return w;
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

  s->setUri(getAccessUri(req.user));

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
  } else if(method == "changeRoomAdminpin"){
    args.assertArrayFmt("sss");
    changeRoomAdminpin(args, ret);
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
  } else if(method == "getRoomPassword"){
    args.assertArrayFmt("ss");
    getRoomPassword(args, ret);
    ret.push(getServerInfoString().c_str());    
  } else if(method == "listRooms"){
    args.assertArrayFmt("s");
    listRooms(args, ret);
    ret.push(getServerInfoString().c_str());    
  } else if(method == "_list"){
    ret.push("roomCreate");
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
  assertArgCStr(args.get(0));
  string room = args.get(0).asCStr();
  rooms_mut.lock();
  
  // sweep rooms (if necessary)
  sweepRooms();

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
  string adminpin = args.get(1).asCStr();

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
    assertArgCStr(args.get(i));

  string room        = args.get(0).asCStr();
  string adminpin    = args.get(1).asCStr();
  string callee      = args.get(2).asCStr();
  string from_user   = args.get(3).asCStr();
  string domain      = args.get(4).asCStr();
  string auth_user   = args.get(5).asCStr();
  string auth_pwd    = args.get(6).asCStr();
  string callee_domain;
  string headers;

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
  }
  catch (AmArg::OutOfBoundsException &e) {
    headers = "";
  }

  try {
    assertArgCStr(args.get(8));
    callee_domain = args.get(8).asCStr();
  }
  catch (AmArg::OutOfBoundsException &e) {
    callee_domain = domain;
  }

  string from = "sip:" + from_user + "@" + domain;
  string to   = "sip:" + callee + "@" + callee_domain;

  // check adminpin
  rooms_mut.lock();
  
  // sweep rooms (if necessary)
  sweepRooms();

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
				string(""), // callid
				headers, // headers
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

void WebConferenceFactory::getRoomPassword(const AmArg& args, AmArg& ret) {

  string pwd  = args.get(0).asCStr();
  string room = args.get(1).asCStr();

  if ((!MasterPassword.length()) || 
      pwd != MasterPassword) {
    ret.push(403);
    ret.push("Wrong Master Password.\n");
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
    ret.push("wrong adminpin");
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
    res.push("Wrong Master Password.\n");
    ret.push(res);
    return;
  }

  AmArg room_list;
  
  rooms_mut.lock();
  for (map<string, ConferenceRoom>::iterator it = 
	 rooms.begin(); it != rooms.end(); it++) {
    room_list.push(it->first.c_str());
  }
  rooms_mut.unlock();

  ret.push(200);  
  ret.push(room_list);  
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

void WebConferenceFactory::callStats(bool success, unsigned int connect_t) {
  if (stats != NULL) {
    stats->addCall(success, connect_t);
  }
}

