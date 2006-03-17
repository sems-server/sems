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

    return 0;
}

AmSession* ConferenceFactory::onInvite(const AmSipRequest& req)
{
    return new ConferenceDialog(req.user);
}

ConferenceDialog::ConferenceDialog(const string& conf_id)
    : conf_id(conf_id), channel(0), play_list(this)
{
}

ConferenceDialog::~ConferenceDialog()
{
}

void ConferenceDialog::onStart() 
{
  //  setUseSessionTimer(false);
}

void ConferenceDialog::onSessionStart(const AmSipRequest& req)
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

    channel.reset(AmConferenceStatus::getChannel(conf_id,getLocalTag()));

    play_list.addToPlaylist(new AmPlaylistItem(channel.get(),channel.get()));

    setInOut(&play_list,&play_list);
    
    setCallgroup(conf_id);

    //setInOut(channel.get(),channel.get());
}

void ConferenceDialog::onBye(const AmSipRequest& req)
{
    play_list.close();
    setInOut(NULL,NULL);
    channel.reset(NULL);
    setStopped();
}

void ConferenceDialog::process(AmEvent* ev)
{
    ConferenceEvent* ce = dynamic_cast<ConferenceEvent*>(ev);
    if(ce){
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

    AmSession::process(ev);
}
