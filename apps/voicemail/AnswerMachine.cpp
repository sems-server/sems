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

#define MOD_NAME               "voicemail"
#define DEFAULT_AUDIO_EXT      "wav"

#define DEFAULT_MAIL_TMPL_PATH  string("/usr/local/etc/sems")
#define DEFAULT_MAIL_TMPL       string("default")
#define DEFAULT_MAIL_TMPL_EXT   string("template")
#define DEFAULT_MIN_RECORD_TIME 0
#define RECORD_TIMER 99

EXPORT_SESSION_FACTORY(AnswerMachineFactory,MOD_NAME);

AnswerMachineFactory::AnswerMachineFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{}

string AnswerMachineFactory::EmailAddress;
string AnswerMachineFactory::RecFileExt;
string AnswerMachineFactory::AnnouncePath;
string AnswerMachineFactory::DefaultAnnounce;
int    AnswerMachineFactory::MaxRecordTime;
int    AnswerMachineFactory::MinRecordTime = 0;
AmDynInvokeFactory* AnswerMachineFactory::UserTimer=0;

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
  std::string email_tmpl_file = add2path(path, 1, 
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

  AnnouncePath    = cfg.getParameter("announce_path",ANNOUNCE_PATH);
  DefaultAnnounce = cfg.getParameter("default_announce",DEFAULT_ANNOUNCE);

#endif

  MaxRecordTime   = cfg.getParameterInt("max_record_time",DEFAULT_RECORD_TIME);
  MinRecordTime   = cfg.getParameterInt("min_record_time",DEFAULT_MIN_RECORD_TIME);
  RecFileExt      = cfg.getParameter("rec_file_ext",DEFAULT_AUDIO_EXT);

  UserTimer = AmPlugIn::instance()->getFactory4Di("user_timer");
  if(!UserTimer){
	
    ERROR("could not load user_timer from session_timer plug-in\n");
    return -1;
  }

  EmailAddress = cfg.getParameter("email_address");
  return 0;
}

AmSession* AnswerMachineFactory::onInvite(const AmSipRequest& req)
{
  string language;
  string email;

  string iptel_app_param = getHeader(req.hdrs, PARAM_HDR);

  if (!EmailAddress.length()) {
    if (iptel_app_param.length()) {
      language = get_header_keyvalue(iptel_app_param,"Language");
      email = get_header_keyvalue(iptel_app_param,"Email-Address");
    } else {      
      email = getHeader(req.hdrs,"P-Email-Address");
      language = getHeader(req.hdrs,"P-Language"); // local user's language
      
      if (email.length()) {
	INFO("Use of P-Email-Address/P-Language is deprecated. \n");
	INFO("Use '%s: Email-Address=<addr>;"
	     "Language=<lang>' instead.\n",PARAM_HDR);
      }
    }
  } else {
    email = EmailAddress;
  }

  DBG("email address for user '%s': <%s> \n",
      req.user.c_str(),email.c_str());
  DBG("language: <%s> \n", language.c_str());

#ifdef USE_MYSQL

  string announce_file;

  if (get_audio_file(GREETING_MSG, req.domain, req.user, "",
		     &announce_file) && !announce_file.empty())
    goto announce_found;

  if (!language.empty()) {
    if (get_audio_file(GREETING_MSG, req.domain, "", language,
		       &announce_file) && !announce_file.empty())
      goto announce_found;
  } else {
    if (get_audio_file(GREETING_MSG, req.domain, "", "",
		       &announce_file) && !announce_file.empty())
      goto announce_found;
  }
    
  if (!language.empty())
    if (get_audio_file(GREETING_MSG, "", "", language,
		       &announce_file) && !announce_file.empty())
      goto announce_found;
    
  get_audio_file(GREETING_MSG, "", "", "", &announce_file);
    
#else

  string announce_file = add2path(AnnouncePath,2, req.domain.c_str(), (req.user + ".wav").c_str());
  if (file_exists(announce_file)) goto announce_found;

  if (!language.empty()) {
    announce_file = add2path(AnnouncePath,3, req.domain.c_str(), language.c_str(), DefaultAnnounce.c_str());
    if (file_exists(announce_file)) goto announce_found;
  }

  announce_file = add2path(AnnouncePath,2, req.domain.c_str(), DefaultAnnounce.c_str());
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

  if(email.empty())
    throw AmSession::Exception(404,"missing email address");

  std::map<string,EmailTemplate>::iterator tmpl_it;
  if (!language.empty()) {
    tmpl_it = email_tmpl.find(req.domain + "_" + language);
    if(tmpl_it == email_tmpl.end()) {
      tmpl_it = email_tmpl.find(req.domain);
      if(tmpl_it == email_tmpl.end()) {
	tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL + "_"
				  + language);
	if(tmpl_it == email_tmpl.end())
	  tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL);
      }
    }
  } else {
    tmpl_it = email_tmpl.find(req.domain);
    if(tmpl_it == email_tmpl.end())
      tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL);
  }
	    
  if(tmpl_it == email_tmpl.end()){
    ERROR("Voicemail: unable to find an email template.\n");
    return 0;
  }

  return new AnswerMachineDialog(email,announce_file,
				 &tmpl_it->second);
}


AnswerMachineDialog::AnswerMachineDialog(const string& email, 
					 const string& announce_file, 
					 const EmailTemplate* tmpl) 
  : announce_file(announce_file), 
    tmpl(tmpl), playlist(this), 
    status(0)
{
  email_dict["email"] = email;
  user_timer = AnswerMachineFactory::UserTimer->getInstance();
  if(!user_timer){
    ERROR("could not get a user timer reference\n");
    throw AmSession::Exception(500,"could not get a user timer reference");
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
	sendMailNotification();
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

  request2dict(req);
}

void AnswerMachineDialog::onBye(const AmSipRequest& req)
{
  sendMailNotification();
  setStopped();
}

void AnswerMachineDialog::sendMailNotification()
{
  int rec_length = a_msg.getLength();
  DBG("recorded file length: %i ms\n",rec_length);

  if(rec_length <= AnswerMachineFactory::MinRecordTime){
    DBG("recorded file too small. Not sending voicemail.\n");
    unlink(msg_filename.c_str());
  }
  else {
    try {
      // avoid tmp file to be closed
      // ~AmMail will do that...
      a_msg.setCloseOnDestroy(false);
      a_msg.on_close();

      AmMail* mail = new AmMail(tmpl->getEmail(email_dict));
      mail->attachements.push_back(Attachement(a_msg.getfp(),
					       "message."
					       + AnswerMachineFactory::RecFileExt,
					       a_msg.getMimeType()));
      //mail->clean_up = clean_up_mail;
      AmMailDeamon::instance()->sendQueued(mail);
    }
    catch(const string& err){
      ERROR("while creating email: %s\n",err.c_str());
    }
  }
}

void AnswerMachineDialog::clean_up_mail(AmMail* mail)
{
  //     for( Attachements::const_iterator att_it = mail->attachements.begin();
  // 	 att_it != mail->attachements.end(); ++att_it )
	
  // 	unlink(att_it->fullname.c_str());
}

void AnswerMachineDialog::request2dict(const AmSipRequest& req)
{
  email_dict["user"]   = req.user;
  email_dict["domain"] = req.domain;
  email_dict["from"]   = req.from;
  email_dict["to"]     = req.to;

  string::size_type pos1 = req.from.rfind("<sip:");
  string::size_type pos2 = req.from.find("@",pos1);

  if(pos1 != string::npos && pos2 != string::npos) {

    email_dict["from_user"] = req.from.substr(pos1+5,pos2-pos1-5);

    pos1 = pos2;
    pos2 = req.from.find(">",pos1);
    if(pos1 != string::npos && pos2 != string::npos)
      email_dict["from_domain"] = req.from.substr(pos1+1,pos2-pos1-1);
  }
}
