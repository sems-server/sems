/*
 * $Id: AmApi.cpp,v 1.9.2.1 2005/08/03 21:00:30 sayer Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
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

#include "AmApi.h"
#include "log.h"

AmStateFactory::AmStateFactory(const string& _app_name)
  : app_name(_app_name)
{
}

int AmStateFactory::onLoad()
{
    return 0;
}

AmDialogState::AmDialogState()
    : AmEventQueue(this)
{
}

void AmDialogState::process(AmEvent* event)
{   
    AmSessionEvent* session_event = dynamic_cast<AmSessionEvent*>(event);
    if(session_event){

	DBG("in-dialog event received: %s\n",
	    session_event->request.cmd.method.c_str());

	if(session_event->event_id == AmSessionEvent::Bye)
	    onBye(&session_event->request);
	else if(onSessionEvent(session_event))
	    ERROR("while processing session event (ID=%i).\n",session_event->event_id);

	return;
	
    } 

    //  AmRequestUACStatusEvent?
    AmRequestUACStatusEvent* status_event = 
	dynamic_cast<AmRequestUACStatusEvent*>(event);
    
    if(status_event){
	DBG("in-dialog RequestUAC Staus event received: "
	    "method=%s, cseq = %d, \n\t\tcode = %d, reason = %s\n",
	    status_event->request.cmd.method.c_str(), status_event->request.cmd.cseq, 
	    status_event->code, status_event->reason.c_str());
	
	if(onUACRequestStatus(status_event))
	    ERROR("while processing session event (ID=%i).\n", 
		  session_event->event_id);

	return;
    } 

    AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
    if(audio_event && (audio_event->event_id == AmAudioEvent::cleared)){
	stopSession();
	return;
    }
    
    ERROR("AmSession: invalid event received.\n");
    return;
}

void AmDialogState::onError(unsigned int code, const string& reason)
{
}

void AmDialogState::onBeforeCallAccept(AmRequest*)
{
}

int AmDialogState::onSessionEvent(AmSessionEvent* event)
{
    if(!event->processed){
	switch(event->event_id){
	case AmSessionEvent::Refer:
	    event->request.reply(488,"Not acceptable here");
	    break;
	case AmSessionEvent::ReInvite:
	    getSession()->negotiate(&event->request);
	    break;
	}
    }
    
    return 0;
}

int AmDialogState::onUACRequestStatus(AmRequestUACStatusEvent* event) 
{ 
    if(!event->processed){
	switch(event->event_id){
	    case AmRequestUACStatusEvent::Accepted:
		DBG("UAC Request Status: Accepted (method %s, seqnr %d)\n", 
		    event->request.cmd.method.c_str(), event->request.cmd.cseq);
		break;
	    case AmRequestUACStatusEvent::Error:
		DBG("UAC Request Status: Error (method %s, seqnr %d)\n",
		    event->request.cmd.method.c_str(), event->request.cmd.cseq);
		break;
	}
	event->processed = true;
    }
    
    return 0;
}

void AmDialogState::onDtmf(int event, int duration_msec)
{
}

AmStateFactory* AmDialogState::getStateFactory() 
{ 
    return state_factory; 
}

AmSession* AmDialogState::getSession() 
{ 
    return session; 
}
