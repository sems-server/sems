
#include "UserTimer.h"

#include <sys/time.h>
#include <unistd.h>

#define SESSION_TIMER_GRANULARITY 100 // check every 100 millisec

/** \brief component for providing user_timer DI interface */
class UserTimerFactory: public AmDynInvokeFactory
{
public:
  UserTimerFactory(const string& name)
    : AmDynInvokeFactory(name) {}

  AmDynInvoke* getInstance(){
    return UserTimer::instance();
  }

  int onLoad(){
#ifdef SESSION_TIMER_THREAD
    UserTimer::instance()->start();
#endif
    return 0;
  }
};


EXPORT_PLUGIN_CLASS_FACTORY(UserTimerFactory,"user_timer");

AmTimeoutEvent::AmTimeoutEvent(int timer_id)
  : AmPluginEvent(TIMEOUTEVENT_NAME)
{
  data.push(AmArg(timer_id));
}


UserTimer::UserTimer()
{
}

UserTimer::~UserTimer()
{
}

UserTimer* UserTimer::_instance=0;

UserTimer* UserTimer::instance()
{
  if(!_instance)
    _instance = new UserTimer();
  return _instance;
}

#ifdef SESSION_TIMER_THREAD
void UserTimer::run() {
  while(1){
    usleep(SESSION_TIMER_GRANULARITY * 1000);
    checkTimers();
  }
}

void UserTimer::on_stop() {
}
#endif // SESSION_TIMER_THREAD

bool operator < (const AmTimer& l, const AmTimer& r)
{
  return timercmp(&l.time,&r.time,<);
}

void UserTimer::checkTimers() {
  vector<std::pair<string, int> > expired_timers;

  timers_mut.lock();
  if(timers.empty()){
    timers_mut.unlock();
    return;
  }
  
  struct timeval cur_time;
  gettimeofday(&cur_time,NULL);
  
  std::multiset<AmTimer>::iterator it = timers.begin();
  
  while( timercmp(&it->time,&cur_time,<) 
	 || timercmp(&it->time,&cur_time,==) ){
    int id = it->id;
    string session_id = it->session_id;
    // erase
    timers.erase(it);
    expired_timers.push_back(make_pair(session_id, id));
    
    if(timers.empty()) break;
    it = timers.begin();
  }
  timers_mut.unlock();

  for (vector<std::pair<string, int> >::iterator e_it =
	 expired_timers.begin(); e_it != expired_timers.end(); e_it++) {
    // 'fire' timer
    if (!AmSessionContainer::instance()->postEvent(e_it->first, 
						   new AmTimeoutEvent(e_it->second))) {
      DBG("Timeout Event '%d' could not be posted, session '%s' does not exist any more.\n",
	  e_it->second, e_it->first.c_str());
    }
    else {
      DBG("Timeout Event '%d' posted to %s.\n", 
	  e_it->second, e_it->first.c_str());
    }
  }
}

void UserTimer::setTimer(int id, int seconds, const string& session_id) {
  struct timeval tval;
  gettimeofday(&tval,NULL);

  tval.tv_sec += seconds;
  setTimer(id, &tval, session_id);
}

void UserTimer::setTimer(int id, struct timeval* t, 
			 const string& session_id) 
{
  timers_mut.lock();
  
  // erase old timer if exists
  unsafe_removeTimer(id, session_id);

  // add new
  timers.insert(AmTimer(id, session_id, t));
  
  timers_mut.unlock();
}


void UserTimer::removeTimer(int id, const string& session_id) {
  timers_mut.lock();
  unsafe_removeTimer(id, session_id);
  timers_mut.unlock();
}

void UserTimer::unsafe_removeTimer(int id, const string& session_id) 
{
  // erase old timer if exists
  std::multiset<AmTimer>::iterator it = timers.begin(); 
  while (it != timers.end()) {
    if ((it->id == id)&&(it->session_id == session_id)) {
      timers.erase(it);
      break;
    }
    it++;
  }
}

void UserTimer::removeTimers(const string& session_id) {
  //  DBG("removing timers for <%s>\n", session_id.c_str());
  timers_mut.lock();
  std::multiset<AmTimer>::iterator it = timers.begin(); 
  while (it != timers.end()) {
    if (it->session_id == session_id) {
      std::multiset<AmTimer>::iterator d_it = it;
      it++;
      timers.erase(d_it);
      //  DBG("    o timer removed.\n");
    } else {
      it++;
    }
  }
  timers_mut.unlock();
}

void UserTimer::removeUserTimers(const string& session_id) {
  //  DBG("removing User timers for <%s>\n", session_id.c_str());
  timers_mut.lock();
  std::multiset<AmTimer>::iterator it = timers.begin(); 
  while (it != timers.end()) {
    if ((it->id > 0)&&(it->session_id == session_id)) {
      std::multiset<AmTimer>::iterator d_it = it;
      it++;
      timers.erase(d_it);
      //  DBG("    o timer removed.\n");
    } else {
      it++;
    }
  }
  timers_mut.unlock();
}

void UserTimer::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  if(method == "setTimer"){
    setTimer(args.get(0).asInt(),
	     args.get(1).asInt(),
	     args.get(2).asCStr());
  }
  else if(method == "removeTimer"){
    removeTimer(args.get(0).asInt(),
		args.get(1).asCStr());
  }
  else if(method == "removeUserTimers"){
    removeUserTimers(args.get(0).asCStr());
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}
