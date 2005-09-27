/*
 * $Id: AmConferenceStatus.cpp,v 1.1.2.3 2005/08/25 06:55:12 rco Exp $
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

#include "AmConferenceStatus.h"
#include "AmConference.h"
#include "AmMultiPartyMixerSocket.h"
#include "AmMultiPartyMixer.h"

#include "AmAudio.h"
#include "log.h"

#include <assert.h>
#include <unistd.h>

AmConferenceStatus::AmConferenceStatus(const string& conference_id)
    : conf_id(conference_id), mixer()
{
// FIXME add tracing
//     if(!ConferenceFactory::TracePath.empty())
// 	str_set.enableRTPTrace(ConferenceFactory::TracePath + "/" + conf_id);
}

AmConferenceStatus::~AmConferenceStatus()
{
}

unsigned int AmConferenceStatus::add_channel(AmSession* session) {
    map<AmSession*, unsigned int>::iterator it = sessions.find(session);
    if (it != sessions.end()) {
	ERROR("session already in channel #%i.\n ", it->second);
	return it->second;
    }

    int channel =  mixer.addChannel();
    
    sessions[session] = channel;
    return channel;
}

int AmConferenceStatus::remove_channel(AmSession* session) {
    map<AmSession*, unsigned int>::iterator it = sessions.find(session);
    if (it != sessions.end()) {
	DBG("removing session 0x%lX with channel #%i from conference.\n", 
	    (unsigned long) session, it->second);
	sessions.erase(it);
	if (!sessions.size()) {
	    DBG("ConferenceStatus empty, destroying myself...\n");
	    AmConferenceStatusContainer::instance()->destroyStatus(conf_id);
	    runcond.set(false);
	}
	return 0;
    } else {
	ERROR("channel for Session 0X%lx not found.\n", (unsigned long)session);
	return -1;
    }
}

AmConferenceStatusContainer* AmConferenceStatusContainer::_instance=0;

AmConferenceStatusContainer::AmConferenceStatusContainer()
    : _run_cond(false)
{
}

AmConferenceStatusContainer* AmConferenceStatusContainer::instance()
{
    if(!_instance){
	_instance = new AmConferenceStatusContainer();
	_instance->start();
    }

    return _instance;
}

AmConferenceStatus* AmConferenceStatusContainer::add_status(const string& conf_id) {
    cid2s_mut.lock();

    AmConferenceStatus* status=0;
    bool is_new;

    map<string,AmConferenceStatus*>::iterator conf_it;
    if( ( conf_it = cid2status.find(conf_id) ) 
	!= cid2status.end() ) {

	status = conf_it->second;
	is_new = false;
    }
    else {

	DBG("creating new AmConferenceStatus: conf_id=%s\n",conf_id.c_str());
	status = new AmConferenceStatus(conf_id);
	is_new = true;

	cid2status.insert(std::make_pair(conf_id,status));
    }
    
    cid2s_mut.unlock();
    return status;
}

AmConferenceStatus* AmConferenceStatusContainer::get_status(const string& conf_id) {
    cid2s_mut.lock();
    map<string,AmConferenceStatus*>::iterator conf_it;
    if( ( conf_it = cid2status.find(conf_id) ) 
	!= cid2status.end() ) {
	cid2s_mut.unlock();
	return conf_it->second;
    }
    cid2s_mut.unlock();
    return 0;
}
 
void AmConferenceStatusContainer::destroyStatus(const string& conf_id)
{
    cid2s_mut.lock();
    map<string,AmConferenceStatus*>::iterator conf_it;
    if( ( conf_it = cid2status.find(conf_id) ) 
	!= cid2status.end() ) {

	AmConferenceStatus* status = conf_it->second;
	cid2status.erase(conf_it);
	
	ds_mut.lock();
//	status->stop();
	d_status.push(status);
	ds_mut.unlock();
	_run_cond.set(true);
    }    
    cid2s_mut.unlock();
}


// garbage collector
void AmConferenceStatusContainer::run()
{
    while(1){

	_run_cond.wait_for();

	// Let some time to Status to stop 
	sleep(5);

	ds_mut.lock();
	DBG("ConferenceStatus cleaner starting its work\n");

	try {
	    queue<AmConferenceStatus*> n_status;

	    while(!d_status.empty()){

	        AmConferenceStatus* cur_status = d_status.front();
		d_status.pop();

		ds_mut.unlock();

		if(cur_status->getStopped()){
		    delete cur_status;
		    DBG("status 0x%lX has been destroyed'\n",(unsigned long)cur_status);
		}
		else {
		    DBG("status 0x%lX still running\n",(unsigned long)cur_status);
		    n_status.push(cur_status);
		}

		ds_mut.lock();
	    }

	    swap(d_status,n_status);

	}catch(...){}

	bool more = !d_status.empty();
	ds_mut.unlock();

	DBG("ConferenceStatus cleaner finished\n");
	if(!more)
	    _run_cond.set(false);
    }
}

void AmConferenceStatusContainer::on_stop()
{
}
