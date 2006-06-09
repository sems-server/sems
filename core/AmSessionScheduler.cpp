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

#include <assert.h>
#include <sys/time.h>
#include <signal.h>

struct SchedRequest :
    public AmEvent
{
    AmSession* s;

    SchedRequest(int id, AmSession* s)
        : AmEvent(id), s(s) {}
};

/*         session scheduler              */

AmSessionScheduler* AmSessionScheduler::_instance;

AmSessionScheduler::AmSessionScheduler()
{
}

AmSessionScheduler::~AmSessionScheduler()
{
}

void AmSessionScheduler::init() {
  // start the threads
  num_threads = AmConfig::SessionSchedulerThreads;
  assert(num_threads > 0);
  DBG("Starting %u SessionSchedulerThreads.\n", num_threads);
  threads = new AmSessionSchedulerThread*[num_threads];
  for (unsigned int i=0;i<num_threads;i++) {
    threads[i] = new AmSessionSchedulerThread();
    threads[i]->start();
  }
}

AmSessionScheduler* AmSessionScheduler::instance()
{
    if(!_instance)
	_instance = new AmSessionScheduler();

    return _instance;
}

void AmSessionScheduler::addSession(AmSession* s, 
				    const string& callgroup)
{
    s->detached.set(false);
 
   // evaluate correct scheduler
    unsigned int sched_thread = 0;
    group_mut.lock();
    
    // callgroup already in a thread? 
    map<string, unsigned int>::iterator it =
      callgroup2thread.find(callgroup);
    if (it != callgroup2thread.end()) {
      // yes, use it
      sched_thread = it->second; 
    } else {
      // no, find the thread with lowest load
      unsigned int lowest_load = threads[0]->getLoad();
      for (unsigned int i=1;i<num_threads;i++) {
	unsigned int lower = threads[i]->getLoad();
	if (lower < lowest_load) {
	  lowest_load = lower; sched_thread = i;
	}
      }
      // create callgroup->thread mapping
      callgroup2thread[callgroup] = sched_thread;
    }
    
    // join the callgroup
    callgroupmembers.insert(make_pair(callgroup, s));
    session2callgroup[s]=callgroup;
    
    group_mut.unlock();
    
    // add the session to selected thread
    threads[sched_thread]->
      postRequest(new SchedRequest(InsertSession,s));
}

void AmSessionScheduler::removeSession(AmSession* s)
{
  DBG("AmSessionScheduler::removeSession\n");
  group_mut.lock();
  // get scheduler
  string callgroup = session2callgroup[s];
  unsigned int sched_thread = callgroup2thread[callgroup];
  DBG("  callgroup is '%s', thread %u\n", callgroup.c_str(), sched_thread);
  // erase callgroup membership entry
  multimap<string, AmSession*>::iterator it = 
    callgroupmembers.lower_bound(callgroup);
  while ((it != callgroupmembers.end()) &&
         (it != callgroupmembers.upper_bound(callgroup))) {
    if (it->second == s) {
      callgroupmembers.erase(it);
      break;
    }
    it++;
  }
  // erase callgroup entry if empty
  if (!callgroupmembers.count(callgroup)) {
    callgroup2thread.erase(callgroup);
    DBG("callgroup empty, erasing it.\n");
  }
  // erase session entry
  session2callgroup.erase(s);
  group_mut.unlock();    

  threads[sched_thread]->postRequest(new SchedRequest(RemoveSession,s));
}

/* the actual session scheduler thread */

AmSessionSchedulerThread::AmSessionSchedulerThread()
    : events(this)
{
}
AmSessionSchedulerThread::~AmSessionSchedulerThread()
{
}

void AmSessionSchedulerThread::on_stop()
{
}

void AmSessionSchedulerThread::run()
{
    struct timeval now,next_tick,diff,tick;
    unsigned int ts = 0;

    tick.tv_sec  = 0;
    tick.tv_usec = 10000; // 10 ms

    gettimeofday(&now,NULL);
    timeradd(&tick,&now,&next_tick);
    
    while(true){

	gettimeofday(&now,NULL);

	if(timercmp(&now,&next_tick,<)){

	    struct timespec sdiff,rem;
	    timersub(&next_tick,&now,&diff);

	    sdiff.tv_sec  = diff.tv_sec;
	    sdiff.tv_nsec = diff.tv_usec * 1000;

	    if(sdiff.tv_nsec > 2000) // 2 ms
		nanosleep(&sdiff,&rem);
	}

	processAudio(ts);
	events.processEvents();
        processDtmfEvents();

	ts += 80; // 10 ms
	timeradd(&tick,&next_tick,&next_tick);
    }
}

/**
 * process pending DTMF events
 */
void AmSessionSchedulerThread::processDtmfEvents()
{
    for(set<AmSession*>::iterator it = sessions.begin();
        it != sessions.end(); it++)
    {
        AmSession* s = (*it);
        s->processDtmfEvents();
    }
}

void AmSessionSchedulerThread::processAudio(unsigned int ts)
{
    for(set<AmSession*>::iterator it = sessions.begin();
	it != sessions.end(); it++){

	AmSession* s = (*it);
	s->lockAudio();
	AmAudio* input = s->getInput();

	if(s->rtp_str.checkInterval(ts)){

	    //DBG("ts = %u\n",ts);
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
			postRequest(new SchedRequest(AmSessionScheduler::RemoveSession,s));
			break;
		}
	    }
	    else {
		unsigned int f_size = s->rtp_str.getFrameSize();
		int size = s->rtp_str.get(ts,buffer,f_size);
		if (input) {
		    
		    int ret = input->put(ts,buffer,size);
		    if(ret < 0){
			DBG("input->put() returned: %i\n",ret);
			postRequest(new SchedRequest(AmSessionScheduler::RemoveSession,s));
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
	    
	if(s->rtp_str.sendIntReached()){
		
	    int size = output->get(ts,buffer,s->rtp_str.getFrameSize());
	    if(size <= 0){
		DBG("output->get() returned: %i\n",size);
		postRequest(new SchedRequest(AmSessionScheduler::RemoveSession,s)); //removeSession(s);
	    }
	    else if(!s->rtp_str.mute){
		
		if(s->rtp_str.put(ts,buffer,size)<0)
		  postRequest(new SchedRequest(AmSessionScheduler::RemoveSession,s));
		//		    removeSession(s);
	    }
	}
	s->unlockAudio();
    }	
}

void AmSessionSchedulerThread::process(AmEvent* e)
{
    SchedRequest* sr = dynamic_cast<SchedRequest*>(e);
    if(!sr){
	ERROR("AmSessionSchedulerThread::process: wrong event type\n");
	return;
    }

    switch(sr->event_id){

	case AmSessionScheduler::InsertSession:
	    DBG("Session inserted to the scheduler\n");
	    sessions.insert(sr->s);
	    break;

	case AmSessionScheduler::RemoveSession:{
	    AmSession* s = sr->s;
	    set<AmSession*>::iterator s_it = sessions.find(s);
	    if(s_it != sessions.end()){
		sessions.erase(s_it);
		s->clearAudio();
		s->detached.set(true);
		DBG("Session removed from the scheduler\n");
	    }
	}
	    break;

	default:
	    ERROR("AmSessionSchedulerThread::process: unknown event id.");
	    break;
    }
}

unsigned int AmSessionSchedulerThread::getLoad() {
  // lock ? 
  return sessions.size();
}

inline void AmSessionSchedulerThread::postRequest(SchedRequest* sr) {
  events.postEvent(sr);
}
