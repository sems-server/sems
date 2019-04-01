/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
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
  AmMediaSession* s;

  SchedRequest(int id, AmMediaSession* s)
    : AmEvent(id), s(s) {}
};

/*         session scheduler              */

AmMediaProcessor* AmMediaProcessor::_instance = NULL;

AmMediaProcessor::AmMediaProcessor()
  : num_threads(0), threads(NULL)
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

void AmMediaProcessor::addSession(AmMediaSession* s, 
				  const string& callgroup)
{
  s->onMediaProcessingStarted();
 
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

void AmMediaProcessor::clearSession(AmMediaSession* s) {
  removeFromProcessor(s, ClearSession);
}

void AmMediaProcessor::removeSession(AmMediaSession* s) {
  removeFromProcessor(s, RemoveSession);
}

void AmMediaProcessor::softRemoveSession(AmMediaSession* s) {
  removeFromProcessor(s, SoftRemoveSession);
}

/* FIXME: implement Call Group ts offsets for soft changing of 
	call groups 
*/
void AmMediaProcessor::changeCallgroup(AmMediaSession* s, 
				       const string& new_callgroup) {
  removeFromProcessor(s, SoftRemoveSession);
  addSession(s, new_callgroup);
}

void AmMediaProcessor::removeFromProcessor(AmMediaSession* s, 
					   unsigned int r_type) {
  DBG("AmMediaProcessor::removeSession\n");
  group_mut.lock();
  // get scheduler
  string callgroup = session2callgroup[s];
  unsigned int sched_thread = callgroup2thread[callgroup];
  DBG("  callgroup is '%s', thread %u\n", callgroup.c_str(), sched_thread);
  // erase callgroup membership entry
  std::multimap<std::string, AmMediaSession*>::iterator it = 
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
    usleep(10000); // 10ms
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
  unsigned long long ts = 0;//4294417296;

  tick.tv_sec  = 0;
  tick.tv_usec = 1000*WC_INC_MS;

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

    ts = (ts + WC_INC) & WALLCLOCK_MASK;
    timeradd(&tick,&next_tick,&next_tick);
  }
}

/**
 * process pending DTMF events
 */
void AmMediaProcessorThread::processDtmfEvents()
{
  for(set<AmMediaSession*>::iterator it = sessions.begin();
      it != sessions.end(); it++)
    {
      AmMediaSession* s = (*it);
      s->processDtmfEvents();
    }
}

void AmMediaProcessorThread::processAudio(unsigned long long ts)
{
  // receiving
  for(set<AmMediaSession*>::iterator it = sessions.begin();
      it != sessions.end(); it++)
  {
    if ((*it)->readStreams(ts, buffer) < 0)
      postRequest(new SchedRequest(AmMediaProcessor::ClearSession, *it));
  }

  // sending
  for(set<AmMediaSession*>::iterator it = sessions.begin();
      it != sessions.end(); it++)
  {
    if ((*it)->writeStreams(ts, buffer) < 0)
      postRequest(new SchedRequest(AmMediaProcessor::ClearSession, *it));
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
    sr->s->clearRTPTimeout();
    break;

  case AmMediaProcessor::RemoveSession:{
    AmMediaSession* s = sr->s;
    set<AmMediaSession*>::iterator s_it = sessions.find(s);
    if(s_it != sessions.end()){
      sessions.erase(s_it);
      s->onMediaProcessingTerminated();
      DBG("Session removed from the scheduler\n");
    }
  }
    break;

  case AmMediaProcessor::ClearSession:{
    AmMediaSession* s = sr->s;
    set<AmMediaSession*>::iterator s_it = sessions.find(s);
    if(s_it != sessions.end()){
      sessions.erase(s_it);
      s->clearAudio();
      s->onMediaProcessingTerminated();
      DBG("Session removed from the scheduler\n");
    }
  }
    break;


  case AmMediaProcessor::SoftRemoveSession:{
    AmMediaSession* s = sr->s;
    set<AmMediaSession*>::iterator s_it = sessions.find(s);
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
