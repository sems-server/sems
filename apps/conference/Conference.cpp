/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2007 Juha Heinanen (USE_MYSQL parts)
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

#include "Conference.h"
#include "AmUtils.h"
#include "AmConfigReader.h"
#include "AmConferenceStatus.h"
#include "AmConfig.h"

#include "AmSessionContainer.h"
#include "AmMediaProcessor.h"

#include "sems.h"
#include "log.h"

#ifdef USE_MYSQL
#include <mysql++/mysql++.h>
#include <stdio.h>
#define DEFAULT_AUDIO_TABLE "default_audio"
#define DOMAIN_AUDIO_TABLE "domain_audio"
#define LONELY_USER_MSG "first_participant_msg"
#define JOIN_SOUND "join_snd"
#define DROP_SOUND "drop_snd"
#endif

#define APP_NAME "conference"
	
EXPORT_SESSION_FACTORY(ConferenceFactory,APP_NAME);

ConferenceFactory::ConferenceFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

string ConferenceFactory::AudioPath;
string ConferenceFactory::LonelyUserFile;
string ConferenceFactory::JoinSound;
string ConferenceFactory::DropSound;
string ConferenceFactory::DialoutSuffix;
PlayoutType ConferenceFactory::m_PlayoutType = ADAPTIVE_PLAYOUT;
unsigned int ConferenceFactory::MaxParticipants;

#ifdef USE_MYSQL
mysqlpp::Connection ConferenceFactory::Connection(mysqlpp::use_exceptions);

int get_audio_file(string message, string domain, string language,
		   string *audio_file)
{
  string query_string;

  if (language.empty()) {
    if (domain.empty()) {
      *audio_file = string("/tmp/") + APP_NAME + "_" + message + ".wav";
      query_string = "select audio from " + string(DEFAULT_AUDIO_TABLE) + " where application='" + APP_NAME + "' and message='" + message + "' and language=''";
    } else {
      *audio_file = "/tmp/" + domain + "_" + APP_NAME + "_" + 
	message + ".wav";
      query_string = "select audio from " + string(DOMAIN_AUDIO_TABLE) + " where application='" + APP_NAME + "' and message='" + message + "' and domain='" + domain + "' and language=''";
    }
  } else {
    if (domain.empty()) {
      *audio_file = string("/tmp/") + APP_NAME + "_" + message + "_" +
	language + ".wav";
      query_string = "select audio from " + string(DEFAULT_AUDIO_TABLE) + " where application='" + APP_NAME + "' and message='" + message + "' and language='" + language + "'";
    } else {
      *audio_file = "/tmp/" + domain + "_" + APP_NAME + "_" +
	message + "_" +	language + ".wav";
      query_string = "select audio from " + string(DOMAIN_AUDIO_TABLE) + " where application='" + APP_NAME + "' and message='" + message + "' and domain='" + domain + "' and language='" + language + "'";
    }
  }

  try {

    mysqlpp::Query query = ConferenceFactory::Connection.query();
	    
    DBG("Query string <%s>\n", query_string.c_str());

    query << query_string;
    mysqlpp::Result res = query.store();

    mysqlpp::Row row;

    if (res) {
      if ((res.num_rows() > 0) && (row = res.at(0))) {
	FILE *file;
	unsigned long length = row.raw_string(0).size();
	file = fopen((*audio_file).c_str(), "wb");
	fwrite(row.at(0).data(), 1, length, file);
	fclose(file);
	return 1;
      } else {
	*audio_file = "";
	return 1;
      }
    } else {
      ERROR("Database query error\n");
      *audio_file = "";
      return 0;
    }
  }

  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    *audio_file = "";
    return 0;
  }
}

#endif

int ConferenceFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

#ifdef USE_MYSQL

  /* Get default audio from MySQL */

  string mysql_server, mysql_user, mysql_passwd, mysql_db;

  mysql_server = cfg.getParameter("mysql_server");
  if (mysql_server.empty()) {
    mysql_server = "localhost";
  }

  mysql_user = cfg.getParameter("mysql_user");
  if (mysql_user.empty()) {
    ERROR("conference.conf paramater 'mysql_user' is missing.\n");
    return -1;
  }

  mysql_passwd = cfg.getParameter("mysql_passwd");
  if (mysql_passwd.empty()) {
    ERROR("conference.conf paramater 'mysql_passwd' is missing.\n");
    return -1;
  }

  mysql_db = cfg.getParameter("mysql_db");
  if (mysql_db.empty()) {
    mysql_db = "sems";
  }

  try {

    Connection.connect(mysql_db.c_str(), mysql_server.c_str(),
		       mysql_user.c_str(), mysql_passwd.c_str());
    if (!Connection) {
      ERROR("Database connection failed: %s\n", Connection.error());
      return -1;
    }
  }
	
  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return -1;
  }

  if (!get_audio_file(LONELY_USER_MSG, "", "", &LonelyUserFile)) {
    return -1;
  }

  if (LonelyUserFile.empty()) {
    ERROR("default announce 'first_participant_msg'\n");
    ERROR("for module conference does not exist.\n");
    return -1;
  }

  if (!get_audio_file(JOIN_SOUND, "", "", &JoinSound)) {
    return -1;
  }

  if (!get_audio_file(DROP_SOUND, "", "", &DropSound)) {
    return -1;
  }

#else 

  /* Get default audio from file system */

  AudioPath = cfg.getParameter("audio_path", ANNOUNCE_PATH);

  LonelyUserFile = cfg.getParameter("default_announce");
  if (LonelyUserFile.empty()) {
    LonelyUserFile = AudioPath + "/" ANNOUNCE_FILE;
  } else {
    if (LonelyUserFile[0] != '/') {
      LonelyUserFile = AudioPath + "/" + LonelyUserFile;
    }
  }
  if(!file_exists(LonelyUserFile)){
    ERROR("default announce '%s' \n",LonelyUserFile.c_str());
    ERROR("for module conference does not exist.\n");
    return -1;
  }

  JoinSound = cfg.getParameter("join_sound");
  if (!JoinSound.empty()) {
    if (JoinSound[0] != '/') {
      JoinSound = AudioPath + "/" + JoinSound;
    }
  }

  DropSound = cfg.getParameter("drop_sound");
  if (!DropSound.empty()) {
    if (DropSound[0] != '/') {
      DropSound = AudioPath + "/" + DropSound;
    }
  }

#endif
	
  DialoutSuffix = cfg.getParameter("dialout_suffix");
  if(DialoutSuffix.empty()){
    WARN("No dialout_suffix has been configured in the conference plug-in:\n");
    WARN("\t -> dial out will not be available unless P-Dialout-Suffix\n");
    WARN("\t -> header parameter is passed to conference plug-in\n");
  }
    
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

  MaxParticipants = 0;
  string max_participants = cfg.getParameter("max_participants");
  if (max_participants.length() && str2i(max_participants, MaxParticipants)) {
    ERROR("while parsing max_participants parameter\n"); 
  }

  return 0;
}

AmSession* ConferenceFactory::onInvite(const AmSipRequest& req)
{
  if ((ConferenceFactory::MaxParticipants > 0) &&
      (AmConferenceStatus::getConferenceSize(req.user) >=
      ConferenceFactory::MaxParticipants)) {
    DBG("Conference is full.\n");
    throw AmSession::Exception(486, "Busy Here");
  }
  return new ConferenceDialog(req.user);
}

AmSession* ConferenceFactory::onRefer(const AmSipRequest& req)
{
  if(req.to_tag.empty())
    throw AmSession::Exception(488,"Not accepted here");

  AmSession* s = new ConferenceDialog(req.user);
  s->dlg.local_tag  = req.from_tag;
    

  DBG("ConferenceFactory::onRefer: local_tag = %s\n",s->dlg.local_tag.c_str());

  return s;
}

ConferenceDialog::ConferenceDialog(const string& conf_id,
				   AmConferenceChannel* dialout_channel)
  : conf_id(conf_id), 
    channel(0),
    play_list(this),
    dialout_channel(dialout_channel),
    state(CS_normal),
    allow_dialout(false)
{
  dialedout = this->dialout_channel.get() != 0;
  rtp_str.setPlayoutType(ConferenceFactory::m_PlayoutType);
}

ConferenceDialog::~ConferenceDialog()
{
  DBG("ConferenceDialog::~ConferenceDialog()\n");

  // clean playlist items
  play_list.close(false);
}

void ConferenceDialog::onStart() 
{
}

void ConferenceDialog::onSessionStart(const AmSipRequest& req)
{
  int i, len;
  string lonely_user_file;

  string app_param_hdr = getHeader(req.hdrs, PARAM_HDR);
  if (app_param_hdr.length()) {
    from_header = get_header_keyvalue(app_param_hdr, "Dialout-From");
    extra_headers = get_header_keyvalue(app_param_hdr, "Dialout-Extra");
    dialout_suffix = get_header_keyvalue(app_param_hdr, "Dialout-Suffix");      
    language = get_header_keyvalue(app_param_hdr, "Language");      
  } else {
    from_header = getHeader(req.hdrs, "P-Dialout-From");
    extra_headers = getHeader(req.hdrs, "P-Dialout-Extra");
    dialout_suffix = getHeader(req.hdrs, "P-Dialout-Suffix");
    if (from_header.length() || extra_headers.length() 
	|| dialout_suffix.length()) {
      DBG("Warning: P-Dialout- style headers are deprecated."
	  " Please use P-App-Param header instead.\n");
    }
    language = getHeader(req.hdrs, "P-Language");
    if (language.length()) {
      DBG("Warning: P-Language header is deprecated."
	  " Please use P-App-Param header instead.\n");
    }
  }

  len = extra_headers.length();
  for (i = 0; i < len; i++) {
    if (extra_headers[i] == '|') extra_headers[i] = '\n';
  }

  if (dialout_suffix.length() == 0) {
    if (!ConferenceFactory::DialoutSuffix.empty()) {
      dialout_suffix = ConferenceFactory::DialoutSuffix;
    } else {
      dialout_suffix = "";
    }
  }
    
  allow_dialout = dialout_suffix.length() > 0;

  if (!language.empty()) {

#ifdef USE_MYSQL
    /* Get domain/language specific lonely user file from MySQL */
    if (get_audio_file(LONELY_USER_MSG, req.domain, language,
		       &lonely_user_file) &&
	!lonely_user_file.empty()) {
      ConferenceFactory::LonelyUserFile = lonely_user_file;
    } else {
      if (get_audio_file(LONELY_USER_MSG, "", language,
			 &lonely_user_file) &&
	  !lonely_user_file.empty()) {
	ConferenceFactory::LonelyUserFile = lonely_user_file;
      }
    }
#else
    /* Get domain/language specific lonely user file from file system */
    lonely_user_file = ConferenceFactory::AudioPath + "/lonely_user_msg/" +
      req.domain + "/" + "default_" + language + ".wav";
    if(file_exists(lonely_user_file)) {
      ConferenceFactory::LonelyUserFile = lonely_user_file;
    } else {
      lonely_user_file = ConferenceFactory::AudioPath +
	"/lonely_user_msg/default_" + language + ".wav";
      if(file_exists(lonely_user_file)) {
	ConferenceFactory::LonelyUserFile = lonely_user_file;
      }
    }
#endif
  }

  DBG("Using LonelyUserFile <%s>\n",
      ConferenceFactory::LonelyUserFile.c_str());
	
  setupAudio();
}

// void ConferenceDialog::onSessionStart(const AmSipReply& reply)
// {
//     setupAudio();
// }

void ConferenceDialog::setupAudio()
{
  if(!ConferenceFactory::JoinSound.empty()) {
	
    JoinSound.reset(new AmAudioFile());
    if(JoinSound->open(ConferenceFactory::JoinSound,
		       AmAudioFile::Read))
      JoinSound.reset(0);
  }

  if(!ConferenceFactory::DropSound.empty()) {
	
    DropSound.reset(new AmAudioFile());
    if(DropSound->open(ConferenceFactory::DropSound,
		       AmAudioFile::Read))
      DropSound.reset(0);
  }


  play_list.close();// !!!

  if(dialout_channel.get()){

    DBG("adding dialout_channel to the playlist (dialedout = %i)\n",dialedout);
    play_list.addToPlaylist(new AmPlaylistItem(dialout_channel.get(),
					       dialout_channel.get()));
  }
  else {

    channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag()));

    play_list.addToPlaylist(new AmPlaylistItem(channel.get(),
					       channel.get()));
  }

  setInOut(&play_list,&play_list);
    
  setCallgroup(conf_id);
	
  if(dialedout || !allow_dialout) {
    DBG("Dialout not enabled or dialout channel. Disabling DTMF detection.\n");
    setDtmfDetectionEnabled(false);
  }
}

void ConferenceDialog::onBye(const AmSipRequest& req)
{
  if(dialout_channel.get())
    disconnectDialout();

  closeChannels();
  setStopped();
}

void ConferenceDialog::process(AmEvent* ev)
{
  ConferenceEvent* ce = dynamic_cast<ConferenceEvent*>(ev);
  if(ce && (conf_id == ce->conf_id)){
    switch(ce->event_id){
    case ConfNewParticipant:

      DBG("########## new participant #########\n");
      if((ce->participants == 1) && 
	 !ConferenceFactory::LonelyUserFile.empty() ){

	if(!LonelyUserFile.get()){
			
	  LonelyUserFile.reset(new AmAudioFile());
	  if(LonelyUserFile->open(ConferenceFactory::LonelyUserFile,
				  AmAudioFile::Read))
	    LonelyUserFile.reset(0);
	}
		
	if(LonelyUserFile.get())
	  play_list.addToPlayListFront(
				       new AmPlaylistItem( LonelyUserFile.get(), NULL ));
      }
      else {
		
	if(JoinSound.get()){
	  JoinSound->rewind();
	  play_list.addToPlayListFront(
				       new AmPlaylistItem( JoinSound.get(), NULL ));
	}
      }
		
      break;
    case ConfParticipantLeft:
      DBG("########## participant left the room #########\n");
      if(DropSound.get()){
	DropSound->rewind();
	play_list.addToPlayListFront(
				     new AmPlaylistItem( DropSound.get(), NULL ));
      }
      break;
    default:
      break;
    }
    return;
  }

  DialoutConfEvent* do_ev = dynamic_cast<DialoutConfEvent*>(ev);
  if(do_ev){

    if(dialedout){

      switch(do_ev->event_id){

      case DoConfConnect:

	connectMainChannel();
	break;
	
      case DoConfDisconnect:
		
	dlg.bye();
	closeChannels();
	setStopped();
	break;

      default:
	break;
      }
    }
    else {
	    
      switch(do_ev->event_id){

      case DoConfDisconnect:

	DBG("****** Caller received DoConfDisconnect *******\n");
	connectMainChannel();
	state = CS_normal;
	break;

      case DoConfConnect:

	state = CS_dialout_connected;

	play_list.close(); // !!!
	play_list.addToPlaylist(new AmPlaylistItem(dialout_channel.get(),
						   dialout_channel.get()));
	break;

      case DoConfRinging:
		
	if(!RingTone.get())
	  RingTone.reset(new AmRingTone(0,2000,4000,440,480)); // US

	DBG("adding ring tone to the playlist (dialedout = %i)\n",dialedout);
	play_list.close();
	play_list.addToPlaylist(new AmPlaylistItem(RingTone.get(),NULL));
	break;

      case DoConfError:
		
	DBG("****** Caller received DoConfError *******\n");
	if(!ErrorTone.get())
	  ErrorTone.reset(new AmRingTone(2000,250,250,440,480));

	DBG("adding error tone to the playlist (dialedout = %i)\n",dialedout);
	//play_list.close();
	play_list.addToPlayListFront(new AmPlaylistItem(ErrorTone.get(),NULL));
	break;
		
      }
    }

    return;
  }

  AmSession::process(ev);
}

string dtmf2str(int event)
{
  switch(event){
  case 0: case 1: case 2:
  case 3: case 4: case 5:
  case 6: case 7: case 8:
  case 9:
    return int2str(event);
	
  case 10: return "*";
  case 11: return "#";
  default: return "";
  }
}


void ConferenceDialog::onDtmf(int event, int duration)
{
  DBG("ConferenceDialog::onDtmf\n");
  if (dialedout || !allow_dialout ||
      ((ConferenceFactory::MaxParticipants > 0) &&
       (AmConferenceStatus::getConferenceSize(dlg.user) >= 
	ConferenceFactory::MaxParticipants)))
    return;

  switch(state){
	
  case CS_normal:
    DBG("CS_normal\n");
    dtmf_seq += dtmf2str(event);

    if(dtmf_seq.length() == 2){
      if(dtmf_seq == "#*") {
	state = CS_dialing_out;
	dtmf_seq = "";
      } else {
	// keep last digit
	dtmf_seq = dtmf_seq[1]; 
      }
    }
    break;

  case CS_dialing_out:{
    DBG("CS_dialing_out\n");
    string digit = dtmf2str(event);

    if(digit == "*"){
	    
      if(!dtmf_seq.empty()){
	createDialoutParticipant(dtmf_seq);
	state = CS_dialed_out;
      }
      else {
	DBG("state = CS_normal; ????????\n");
	state = CS_normal;
      }

      dtmf_seq = "";
    }
    else 
      dtmf_seq += digit;

  } break;


  case CS_dialout_connected:
    DBG("CS_dialout_connected\n");
    if(event == 10){ // '*'

      AmSessionContainer::instance()
	->postEvent(dialout_id,
		    new DialoutConfEvent(DoConfConnect,
					 getLocalTag()));

      connectMainChannel();
      state = CS_normal;
    }

  case CS_dialed_out:
    DBG("CS_dialed_out\n");
    if(event == 11){ // '#'
      disconnectDialout();
      state = CS_normal;
    }
    break;
	
  }
}

void ConferenceDialog::createDialoutParticipant(const string& uri_user)
{
  string uri;

  uri = "sip:" + uri_user + dialout_suffix;

  dialout_channel.reset(AmConferenceStatus::getChannel(getLocalTag(),getLocalTag()));

  dialout_id = AmSession::getNewId();
    
  ConferenceDialog* dialout_session = 
    new ConferenceDialog(conf_id,
			 AmConferenceStatus::getChannel(getLocalTag(),
							dialout_id));

  AmSipDialog& dialout_dlg = dialout_session->dlg;

  dialout_dlg.local_tag    = dialout_id;
  dialout_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;

  if (from_header.length() > 0) {
    dialout_dlg.local_party  = from_header;
  } else {
    dialout_dlg.local_party  = dlg.local_party;
  }
  dialout_dlg.remote_party = uri;
  dialout_dlg.remote_uri   = uri;

  string body;
  int local_port = dialout_session->rtp_str.getLocalPort();
  dialout_session->sdp.genRequest(AmConfig::LocalIP,local_port,body);

  if (extra_headers.length() == 0) {
    extra_headers = "";
  }

  dialout_dlg.sendRequest("INVITE","application/sdp",body,extra_headers);

  dialout_session->start();

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(dialout_id,dialout_session);
}

void ConferenceDialog::disconnectDialout()
{
  if(dialedout){
	
    if(dialout_channel.get()){
	    
      AmSessionContainer::instance()
	->postEvent(dialout_channel->getConfID(),
		    new DialoutConfEvent(DoConfDisconnect,
					 dialout_channel->getConfID()));
    }
  }
  else {

    AmSessionContainer::instance()
      ->postEvent(dialout_id,
		  new DialoutConfEvent(DoConfDisconnect,
				       getLocalTag()));
    
    connectMainChannel();
  }
}

void ConferenceDialog::connectMainChannel()
{
  dialout_id = "";
  dialedout = false;
  dialout_channel.reset(NULL);
    
  play_list.close();

  if(!channel.get())
    channel.reset(AmConferenceStatus
		  ::getChannel(conf_id,
			       getLocalTag()));

  play_list.addToPlaylist(new AmPlaylistItem(channel.get(),
					     channel.get()));
}

void ConferenceDialog::closeChannels()
{
  play_list.close();
  setInOut(NULL,NULL);
  channel.reset(NULL);
  dialout_channel.reset(NULL);
}

void ConferenceDialog::onSipRequest(const AmSipRequest& req)
{
  AmSession::onSipRequest(req);
  if((dlg.getStatus() >= AmSipDialog::Connected) ||
     (req.method != "REFER"))
    return;

  std::swap(dlg.local_party,dlg.remote_party);
  dlg.remote_tag = "";

  // get route set and next hop
  string iptel_app_param = getHeader(req.hdrs, PARAM_HDR);
  if (iptel_app_param.length()) {
    dlg.setRoute(get_header_keyvalue(iptel_app_param,"Transfer-RR"));
    dlg.next_hop = get_header_keyvalue(iptel_app_param,"Transfer-NH");
  } else {
    INFO("Use of P-Transfer-RR/P-Transfer-NH is deprecated. "
	 "Use '%s: Transfer-RR=<rr>;Transfer-NH=<nh>' instead.\n",PARAM_HDR);

    dlg.setRoute(getHeader(req.hdrs,"P-Transfer-RR"));
    dlg.next_hop = getHeader(req.hdrs,"P-Transfer-NH");
  }

  DBG("ConferenceDialog::onSipRequest: local_party = %s\n",dlg.local_party.c_str());
  DBG("ConferenceDialog::onSipRequest: local_tag = %s\n",dlg.local_tag.c_str());
  DBG("ConferenceDialog::onSipRequest: remote_party = %s\n",dlg.remote_party.c_str());
  DBG("ConferenceDialog::onSipRequest: remote_tag = %s\n",dlg.remote_tag.c_str());

  string body;
  int local_port = rtp_str.getLocalPort();
  sdp.genRequest(AmConfig::LocalIP,local_port,body);
  dlg.sendRequest("INVITE","application/sdp",body,"");

  transfer_req.reset(new AmSipRequest(req));

  return;
}

void ConferenceDialog::onSipReply(const AmSipReply& reply)
{
  int status = dlg.getStatus();
  AmSession::onSipReply(reply);

  DBG("ConferenceDialog::onSipReply: code = %i, reason = %s\n, status = %i\n",
      reply.code,reply.reason.c_str(),dlg.getStatus());
    
  if(!dialedout && 
     !transfer_req.get())
    return;

  if(status < AmSipDialog::Connected){

    switch(dlg.getStatus()){

    case AmSipDialog::Connected:

      // connected!
      try {

	acceptAudio(reply.body,reply.hdrs);

	if(getDetached() && !getStopped()){
		    
	  setupAudio();
		    
	  if(getInput() || getOutput())
	    AmMediaProcessor::instance()->addSession(this,
						     getCallgroup()); 
	  else { 
	    ERROR("missing audio input and/or ouput.\n");
	    return;
	  }

	  if(!transfer_req.get()){

	    // send connect event
	    AmSessionContainer::instance()
	      ->postEvent(dialout_channel->getConfID(),
			  new DialoutConfEvent(DoConfConnect,
					       dialout_channel->getConfID()));
	  }
	  else {
	    dlg.reply(*(transfer_req.get()),202,"Accepted");
	    transfer_req.reset(0);
	    connectMainChannel();
	  }
	} 
	
      }
      catch(const AmSession::Exception& e){
	ERROR("%i %s\n",e.code,e.reason.c_str());
	dlg.bye();
	setStopped();
      }
      break;

    case AmSipDialog::Pending:

      switch(reply.code){
      case 180:

	// send ringing event
	AmSessionContainer::instance()
	  ->postEvent(dialout_channel->getConfID(),
		      new DialoutConfEvent(DoConfRinging,
					   dialout_channel->getConfID()));
		
	break;
      case 183: break;//TODO: remote ring tone.
      default:  break;// continue waiting.
      }
      break;

    case AmSipDialog::Disconnected:

      if(!transfer_req.get()){

	disconnectDialout();
	//switch(reply.code){
	//default:
	    
	AmSessionContainer::instance()
	  ->postEvent(dialout_channel->getConfID(),
		      new DialoutConfEvent(DoConfError,
					   dialout_channel->getConfID()));
	//}
      }
      else {
		
	dlg.reply(*(transfer_req.get()),reply.code,reply.reason);
	transfer_req.reset(0);
	setStopped();
      }
      break;

	    

    default: break;
    }


  }
}

