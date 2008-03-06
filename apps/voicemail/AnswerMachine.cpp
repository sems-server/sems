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

#include "AnswerMachine.h"
#include "AmApi.h"
#include "AmSession.h"
#include "AmConfig.h"
#include "AmMail.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmPlaylist.h"

#include "../msg_storage/MsgStorageAPI.h"
#include "sems.h"
#include "log.h"

#ifdef USE_MYSQL
#include <mysql++/mysql++.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#define DEFAULT_TEMPLATE_TABLE "default_template"
#define DOMAIN_TEMPLATE_TABLE "domain_template"
#define DEFAULT_AUDIO_TABLE "default_audio"
#define DOMAIN_AUDIO_TABLE "domain_audio"
#define USER_AUDIO_TABLE "user_audio"
#define GREETING_MSG "greeting_msg"
#define BEEP_SOUND "beep_snd"
#define EMAIL_TMPL "email_tmpl"
#endif

#include <unistd.h>
#include <dirent.h>

#include <time.h>

#define MOD_NAME               "voicemail"
#define DEFAULT_AUDIO_EXT      "wav"

#define DEFAULT_MAIL_TMPL_PATH  string("/usr/local/etc/sems")
#define DEFAULT_MAIL_TMPL       string("default")
#define DEFAULT_MAIL_TMPL_EXT   string("template")

#define RECORD_TIMER 99

EXPORT_SESSION_FACTORY(AnswerMachineFactory,MOD_NAME);

AnswerMachineFactory::AnswerMachineFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{}

string AnswerMachineFactory::RecFileExt;
string AnswerMachineFactory::AnnouncePath;
string AnswerMachineFactory::DefaultAnnounce;
int    AnswerMachineFactory::MaxRecordTime;
AmDynInvokeFactory* AnswerMachineFactory::UserTimer=0;
AmDynInvokeFactory* AnswerMachineFactory::MessageStorage=0;
bool AnswerMachineFactory::SaveEmptyMsg = true;


string       AnswerMachineFactory::SmtpServerAddress       = SMTP_ADDRESS_IP;
unsigned int AnswerMachineFactory::SmtpServerPort          = SMTP_PORT;

#ifdef USE_MYSQL
mysqlpp::Connection AnswerMachineFactory::Connection(mysqlpp::use_exceptions);

int get_audio_file(string message, string domain, string user,
                  string language, string *audio_file)
{
  string query_string;

  if (!user.empty()) {
    *audio_file = string("/tmp/") + domain + "_" + user + "_" +
      MOD_NAME + "_" + message + ".wav";
    query_string = "select audio from " + string(USER_AUDIO_TABLE) + " where application='" + MOD_NAME + "' and message='" + message + "' and domain='" + domain + "' and userid='" + user + "'";
  } else {
    if (language.empty()) {
      if (domain.empty()) {
       *audio_file = string("/tmp/") + MOD_NAME + "_" + message +
         ".wav";
       query_string = "select audio from " + string(DEFAULT_AUDIO_TABLE) + " where application='" + MOD_NAME + "' and message='" + message + "' and language=''";
      } else {
       *audio_file = string("/tmp/") + domain + "_" + MOD_NAME +
         "_" + message + ".wav";
       query_string = "select audio from " + string(DOMAIN_AUDIO_TABLE) + " where application='" + MOD_NAME + "' and message='" + message + "' and domain='" + domain + "' and language=''";
      }
    } else {
      if (domain.empty()) {
       *audio_file = string("/tmp/") + MOD_NAME + "_" + message +
         "_" + language + ".wav";
       query_string = "select audio from " + string(DEFAULT_AUDIO_TABLE) + " where application='" + MOD_NAME + "' and message='" + message + "' and language='" + language + "'";
      } else {
       *audio_file = string("/tmp/") + domain + "_" + MOD_NAME + "_" +
         message + "_" + language + ".wav";
       query_string = "select audio from " + string(DOMAIN_AUDIO_TABLE) + " where application='" + MOD_NAME + "' and message='" + message + "' and domain='" + domain + "' and language='" + language + "'";
      }
    }
  }

  try {

    mysqlpp::Query query = AnswerMachineFactory::Connection.query();

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

int AnswerMachineFactory::loadEmailTemplatesFromMySQL()
{

  try {

    mysqlpp::Query query = AnswerMachineFactory::Connection.query();

    string query_string, table;
    query_string = "select replace(template, '\r', '') as template, language from " + string(DEFAULT_TEMPLATE_TABLE) + " where application='" + MOD_NAME + "' and message='" + EMAIL_TMPL + "'";

    DBG("Query string <%s>\n", query_string.c_str());

    query << query_string;
    mysqlpp::Result res = query.store();

    mysqlpp::Row::size_type i;
    mysqlpp::Row::size_type row_count = res.num_rows();
    mysqlpp::Row row;

    for (i = 0; i < row_count; i++) {
      row = res.at(i);
      FILE *file;
      unsigned long length = row["template"].size();
      string tmp_file, tmpl_name;
      row = res.at(i);
      if (string(row["language"]) == "") {
       tmp_file = "/tmp/voicemail_email.template";
       tmpl_name = DEFAULT_MAIL_TMPL;
      } else {
       tmp_file = string("/tmp/voicemail_email_") +
         string(row["language"]) + ".template";
       tmpl_name = DEFAULT_MAIL_TMPL + "_" +
         string(row["language"]);
      }
      file = fopen(tmp_file.c_str(), "wb");
      fwrite(row["template"], 1, length, file);
      fclose(file);
      DBG("loading %s as %s ...\n", tmp_file.c_str(), tmpl_name.c_str());
      EmailTemplate tmp_tmpl;
      if (tmp_tmpl.load(tmp_file)) {
       ERROR("Voicemail: could not load default"
             " email template: '%s'\n", tmp_file.c_str());
       return -1;
      } else {
       email_tmpl[tmpl_name] = tmp_tmpl;
      }
    }
    if (email_tmpl.count(DEFAULT_MAIL_TMPL) == 0) {
      ERROR("Voicemail: default email template does not exist\n");
      return -1;
    }

    query_string = "select domain, replace(template, '\r', '') as template, language from " + string(DOMAIN_TEMPLATE_TABLE) + " where application='" + MOD_NAME +"' and message='" + EMAIL_TMPL + "'";

    DBG("Query string <%s>\n", query_string.c_str());

    query << query_string;
    res = query.store();

    row_count = res.num_rows();

    for (i = 0; i < row_count; i++) {
      row = res.at(i);
      FILE *file;
      unsigned long length = row["template"].size();
      string tmp_file, tmpl_name;
      row = res.at(i);
      if (string(row["language"]) == "") {
       tmp_file = "/tmp/" + string(row["domain"]) +
         "_voicemail_email.template";
       tmpl_name = string(row["domain"]);
      } else {
       tmp_file = string("/tmp/") + string(row["domain"]) +
         "_voicemail_email_" + string(row["language"]) +
         ".template";
       tmpl_name = string(row["domain"]) + "_" +
         string(row["language"]);
      }
      file = fopen(tmp_file.c_str(), "wb");
      fwrite(row["template"], 1, length, file);
      fclose(file);
      DBG("loading %s as %s ...\n",tmp_file.c_str(), tmpl_name.c_str());
      EmailTemplate tmp_tmpl;
      if (tmp_tmpl.load(tmp_file) < 0) {
       ERROR("Voicemail: could not load default"
             " email template: '%s'\n", tmp_file.c_str());
       return -1;
      } else {
       email_tmpl[tmpl_name] = tmp_tmpl;
      }
    }
  }

  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return -1;
  }

  return 0;
}

#else


int AnswerMachineFactory::loadEmailTemplates(const string& path)
{
  string email_tmpl_file = add2path(path, 1, 
				    (DEFAULT_MAIL_TMPL + "."
				     + DEFAULT_MAIL_TMPL_EXT).c_str());
    
  EmailTemplate& tmpl = email_tmpl[DEFAULT_MAIL_TMPL];
  if(tmpl.load(email_tmpl_file)){
    ERROR("Voicemail: could not load default"
	  " email template: '%s'\n",
	  email_tmpl_file.c_str());
    return -1;
  }

  int err=0;
  struct dirent* entry=0;
  DIR* dir = opendir(path.c_str());

  if(!dir){
    ERROR("Voicemail: email template loader (%s): %s\n",
	  path.c_str(),strerror(errno));
    return -1;
  }

  string file_ext = string(".") + DEFAULT_MAIL_TMPL_EXT;
  while( ((entry = readdir(dir)) != NULL) && (err == 0) ){

    string tmpl_file = add2path(path,1,entry->d_name);
    string tmpl_name = entry->d_name;

    if( (tmpl_name.length() <= file_ext.length()) 
	|| tmpl_name.substr( tmpl_name.length()
			     - file_ext.length(),
			     file_ext.length() )
	!= file_ext ){
      continue;
    }
    tmpl_name = tmpl_name.substr( 0, tmpl_name.length()
				  - file_ext.length() );

    DBG("loading %s ...\n",tmpl_file.c_str());
    EmailTemplate tmp_tmpl;
    if( (err = tmp_tmpl.load(tmpl_file)) < 0 )
      ERROR("Voicemail: while loading template '%s'\n",tmpl_file.c_str());
    else
      email_tmpl[tmpl_name] = tmp_tmpl;
  }

  closedir(dir);
  return err;
}

#endif

int AnswerMachineFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1, MOD_NAME ".conf")))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  // smtp_server
  SmtpServerAddress = cfg.getParameter("smtp_server",SmtpServerAddress);

  // smtp_port
  if(cfg.hasParameter("smtp_port")){
    if(sscanf(cfg.getParameter("smtp_port").c_str(),
             "%u",&SmtpServerPort) != 1) {
      ERROR("invalid smtp_port specified\n");
      return -1;
    }
  }

  DBG("SMTP server set to %s:%u\n",
      SmtpServerAddress.c_str(), SmtpServerPort);

#ifdef USE_MYSQL

  /* Get email templates from MySQL */

  string mysql_server, mysql_user, mysql_passwd, mysql_db;

  mysql_server = cfg.getParameter("mysql_server");
  if (mysql_server.empty()) {
    mysql_server = "localhost";
  }

  mysql_user = cfg.getParameter("mysql_user");
  if (mysql_user.empty()) {
    ERROR("voicemail.conf paramater 'mysql_user' is missing.\n");
    return -1;
  }

  mysql_passwd = cfg.getParameter("mysql_passwd");
  if (mysql_passwd.empty()) {
    ERROR("voicemail.conf paramater 'mysql_passwd' is missing.\n");
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
    Connection.set_option(mysqlpp::Connection::opt_reconnect, true);
  }

  catch (const mysqlpp::Exception& er) {
    // Catch-all for any MySQL++ exceptions
    ERROR("MySQL++ error: %s\n", er.what());
    return -1;
  }

  if(loadEmailTemplatesFromMySQL()){
    ERROR("while loading email templates from MySQL\n");
    return -1;
  }

#else

  /* Get email templates from file system */

  if(loadEmailTemplates(cfg.getParameter("email_template_path",DEFAULT_MAIL_TMPL_PATH))){
    ERROR("while loading email templates\n");
    return -1;
  }

  AnnouncePath    = cfg.getParameter("announce_path");
  DefaultAnnounce = cfg.getParameter("default_announce");

#endif

  MaxRecordTime   = cfg.getParameterInt("max_record_time",DEFAULT_RECORD_TIME);
  RecFileExt      = cfg.getParameter("rec_file_ext",DEFAULT_AUDIO_EXT);

  UserTimer = AmPlugIn::instance()->getFactory4Di("user_timer");
  if(!UserTimer){
	
    ERROR("could not load user_timer from session_timer plug-in\n");
    return -1;
  }

  MessageStorage = NULL;
  MessageStorage = AmPlugIn::instance()->getFactory4Di("msg_storage");
  if(NULL == MessageStorage){
    INFO("could not load msg_storage. Voice Box mode will not be available.\n");
  }

  DBG("Starting SMTP daemon\n");
  AmMailDeamon::instance()->start();
  
  string s_save_empty_msg = cfg.getParameter("box_save_empty_msg");
  if (!s_save_empty_msg.empty()) {
    SaveEmptyMsg = !(s_save_empty_msg == "no");
  }
  DBG("Voicebox will%s save empty messages.\n", 
      SaveEmptyMsg?"":" not");

  return 0;
}

AmSession* AnswerMachineFactory::onInvite(const AmSipRequest& req)
{
  string language;
  string email;
  string domain;
  string user;
  string sender;

  int vm_mode = MODE_VOICEMAIL; 

  string iptel_app_param = getHeader(req.hdrs, PARAM_HDR);

  if (!iptel_app_param.length()) {
    AmSession::Exception(500, "voicemail: parameters not found");
  }

  language = get_header_keyvalue(iptel_app_param,"Language");
  email = get_header_keyvalue(iptel_app_param,"Email-Address");
  string mode = get_header_keyvalue(iptel_app_param,"Mode");
  if (!mode.empty()) {
    if (mode == "box")
      vm_mode = MODE_BOX;
    else if (mode == "both")
      vm_mode = MODE_BOTH;
  }

  user = get_header_keyvalue(iptel_app_param,"User");
  sender = get_header_keyvalue(iptel_app_param,"Sender");
  domain = get_header_keyvalue(iptel_app_param,"Domain");

  // checks
  if (user.empty()) 
    throw AmSession::Exception(500, "voicemail: user missing");

  if (sender.empty()) 
    throw AmSession::Exception(500, "voicemail: sender missing");

  if (((vm_mode == MODE_BOX) || (vm_mode == MODE_BOTH))
      && (NULL == MessageStorage)) {
    throw AmSession::Exception(500, "voicemail: no message storage available");
  }
    

  DBG("voicemail invocation parameters: \n");
  DBG(" Mode:     <%s> \n", mode.c_str());
  DBG(" User:     <%s> \n", user.c_str());
  DBG(" Sender:   <%s> \n", sender.c_str());
  DBG(" Domain:   <%s> \n", domain.c_str());
  DBG(" Language: <%s> \n", language.c_str());

#ifdef USE_MYSQL

  string announce_file;

  if (get_audio_file(GREETING_MSG, domain, req.user, "",
                    &announce_file) && !announce_file.empty())
    goto announce_found;

  if (!language.empty()) {
    if (get_audio_file(GREETING_MSG, domain, "", language,
                      &announce_file) && !announce_file.empty())
      goto announce_found;
  } else {
    if (get_audio_file(GREETING_MSG, domain, "", "",
                      &announce_file) && !announce_file.empty())
      goto announce_found;
  }

  if (!language.empty())
    if (get_audio_file(GREETING_MSG, "", "", language,
                      &announce_file) && !announce_file.empty())
      goto announce_found;

  get_audio_file(GREETING_MSG, "", "", "", &announce_file);

#else

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

#endif

 announce_found:
  if(announce_file.empty())
    throw AmSession::Exception(500,"voicemail: no greeting file found");
    
  // VBOX mode does not need email template
  if (vm_mode == MODE_BOX) 
    return new AnswerMachineDialog(user, sender, domain,
				   email, announce_file, 
				   vm_mode, NULL);
				    
  if(email.empty())
    throw AmSession::Exception(404,"missing email address");

  map<string,EmailTemplate>::iterator tmpl_it;
  if (!language.empty()) {
    tmpl_it = email_tmpl.find(domain + "_" + language);
    if(tmpl_it == email_tmpl.end()) {
      tmpl_it = email_tmpl.find(domain);
      if(tmpl_it == email_tmpl.end()) {
	tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL + "_"
				  + language);
	if(tmpl_it == email_tmpl.end())
	  tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL);
      }
    }
  } else {
    tmpl_it = email_tmpl.find(domain);
    if(tmpl_it == email_tmpl.end())
      tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL);
  }
    
  if(tmpl_it == email_tmpl.end()){
    ERROR("Voicemail: unable to find an email template.\n");
    return 0;
  }
  return new AnswerMachineDialog(user, sender, domain,
				 email, announce_file,
				 vm_mode, &tmpl_it->second);
}


AnswerMachineDialog::AnswerMachineDialog(const string& user, 
					 const string& sender, 
					 const string& domain,
					 const string& email, 
					 const string& announce_file, 
					 int vm_mode,
					 const EmailTemplate* tmpl) 
  : announce_file(announce_file), 
    tmpl(tmpl), playlist(this), 
    status(0), vm_mode(vm_mode)
{
  email_dict["user"] = user;
  email_dict["sender"] = sender;
  email_dict["domain"] = domain;
  email_dict["email"] = email;

  user_timer = AnswerMachineFactory::UserTimer->getInstance();
  if(!user_timer){
    ERROR("could not get a user timer reference\n");
    throw AmSession::Exception(500,"could not get a user timer reference");
  }

  if (vm_mode == MODE_BOTH || vm_mode == MODE_BOX) {
    msg_storage = AnswerMachineFactory::MessageStorage->getInstance();
    if(!msg_storage){
      ERROR("could not get a message storage reference\n");
      throw AmSession::Exception(500,"could not get a message storage reference");
    }
  }
}

AnswerMachineDialog::~AnswerMachineDialog()
{
  playlist.close(false);
}

void AnswerMachineDialog::process(AmEvent* event)
{
  AmAudioEvent* ae = dynamic_cast<AmAudioEvent*>(event);
  if(ae){

    switch(ae->event_id){

    case AmAudioEvent::noAudio:

      switch(status){

      case 0:
	playlist.addToPlaylist(new AmPlaylistItem(NULL,&a_msg));
		
	{AmArg di_args,ret;
	di_args.push(RECORD_TIMER);
	di_args.push(AnswerMachineFactory::MaxRecordTime);
	di_args.push(getLocalTag().c_str());

	user_timer->invoke("setTimer",di_args,ret);}
	status = 1;
	break;

      case 1:
	a_beep.rewind();
	playlist.addToPlaylist(new AmPlaylistItem(&a_beep,NULL));
	status = 2;
	break;

      case 2:
	dlg.bye();
	saveMessage();
	setStopped();
	break;

      }
      break;

    case AmAudioEvent::cleared:
      DBG("AmAudioEvent::cleared\n");
      break;

    default:
      DBG("Unknown event id %i\n",ae->event_id);
      break;
    }

    return;
  }

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout" &&
     plugin_event->data.get(0).asInt() == RECORD_TIMER) {

    // clear list
    playlist.close();
  }
  else
    AmSession::process(event);
}

void AnswerMachineDialog::onSessionStart(const AmSipRequest& req)
{
  // disable DTMF detection - don't use DTMF here
  setDtmfDetectionEnabled(false);

#ifdef USE_MYSQL
  string beep_file;
  if (!get_audio_file(BEEP_SOUND, "", "", "", &beep_file) ||
      beep_file.empty())
    throw string("AnswerMachine: could not find beep file\n");
  if (a_greeting.open(announce_file.c_str(),AmAudioFile::Read) ||
      a_beep.open(beep_file,AmAudioFile::Read))
    throw string("AnswerMachine: could not open greeting or beep file\n");
#else
  if (a_greeting.open(announce_file.c_str(),AmAudioFile::Read) ||
      a_beep.open(add2path(AnswerMachineFactory::AnnouncePath,1, "beep.wav"),AmAudioFile::Read))
    throw string("AnswerMachine: could not open annoucement files\n");
#endif

  msg_filename = "/tmp/" + getLocalTag() + "."
    + AnswerMachineFactory::RecFileExt;
    
  if(a_msg.open(msg_filename,AmAudioFile::Write,true))
    throw string("AnswerMachine: couldn't open ") + 
      msg_filename + string(" for writing");

  //a_msg.setRecordTime(AnswerMachineFactory::MaxRecordTime*1000);
    
  playlist.addToPlaylist(new AmPlaylistItem(&a_greeting,NULL));
  playlist.addToPlaylist(new AmPlaylistItem(&a_beep,NULL));
  //playlist.addToPlaylist(new AmPlaylistItem(NULL,&a_msg));

  setInOut(&playlist,&playlist);

  char now[15];
  sprintf(now, "%d", (int) time(NULL));
  email_dict["ts"] = now;

}

void AnswerMachineDialog::onBye(const AmSipRequest& req)
{
  setInOut(NULL, NULL);
  saveMessage();
  setStopped();
}

void AnswerMachineDialog::saveMessage()
{
  char buf[1024];
  unsigned int rec_size = a_msg.getDataSize();
  DBG("recorded data size: %i\n",rec_size);
          
  if(!rec_size){
    unlink(msg_filename.c_str());

    // record in box empty messages as well
    if (AnswerMachineFactory::SaveEmptyMsg &&
	((vm_mode == MODE_BOX) || 
	 (vm_mode == MODE_BOTH))) {
      saveBox(NULL);
    }
  } else {
    try {
      // avoid tmp file to be closed
      // ~AmMail will do that...
      a_msg.setCloseOnDestroy(false);
      a_msg.on_close();

      // copy to tmpfile for box msg
      if ((vm_mode == MODE_BOTH) ||  
	  (vm_mode == MODE_BOX))  {
	DBG("will save to box...\n");
	FILE* m_fp = a_msg.getfp();

	if (vm_mode == MODE_BOTH) {
	  // copy file to new tmpfile
	  m_fp = tmpfile();
	  if(!m_fp){
	    ERROR("could not create temporary file: %s\n",
		  strerror(errno));
	  } else {
	    FILE* fp = a_msg.getfp();
	    rewind(fp);
	    size_t nread;
	    while (!feof(fp)) {
	      nread = fread(buf, 1, 1024, fp);
	      if (fwrite(buf, 1, nread, m_fp) != nread)
		break;
	    }
	  }
	}
	saveBox(m_fp);
      }
	    
      if ((vm_mode == MODE_BOTH) ||
	  (vm_mode == MODE_VOICEMAIL)) { 
	// send mail
	AmMail* mail = new AmMail(tmpl->getEmail(email_dict));
	mail->attachements.push_back(Attachement(a_msg.getfp(),
						 "message."
						 + AnswerMachineFactory::RecFileExt,
						 a_msg.getMimeType()));
	AmMailDeamon::instance()->sendQueued(mail);
      }
    }
    catch(const string& err){
      ERROR("while creating email: %s\n",err.c_str());
    }
  }
}

void AnswerMachineDialog::saveBox(FILE* fp) {
  string msg_name = email_dict["ts"] + MSG_SEPARATOR +  
    email_dict["sender"] + ".wav";
  DBG("message name is '%s'\n", msg_name.c_str());

  AmArg di_args,ret;
  di_args.push(email_dict["domain"].c_str()); // domain
  di_args.push(email_dict["user"].c_str());   // user
  di_args.push(msg_name.c_str());             // message name
  AmArg df;
  MessageDataFile df_arg(fp);
  df.setBorrowedPointer(&df_arg);
  di_args.push(df);  
  msg_storage->invoke("msg_new",di_args,ret);  
  // TODO: evaluate ret return value
  fclose(fp);
}

