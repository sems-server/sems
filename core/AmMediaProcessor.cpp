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

#include "AmMediaProcessor.h"
#include "AmSession.h"
#include "AmRtpStream.h"

#include <assert.h>
#include <sys/time.h>
#include <signal.h>

// Solaris seems to need this for nanosleep().
#if defined (__SVR4) && defined (__sun)
#include <time.h>
#endif

/** \brief Request event to the MediaProcessor (remove,...) */
struct SchedRequest :
  public AmEvent
{
  AmSession* s;

  SchedRequest(int id, AmSession* s)
    : AmEvent(id), s(s) {}
};

/*         session scheduler              */

AmMediaProcessor* AmMediaProcessor::_instance = NULL;

AmMediaProcessor::AmMediaProcessor()
  : threads(NULL),num_threads(0)
{
}

AmMediaProcessor::~AmMediaProcessor()
{
  INFO("Media processor has been recycled.\n");
}

void AmMediaProcessor::init() {
  // start the threads
  num_threads = AmConfig::MediaProcessorThreads;
  assert(num_threads > 0);
  DBG("Starting %u MediaProcessorThreads.\n", num_threads);
  threads = new AmMediaProcessorThread*[num_threads];
  for (unsigned int i=0;i<num_threads;i++) {
    threads[i] = new AmMediaProcessorThread();
    threads[i]->start();
  }
}

AmMediaProcessor* AmMediaProcessor::instance()
{
  if(!_instance)
    _instance = new AmMediaProcessor();

  return _instance;
}

void AmMediaProcessor::addSession(AmSession* s, 
				  const string& callgroup)
{
  s->detached.set(false);
 
  // evaluate correct scheduler
  unsigned int sched_thread = 0;
  group_mut.lock();
    
  // callgroup already in a thread? 
  std::map<std::string, unsigned int>::iterator it =
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

void AmMediaProcessor::clearSession(AmSession* s) {
  removeFromProcessor(s, ClearSession);
}

void AmMediaProcessor::removeSession(AmSession* s) {
  removeFromProcessor(s, RemoveSession);
}

/* FIXME: implement Call Group ts offsets for soft changing of 
	call groups 
*/
void AmMediaProcessor::changeCallgroup(AmSession* s, 
				       const string& new_callgroup) {
  removeFromProcessor(s, SoftRemoveSession);
  addSession(s, new_callgroup);
}

void AmMediaProcessor::removeFromProcessor(AmSession* s, 
					   unsigned int r_type) {
  DBG("AmMediaProcessor::removeSession\n");
  group_mut.lock();
  // get scheduler
  string callgroup = session2callgroup[s];
  unsigned int sched_thread = callgroup2thread[callgroup];
  DBG("  callgroup is '%s', thread %u\n", callgroup.c_str(), sched_thread);
  // erase callgroup membership entry
  std::multimap<std::string, AmSession*>::iterator it = 
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

  threads[sched_thread]->postRequest(new SchedRequest(r_type,s));
}

void AmMediaProcessor::stop() {
  assert(threads);
  for (unsigned int i=0;i<num_threads;i++) {
    if(threads[i] != NULL) {
      threads[i]->stop();
    }
  }
  bool threads_stopped = true;
  do {
    usleep(10000);
    threads_stopped = true;
    for (unsigned int i=0;i<num_threads;i++) {
      if((threads[i] != NULL) &&(!threads[i]->is_stopped())) {
        threads_stopped = false;
        break;
      }
    }
  } while(!threads_stopped);
  
  for (unsigned int i=0;i<num_threads;i++) {
    if(threads[i] != NULL) {
      delete threads[i];
      threads[i] = NULL;
    }
  }
  delete []  threads;
  threads = NULL;
}

void AmMediaProcessor::dispose() 
{
  if(_instance != NULL) {
    if(_instance->threads != NULL) {
      _instance->stop();
    }
    delete _instance;
    _instance = NULL;
  }
}

/* the actual media processing thread */

AmMediaProcessorThread::AmMediaProcessorThread()
  : events(this), stop_requested(false)
{
}
AmMediaProcessorThread::~AmMediaProcessorThread()
{
}

void AmMediaProcessorThread::on_stop()
{
  INFO("requesting media processor to stop.\n");
  stop_requested.set(true);
}

void AmMediaProcessorThread::run()
{
  stop_requested = false;
  struct timeval now,next_tick,diff,tick;
  // wallclock time
  unsigned int ts = 0;

  tick.tv_sec  = 0;
  tick.tv_usec = 10000; // 10 ms

  gettimeofday(&now,NULL);
  timeradd(&tick,&now,&next_tick);
    
  while(!stop_requested.get()){

    gettimeofday(&now,NULL);

    if(timercmp(&now,&next_tick,<)){

      struct timespec sdiff,rem;
      timersub(&next_tick,&now,&diff);

      sdiff.tv_sec  = diff.tv_sec;
      sdiff.tv_nsec = diff.tv_usec * 1000;

      if(sdiff.tv_nsec > 2000000) // 2 ms
	nanosleep(&sdiff,&rem);
    }

    processAudio(ts);
    events.processEvents();
    processDtmfEvents();

    ts += 10 * SYSTEM_SAMPLERATE / 1000; // 10 ms
    timeradd(&tick,&next_tick,&next_tick);
  }
}

/**
 * process pending DTMF events
 */
void AmMediaProcessorThread::processDtmfEvents()
{
  for(set<AmSession*>::iterator it = sessions.begin();
      it != sessions.end(); it++)
    {
      AmSession* s = (*it);
      s->processDtmfEvents();
    }
}

void AmMediaProcessorThread::processAudio(unsigned int ts)
{
  // receiving
  for(set<AmSession*>::iterator it = sessions.begin();
      it != sessions.end(); it++){

    AmSession* s = (*it);
    // todo: get frame size/checkInterval from local audio if local in+out (?)
    unsigned int f_size = s->RTPStream()->getFrameSize(); 

    // complete frame time reached? 
    if (s->RTPStream()->checkInterval(ts, f_size)) {
      s->lockAudio();

      int got_audio = -1;

      // get/receive audio
      if (!s->getAudioLocal(AM_AUDIO_IN)) {
	// input is not local - receive from rtp stream
	if (s->RTPStream()->receiving || s->RTPStream()->getPassiveMode()) {
	  int ret = s->RTPStream()->receive(ts);
	  if(ret < 0){
	    switch(ret){
	      
	    case RTP_DTMF:
	    case RTP_UNKNOWN_PL:
	    case RTP_PARSE_ERROR:
	      break;
	      
	    case RTP_TIMEOUT:
	      postRequest(new SchedRequest(AmMediaProcessor::RemoveSession,s));
	      s->postEvent(new AmRtpTimeoutEvent());
	      break;
	      
	    case RTP_BUFFER_SIZE:
	    default:
	      ERROR("AmRtpAudio::receive() returned %i\n",ret);
	      postRequest(new SchedRequest(AmMediaProcessor::ClearSession,s));
	      break;
	    }
	  } else {
	    got_audio = s->RTPStream()->get(ts,buffer,f_size);
	    
	    if (s->isDtmfDetectionEnabled() && got_audio > 0)
	      s->putDtmfAudio(buffer, got_audio, ts);
	  }
	}
      } else {
	// input is local - get audio from local_in
	AmAudio* local_input = s->getLocalInput(); 
	if (local_input) {
	  got_audio = local_input->get(ts,buffer,f_size);
	}
      }

      // process received audio
      if (got_audio >= 0) {
	AmAudio* input = s->getInput();
	if (input) {
	  int ret = input->put(ts,buffer,got_audio);
	  if(ret < 0){
	    DBG("input->put() returned: %i\n",ret);
	    postRequest(new SchedRequest(AmMediaProcessor::ClearSession,s));
	  }
	}
      }

      s->unlockAudio();
    }
  }

  // sending
  for(set<AmSession*>::iterator it = sessions.begin();
      it != sessions.end(); it++){

    AmSession* s = (*it);
    s->lockAudio();
    AmAudio* output = s->getOutput();
	    
    if(output && s->RTPStream()->sendIntReached()){
		
      int size = output->get(ts,buffer,s->RTPStream()->getFrameSize());
      if(size <= 0){
	DBG("output->get() returned: %i\n",size);
	postRequest(new SchedRequest(AmMediaProcessor::ClearSession,s)); 
      }
      else {
	if (!s->getAudioLocal(AM_AUDIO_OUT)) {
	  // audio should go to RTP
	  if(!s->RTPStream()->mute){	     
	    if(s->RTPStream()->put(ts,buffer,size)<0)
	      postRequest(new SchedRequest(AmMediaProcessor::ClearSession,s));
	  }
	} else {
	  // output is local - audio should go in local_out
	  AmAudio* local_output = s->getLocalOutput(); 
	  if (local_output) {
	    if (local_output->put(ts,buffer,size)) {
	      postRequest(new SchedRequest(AmMediaProcessor::ClearSession,s));
	    }
	  }
	}
      }
    }
    s->unlockAudio();
  }	
}

void AmMediaProcessorThread::process(AmEvent* e)
{
  SchedRequest* sr = dynamic_cast<SchedRequest*>(e);
  if(!sr){
    ERROR("AmMediaProcessorThread::process: wrong event type\n");
    return;
  }

  switch(sr->event_id){

  case AmMediaProcessor::InsertSession:
    DBG("Session inserted to the scheduler\n");
    sessions.insert(sr->s);
    break;

  case AmMediaProcessor::RemoveSession:{
    AmSession* s = sr->s;
    set<AmSession*>::iterator s_it = sessions.find(s);
    if(s_it != sessions.end()){
      sessions.erase(s_it);
      s->detached.set(true);
      DBG("Session removed from the scheduler\n");
    }
  }
    break;

  case AmMediaProcessor::ClearSession:{
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


  case AmMediaProcessor::SoftRemoveSession:{
    AmSession* s = sr->s;
    set<AmSession*>::iterator s_it = sessions.find(s);
    if(s_it != sessions.end()){
      sessions.erase(s_it);
      DBG("Session removed softly from the scheduler\n");
    }
  }
    break;

  default:
    ERROR("AmMediaProcessorThread::process: unknown event id.");
    break;
  }
}

unsigned int AmMediaProcessorThread::getLoad() {
  // lock ? 
  return sessions.size();
}

inline void AmMediaProcessorThread::postRequest(SchedRequest* sr) {
  events.postEvent(sr);
}
