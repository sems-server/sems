
#include "AmSessionTimer.h"

#include <sys/time.h>

#define SESSION_TIMER_GRANULARITY 100 // check every 100 millisec


AmSessionTimer::AmSessionTimer()
{
}

AmSessionTimer::~AmSessionTimer()
{
}

AmSessionTimer* AmSessionTimer::_instance=0;

AmSessionTimer* AmSessionTimer::instance()
{
    if(!_instance)
	_instance = new AmSessionTimer();
    return _instance;
}

#ifdef SESSION_TIMER_THREAD
void AmSessionTimer::run() {
    while(1){
	usleep(SESSION_TIMER_GRANULARITY * 1000);
        checkTimers();
    }
}

void AmSessionTimer::on_stop() {
}
#endif // SESSION_TIMER_THREAD

bool operator < (const AmTimer& l, const AmTimer& r)
{
  return timercmp(&l.time,&r.time,<);
}

bool operator == (const AmTimer& l, const AmTimer& r)
{
  return l.id == r.id;
}

void AmSessionTimer::checkTimers() {
  timers_mut.lock();
  if(timers.empty()){
    timers_mut.unlock();
    return;
  }
  
  struct timeval cur_time;
  gettimeofday(&cur_time,NULL);
  
  set<AmTimer>::iterator it = timers.begin();
  
  while( timercmp(&it->time,&cur_time,<) 
	 || timercmp(&it->time,&cur_time,==) ){
    int id = it->id;
    string session_id = it->session_id;
    // erase
    timers.erase(it);
    // 'fire' timer	  
    if (!AmSessionContainer::instance()->postEvent(session_id, 
						   new AmTimeoutEvent(id))) {
      DBG("Timeout Event could not be posted, session does not exist any more.\n");
    }
    
    
    if(timers.empty()) break;
    it = timers.begin();
  }
  timers_mut.unlock();
}

void AmSessionTimer::setTimer(int id, int seconds, const string& session_id) {
    struct timeval tval;
    gettimeofday(&tval,NULL);

    tval.tv_sec += seconds;
    setTimer(id, &tval, session_id);
}

void AmSessionTimer::setTimer(int id, struct timeval* t, 
			      const string& session_id) 
{
  timers_mut.lock();
  
  // erase old timer if exists
  unsafe_removeTimer(id, session_id);

  // add new
  timers.insert(AmTimer(id, session_id, t));
  
  timers_mut.unlock();
}


void AmSessionTimer::removeTimer(int id, const string& session_id) {
  timers_mut.lock();
  unsafe_removeTimer(id, session_id);
  timers_mut.unlock();
}

void AmSessionTimer::unsafe_removeTimer(int id, const string& session_id) 
{
  // erase old timer if exists
  set<AmTimer>::iterator it = timers.begin(); 
  while (it != timers.end()) {
    if ((it->id == id)&&(it->session_id == session_id)) {
      timers.erase(it);
      break;
    }
    it++;
  }
}

void AmSessionTimer::removeTimers(const string& session_id) {
  //  DBG("removing timers for <%s>\n", session_id.c_str());
  timers_mut.lock();
  for (set<AmTimer>::iterator it = timers.begin(); 
       it != timers.end(); it++) {
    if (it->session_id == session_id) {
      timers.erase(it);
      //  DBG("    o timer removed.\n");
    }
  }
  timers_mut.unlock();
}

void AmSessionTimer::removeUserTimers(const string& session_id) {
  //  DBG("removing User timers for <%s>\n", session_id.c_str());
  timers_mut.lock();
  for (set<AmTimer>::iterator it = timers.begin(); 
       it != timers.end(); it++) {
    if ((it->id > 0)&&(it->session_id == session_id)) {
      timers.erase(it);
      //  DBG("    o timer removed.\n");
    }
  }
  timers_mut.unlock();
}
