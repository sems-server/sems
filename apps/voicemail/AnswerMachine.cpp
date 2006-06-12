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
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmPlaylist.h"
#include "AmSessionScheduler.h"
#include "AmSessionTimer.h"

#include "sems.h"
#include "log.h"

#include <unistd.h>
#include <dirent.h>

#define MOD_NAME               "voicemail"
#define DEFAULT_AUDIO_EXT      "wav"

#define DEFAULT_MAIL_TMPL_PATH string("/usr/local/etc/sems")
#define DEFAULT_MAIL_TMPL      string("default")
#define DEFAULT_MAIL_TMPL_EXT  string("template")

#define RECORD_TIMER 99

EXPORT_SESSION_FACTORY(AnswerMachineFactory,MOD_NAME);

AnswerMachineFactory::AnswerMachineFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{}

string AnswerMachineFactory::RecFileExt;
string AnswerMachineFactory::AnnouncePath;
string AnswerMachineFactory::DefaultAnnounce;
int    AnswerMachineFactory::MaxRecordTime;
int    AnswerMachineFactory::AcceptDelay;

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

int AnswerMachineFactory::onLoad()
{
    AmConfigReader cfg;
    if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1, MOD_NAME ".conf")))
	return -1;

    // get application specific global parameters
    configureModule(cfg);

    AnnouncePath    = cfg.getParameter("announce_path",ANNOUNCE_PATH);
    DefaultAnnounce = cfg.getParameter("default_announce",DEFAULT_ANNOUNCE);
    MaxRecordTime   = cfg.getParameterInt("max_record_time",DEFAULT_RECORD_TIME);
    RecFileExt      = cfg.getParameter("rec_file_ext",DEFAULT_AUDIO_EXT);

    if(loadEmailTemplates(cfg.getParameter("email_template_path",DEFAULT_MAIL_TMPL_PATH))){
	ERROR("while loading email templates\n");
	return -1;
    }

    AcceptDelay = DEFAULT_ACCEPT_DELAY;

    return 0;
}

AmSession* AnswerMachineFactory::onInvite(const AmSipRequest& req)
{
    string language = getHeader(req.hdrs,"P-Language"); // local user's language

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

announce_found:
    if(announce_file.empty())
	throw AmSession::Exception(500,"voicemail: no greeting file found");

    string email = getHeader(req.hdrs,"P-Email-Address");
    DBG("email address for user '%s': <%s> (from P-Email-Address)\n",
	req.user.c_str(),email.c_str());

    if(email.empty())
	throw AmSession::Exception(404,"missing email address");

    map<string,EmailTemplate>::iterator tmpl_it;
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
		playlist.addToPlaylist(new AmPlaylistItem(NULL,&a_msg));
		AmSessionTimer::instance()->
		    setTimer(RECORD_TIMER,AnswerMachineFactory::MaxRecordTime,
			     getLocalTag());
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

    AmTimeoutEvent* to = dynamic_cast<AmTimeoutEvent*>(event);
    if(to && to->event_id == RECORD_TIMER){
	
	// clear list
	playlist.close();
    }
    else
	AmSession::process(event);
}

void AnswerMachineDialog::onSessionStart(const AmSipRequest& req)
{
    if(a_greeting.open(announce_file.c_str(),AmAudioFile::Read) ||
       a_beep.open(add2path(AnswerMachineFactory::AnnouncePath,1, "beep.wav"),AmAudioFile::Read))
	throw string("AnswerMachine: could not open annoucement files\n");

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
    unsigned int rec_size = a_msg.getDataSize();
    DBG("recorded data size: %i\n",rec_size);
    
    if(!rec_size){
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
