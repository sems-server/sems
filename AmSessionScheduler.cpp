/*
 * $Id: AmSessionScheduler.cpp,v 1.1.2.9 2005/08/31 13:54:29 rco Exp $
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


#include "AmSessionScheduler.h"

#include <sys/time.h>
#include <signal.h>

struct SchedRequest:
    public AmEvent
{
    AmSession* s;

    SchedRequest(int id, AmSession* s)
        : AmEvent(id), s(s) {}
};


AmSessionScheduler* AmSessionScheduler::_instance;

AmSessionScheduler::AmSessionScheduler()
    : events(this)
{
}

AmSessionScheduler::~AmSessionScheduler()
{
}

AmSessionScheduler* AmSessionScheduler::instance()
{
    if(!_instance)
	_instance = new AmSessionScheduler();

    return _instance;
}

void AmSessionScheduler::addSession(AmSession* s)
{
    s->detached.set(false);
    events.postEvent(new SchedRequest(InsertSession,s));
}

void AmSessionScheduler::removeSession(AmSession* s)
{
    DBG("AmSessionScheduler::removeSession\n");
    events.postEvent(new SchedRequest(RemoveSession,s));
}

void AmSessionScheduler::on_stop()
{
}

static AmCondition<bool> alarm_fired(false);

static void h_alarm(int signum)
{
    alarm_fired.set(true);
}

void AmSessionScheduler::run()
{
    unsigned int   ts = 0;
    struct timeval next_alarm,inc_alarm,curr;
    struct itimerval tval;

    alarm_fired.set(false);
    signal(SIGALRM,h_alarm);

    inc_alarm.tv_sec = 0;
    inc_alarm.tv_usec = 10000; // 10ms
    tval.it_value = inc_alarm;
    tval.it_interval = inc_alarm;

    gettimeofday(&next_alarm,NULL);
    setitimer(ITIMER_REAL,&tval,NULL);

    while(true){

	processAudio(ts);
	events.processEvents();
        processDtmfEvents();

	// timer sync
	timeradd(&next_alarm,&inc_alarm,&next_alarm);
	gettimeofday(&curr,NULL);

	if(timercmp(&next_alarm, &curr, <)){

	    struct timeval tv_diff;
	    timersub(&curr, &next_alarm, &tv_diff);
	    double msec_late = tv_diff.tv_sec * 1000.0 + tv_diff.tv_usec / 1000.0;
	    if (msec_late >= 100) {
		 DBG("timer late for %.3f msec\n", msec_late);
		 next_alarm = curr;
	    }
	}
	else {
	    alarm_fired.wait_for();
	    alarm_fired.set(false);
	}

	ts += 80; // 10 ms
    }
}

/**
 * process pending DTMF events
 */
void AmSessionScheduler::processDtmfEvents()
{
    for(set<AmSession*>::iterator it = sessions.begin();
        it != sessions.end(); it++)
    {
        AmSession* s = (*it);
        s->processDtmfEvents();
    }
}

void AmSessionScheduler::processAudio(unsigned int ts)
{
    for(set<AmSession*>::iterator it = sessions.begin();
	it != sessions.end(); it++){

	AmSession* s = (*it);
	s->lockAudio();
	AmAudio* input = s->getInput();

	if(!(ts % s->rtp_str.getFrameSize())){

	    int ret = s->rtp_str.receive(ts);
	    if(ret < 0){
		switch(ret){

		    case RTP_DTMF:
		    case RTP_UNKNOWN_PL:
		    case RTP_PARSE_ERROR:
			break;

		    case RTP_TIMEOUT:
		    case RTP_BUFFER_SIZE:
		    default:
			ERROR("AmRtpAudio::receive() returned %i\n",ret);
			removeSession(s);
			break;
		}
	    }
	    else {
		unsigned int f_size = s->rtp_str.getFrameSize();
		int size = s->rtp_str.get(ts,buffer,f_size);
		if (input) {
		    
		    int ret = input->put(ts,buffer,size);
		    if(ret <= 0){
			DBG("input->put() returned: %i\n",ret);
			removeSession(s);
		    }
		}
                if (s->isDtmfDetectionEnabled())
                    s->putDtmfAudio(buffer, size, ts);
            }
	}
	s->unlockAudio();
    }

    for(set<AmSession*>::iterator it = sessions.begin();
	it != sessions.end(); it++){

	AmSession* s = (*it);
	s->lockAudio();
	AmAudio* output = s->getOutput();
	    
	if(output && !(ts % s->rtp_str.getFrameSize())){
		
	    int size = output->get(ts,buffer,s->rtp_str.getFrameSize());
	    if(size <= 0){
		DBG("output->get() returned: %i\n",size);
		removeSession(s);
	    }
	    else
		s->rtp_str.put(ts,buffer,size);
	}
	s->unlockAudio();
    }	
}

void AmSessionScheduler::process(AmEvent* e)
{
    SchedRequest* sr = dynamic_cast<SchedRequest*>(e);
    if(!sr){
	ERROR("AmSessionScheduler::process: wrong event type\n");
	return;
    }

    switch(sr->event_id){

	case InsertSession:
	    DBG("Session inserted to the scheduler\n");
	    sessions.insert(sr->s);
	    break;

	case RemoveSession:{
	    AmSession* s = sr->s;
	    sessions.erase(s);
	    s->clearAudio();
	    s->detached.set(true);
	    DBG("Session removed from the scheduler\n");}
	    break;

	default:
	    ERROR("AmSessionScheduler::process: unknown event id.");
	    break;
    }
}
