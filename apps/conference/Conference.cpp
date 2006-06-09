/*
 * $Id: Conference.cpp,v 1.7 2004/06/29 09:45:54 rco Exp $
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

#include "Conference.h"
#include "AmUtils.h"
#include "AmConfigReader.h"
#include "AmConferenceStatus.h"
#include "AmConfig.h"

#include "AmSessionContainer.h"
#include "AmSessionScheduler.h"

#include "sems.h"
#include "log.h"

#define APP_NAME "conference"

EXPORT_SESSION_FACTORY(ConferenceFactory,APP_NAME);

ConferenceFactory::ConferenceFactory(const string& _app_name)
    : AmSessionFactory(_app_name)
{
}

string ConferenceFactory::LonelyUserFile;
string ConferenceFactory::JoinSound;
string ConferenceFactory::DropSound;
string ConferenceFactory::DialoutSuffix;

int ConferenceFactory::onLoad()
{
    AmConfigReader cfg;
    if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
	return -1;

    // get application specific global parameters
    configureModule(cfg);

    LonelyUserFile = cfg.getParameter("default_announce",ANNOUNCE_PATH "/" ANNOUNCE_FILE);
    if(!file_exists(LonelyUserFile)){
	ERROR("default announce '%s' \n",LonelyUserFile.c_str());
	ERROR("for module conference does not exist.\n");
	return -1;
    }

    JoinSound = cfg.getParameter("join_sound");
    DropSound = cfg.getParameter("drop_sound");

    DialoutSuffix = cfg.getParameter("dialout_suffix");
    if(DialoutSuffix.empty()){
	WARN("No dialout_suffix has been configured in the conference plug-in:\n");
	WARN("\t -> dial out will not be available\n");
    }

    return 0;
}

AmSession* ConferenceFactory::onInvite(const AmSipRequest& req)
{
    return new ConferenceDialog(req.user);
}

ConferenceDialog::ConferenceDialog(const string& conf_id,
				   AmConferenceChannel* dialout_channel)
    : conf_id(conf_id), 
      channel(0),
      play_list(this),
      dialout_channel(dialout_channel),
      state(CS_normal)
{
    dialedout = this->dialout_channel.get() != 0;
    rtp_str.setAdaptivePlayout(true);
}

ConferenceDialog::~ConferenceDialog()
{
    DBG("ConferenceDialog::~ConferenceDialog()\n");
}

void ConferenceDialog::onStart() 
{
}

void ConferenceDialog::onSessionStart(const AmSipRequest& req)
{
    setupAudio();
}

void ConferenceDialog::onSessionStart(const AmSipReply& reply)
{
    setupAudio();
}

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

		connectMainChannel();
		break;

	    case DoConfConnect:

		state = CS_dialout_connected;
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
    if(dialedout)
	return;

    switch(state){
	
    case CS_normal:
	dtmf_seq += dtmf2str(event);

	if(dtmf_seq.length() == 2){
	    if(dtmf_seq == "#*")
		state = CS_dialing_out;
	    dtmf_seq = "";
	}
	break;

    case CS_dialing_out:{
	string digit = dtmf2str(event);

	if(digit == "*"){
	    
	    if(!dtmf_seq.empty()){
		createDialoutParticipant("sip:" + dtmf_seq + 
					 ConferenceFactory::DialoutSuffix);
		state = CS_dialed_out;
	    }
	    else {
		state = CS_normal;
	    }

	    dtmf_seq = "";
	}
	else 
	    dtmf_seq += digit;

    } break;


    case CS_dialout_connected:
	if(event == 10){ // '*'

	    AmSessionContainer::instance()
		->postEvent(dialout_id,
			    new DialoutConfEvent(DoConfConnect,
						 getLocalTag()));

	    connectMainChannel();
	}
	break;

    case CS_dialed_out:
	if(event == 11){ // '#'
	    disconnectDialout();
	}
	break;
	
    }
}

void ConferenceDialog::createDialoutParticipant(const string& uri)
{
    dialout_channel.reset(AmConferenceStatus::getChannel(getLocalTag(),getLocalTag()));

    dialout_id = AmSession::getNewId();
    
    ConferenceDialog* dialout_session = 
	new ConferenceDialog(conf_id,
			     AmConferenceStatus::getChannel(getLocalTag(),
							    dialout_id));

    AmSipDialog& dialout_dlg = dialout_session->dlg;

    dialout_dlg.local_tag    = dialout_id;
    dialout_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;

    dialout_dlg.local_party  = dlg.local_party;
    dialout_dlg.remote_party = uri;
    dialout_dlg.remote_uri   = uri;

    string body;
    int local_port = dialout_session->rtp_str.getLocalPort();
    dialout_session->sdp.genRequest(AmConfig::LocalIP,local_port,body);
    dialout_dlg.sendRequest("INVITE","application/sdp",body,"");


    play_list.close(); // !!!
    play_list.addToPlaylist(new AmPlaylistItem(dialout_channel.get(),
					       dialout_channel.get()));

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

void ConferenceDialog::onSipReply(const AmSipReply& reply)
{
    int status = dlg.getStatus();
    AmSession::onSipReply(reply);
    
    if(status < AmSipDialog::Connected){

	switch(dlg.getStatus()){

	case AmSipDialog::Connected:

	    // connected!
	    if(acceptAudio(reply.body,reply.hdrs)){
		ERROR("could not connect audio!!!\n");
		dlg.bye();
		setStopped();
	    }
	    else {
		if(getDetached() && !getStopped()){
 	
		    onSessionStart(reply);
 	    
		    if(getInput() || getOutput())
			AmSessionScheduler::instance()->addSession(this,
								   getCallgroup()); 
		    else { 
			ERROR("missing audio input and/or ouput.\n"); 
		    }
		    
		    // send connect event
		    AmSessionContainer::instance()
			->postEvent(dialout_channel->getConfID(),
				    new DialoutConfEvent(DoConfConnect,
							 dialout_channel->getConfID()));
		} 
	    }
	    break;

	case AmSipDialog::Pending:

	    switch(reply.code){
	    case 180: break;//TODO: local ring tone.
	    case 183: break;//TODO: remote ring tone.
	    default:  break;// continue waiting.
	    }
	}
    }
}

