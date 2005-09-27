/*
 * $Id: AnswerMachine.cpp,v 1.20.2.1 2005/08/31 13:54:30 rco Exp $
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
#include "SerDBQuery.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmPlaylist.h"
#include "AmSessionScheduler.h"

#include "sems.h"
#include "log.h"

#include <unistd.h>
#include <dirent.h>

#define MOD_NAME               "voicemail"
#define DEFAULT_AUDIO_EXT      "wav"

#define DEFAULT_MAIL_TMPL_PATH string("/usr/local/etc/sems")
#define DEFAULT_MAIL_TMPL      string("default")
#define DEFAULT_MAIL_TMPL_EXT  string("template")

EXPORT_FACTORY(AnswerMachineFactory,MOD_NAME);

AnswerMachineFactory::AnswerMachineFactory(const string& _app_name)
  : AmStateFactory(_app_name)
{}

string AnswerMachineFactory::RecFileExt;
string AnswerMachineFactory::AnnouncePath;
string AnswerMachineFactory::DefaultAnnounce;
int    AnswerMachineFactory::MaxRecordTime;
int    AnswerMachineFactory::AcceptDelay;

int AnswerMachineFactory::loadEmailTemplates(const string& path)
{
    string email_tmpl_file = 
	path + "/" 
	+ DEFAULT_MAIL_TMPL + "."
	+ DEFAULT_MAIL_TMPL_EXT;
    
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

	string tmpl_file = path + "/" + string(entry->d_name);
	string tmpl_name = string(entry->d_name);

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

int AnswerMachineFactory::onLoad()
{
    char* p = 0;

    if(m_config.reloadModuleConfig(MOD_NAME) != 1)
	return -1;

    if( ((p = m_config.getValueForKey("rec_file_extension")) != NULL) && (*p != '\0') )
	RecFileExt = p;
    else {
	WARN("Voicemail: no rec_file_extension specified in configuration\n");
	WARN("file for module voicemail: using default.\n");
	RecFileExt = DEFAULT_AUDIO_EXT;
    }

    if( ((p = m_config.getValueForKey("announce_path")) != NULL) && (*p != '\0') )
	AnnouncePath = p;
    else {
	WARN("Voicemail: no announce_path specified in configuration\n");
	WARN("file for module voicemail: using default.\n");
	AnnouncePath = ANNOUNCE_PATH;
    }

    if( ((p = m_config.getValueForKey("default_announce")) != NULL) && (*p != '\0') )
	DefaultAnnounce = p;
    else {
	WARN("Voicemail: no default_announce specified in\n"
	     "configuration file : using default.\n");
	DefaultAnnounce = DEFAULT_ANNOUNCE;
    }

    if( ((p = m_config.getValueForKey("max_record_time")) != NULL) && (*p != '\0') ){
 	if(sscanf(p,"%u",&MaxRecordTime) != 1) {
	    ERROR("Voicemail: could not convert max_record_time ('%s') to an integer.\n",p);
	    ERROR("using default max_record_time.\n");
	    MaxRecordTime = DEFAULT_RECORD_TIME;
 	}
    }
    else {
	WARN("Voicemail: no max_record_time specified in\n");
	WARN("configuration file: using default.\n");
	MaxRecordTime = DEFAULT_RECORD_TIME;
    }

    string email_tmpl_path;
    if( ((p = m_config.getValueForKey("email_template_path")) != NULL) && (*p != '\0') ){
	email_tmpl_path = p;
    }
    else {
	WARN("no email_template_path specified in configuration\n");
	WARN("file for module voicemail: using default.\n");
	email_tmpl_path = DEFAULT_MAIL_TMPL_PATH;
    }

    if(loadEmailTemplates(email_tmpl_path)){
	ERROR("while loading email templates\n");
	return -1;
    }

    if( ((p = m_config.getValueForKey("accept_delay")) != NULL) 
	&& (*p != '\0') ){

 	if(sscanf(p,"%u",&AcceptDelay) != 1) {
	    ERROR("could not convert accept_delay ('%s') to an integer.\n",p);
	    ERROR("using default accept_delay (0).\n");
	    AcceptDelay = DEFAULT_ACCEPT_DELAY;
 	}
    }
    else {
	AcceptDelay = DEFAULT_ACCEPT_DELAY;
    }

    return 0;
}

AmDialogState* AnswerMachineFactory::onInvite(AmCmd& cmd)
{
    string announce_path = AnnouncePath;
    if( !announce_path.empty() 
	&& announce_path[announce_path.length()-1] != '/' )
	announce_path += "/";
    
    string language = cmd.getHeader("P-Language"); // local user's language

    string announce_file = announce_path + cmd.domain 
	+ "/" + cmd.user + ".wav";
    if (file_exists(announce_file)) goto announce_found;

    if (!language.empty()) {
	announce_file = announce_path + cmd.domain + "/" + language +
	    "/" +  DefaultAnnounce;
	if (file_exists(announce_file)) goto announce_found;
    }

    announce_file = announce_path + cmd.domain + "/" + DefaultAnnounce;
    if (file_exists(announce_file)) goto announce_found;

    if (!language.empty()) {
	announce_file = announce_path + language + "/" +  DefaultAnnounce;
	if (file_exists(announce_file)) goto announce_found;
    }
	
    announce_file = announce_path + DefaultAnnounce;
    if (!file_exists(announce_file)) announce_file = "";

announce_found:
    if(announce_file.empty())
	throw AmSession::Exception(500,"voicemail: no greeting file found");

    string email = cmd.getHeader("P-Email-Address");
    if (email.empty()) {
	SerDBQuery email_query("subscriber");
	email_query.addKey("email_address");
	email_query.addWhereClause("username = \"" + cmd.user + "\"");
    
	int query_res = email_query.execute();
	if(query_res < 0)
	    throw AmSession::Exception(500,"voicemail: error while"
				       " fetching user's email address");
	if(query_res == 0)
	    throw AmSession::Exception(404,"voicemail: no email address for user <" 
				       + cmd.user + ">");
    
	email = email_query.getVal(0,0);
	DBG("email address for user '%s': <%s>\n",
	    cmd.user.c_str(),email.c_str());

    } else {
	DBG("email address for user '%s': <%s> (from P-Email-Address)\n",
	    cmd.user.c_str(),email.c_str());
    }

    map<string,EmailTemplate>::iterator tmpl_it;
    if (!language.empty()) {
        tmpl_it = email_tmpl.find(cmd.domain + "_" + language);
	if(tmpl_it == email_tmpl.end()) {
	    tmpl_it = email_tmpl.find(cmd.domain);
	    if(tmpl_it == email_tmpl.end()) {
		tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL + "_"
					  + language);
		if(tmpl_it == email_tmpl.end())
			tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL);
	    }
	}
    } else {
	tmpl_it = email_tmpl.find(cmd.domain);
	if(tmpl_it == email_tmpl.end())
	    tmpl_it = email_tmpl.find(DEFAULT_MAIL_TMPL);
    }
	    
    if(tmpl_it == email_tmpl.end()){
	ERROR("Voicemail: unable to find an email template.\n");
	return 0;
    }

    return new AnswerMachineDialog(announce_file,
				   &tmpl_it->second);
}


AnswerMachineDialog::AnswerMachineDialog(string announce_file, const EmailTemplate* tmpl) 
    : announce_file(announce_file), tmpl(tmpl), playlist(this), status(0)
{
}

AnswerMachineDialog::~AnswerMachineDialog()
{
}

void AnswerMachineDialog::process(AmEvent* event)
{
    AmAudioEvent* ae = dynamic_cast<AmAudioEvent*>(event);
    if(ae){

	switch(ae->event_id){

	case AmAudioEvent::noAudio:

	    switch(status){

	    case 0:
		if(a_beep.open(AnswerMachineFactory::AnnouncePath + 
			       "/beep.wav",AmAudioFile::Read))
		    ERROR("could not open beep file\n");
		else
		    playlist.addToPlaylist(new AmPlaylistItem(&a_beep,NULL));

		status = 1;
		break;

	    case 1:
		getSession()->req->bye();
		sendMailNotification();
		stopSession();
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
    }
    else
	AmDialogState::process(event);
}

void AnswerMachineDialog::onBeforeCallAccept(AmRequest* req)
{
    if (AnswerMachineFactory::AcceptDelay != 0) {

	req->reply(180, "ringing");
	
	DBG("waiting %i seconds before accepting the call\n", 
	    AnswerMachineFactory::AcceptDelay);

	sleep(AnswerMachineFactory::AcceptDelay);
    }

    if (sessionStopped())
	throw AmSession::Exception(487, "Request Terminated");
}

void AnswerMachineDialog::onSessionStart(AmRequest* req)
{
    if(a_greeting.open(announce_file.c_str(),AmAudioFile::Read) ||
       a_beep.open(AnswerMachineFactory::AnnouncePath + "/beep.wav",AmAudioFile::Read))
	throw string("AnswerMachine: could not open annoucement files\n");

    msg_filename = "/tmp/" + req->cmd.callid + "." 
	+ AnswerMachineFactory::RecFileExt;
    
    if(a_msg.open(msg_filename,AmAudioFile::Write))
	throw string("AnswerMachine: couldn't open ") + 
	    msg_filename + string("for writing");

    a_msg.setRecordTime(AnswerMachineFactory::MaxRecordTime*1000);
    
    playlist.addToPlaylist(new AmPlaylistItem(&a_greeting,NULL));
    playlist.addToPlaylist(new AmPlaylistItem(&a_beep,NULL));
    playlist.addToPlaylist(new AmPlaylistItem(NULL,&a_msg));

    AmSession* s = getSession();
    s->setInOut(&playlist,&playlist);   
}

void AnswerMachineDialog::onBye(AmRequest* req)
{
    sendMailNotification();
    stopSession();
}

void AnswerMachineDialog::sendMailNotification()
{
    unsigned int rec_size = a_msg.getDataSize();
    DBG("recorded data size: %i\n",rec_size);
    
    if(!rec_size){
	unlink(msg_filename.c_str());
    }
    else {
	try {
	    
	    AmMail* mail = new AmMail(tmpl->getEmail(getSession()->req->cmd));
	    mail->attachements.push_back(Attachement(msg_filename,
						     "message."
						     + AnswerMachineFactory::RecFileExt,
						     a_msg.getMimeType()));
	    mail->clean_up = clean_up_mail;
	    AmMailDeamon::instance()->sendQueued(mail);
	}
	catch(const string& err){
	    ERROR("while creating email: %s\n",err.c_str());
	}
    }
}

void AnswerMachineDialog::clean_up_mail(AmMail* mail)
{
    for( Attachements::const_iterator att_it = mail->attachements.begin();
	 att_it != mail->attachements.end(); ++att_it )
	
	unlink(att_it->fullname.c_str());
}

