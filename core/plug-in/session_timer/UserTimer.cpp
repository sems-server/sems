
#include "UserTimer.h"

#include "sip/hash.h"

#include <sys/time.h>
#include <unistd.h>
#include <math.h>

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

#ifdef SESSION_TIMER_THREAD
  void onUnload() {
    DBG("stopping userTimer thread\n");
    AmThreadWatcher::instance()->add(UserTimer::instance());
    UserTimer::instance()->_running = false;
  }
#endif
};


EXPORT_PLUGIN_CLASS_FACTORY(UserTimerFactory,"user_timer");

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
  _running = true;
  while(_running){
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

  struct timeval cur_time;
  gettimeofday(&cur_time,NULL);
  
  // run through all buckets
  for (unsigned int bucket=0;bucket<TIMERS_LOCKSTRIPE_BUCKETS;bucket++) {
    // get expired timers in bucket
    timers_mut[bucket].lock();
    if (!timers[bucket].empty()) {
      std::multiset<AmTimer>::iterator it = timers[bucket].begin();
  
      while (timercmp(&it->time,&cur_time,<)
	     || timercmp(&it->time,&cur_time,==)) {
	int id = it->id;
	string session_id = it->session_id;
	// erase
	timers[bucket].erase(it);
	expired_timers.push_back(make_pair(session_id, id));
    
	if(timers[bucket].empty()) break;
	it = timers[bucket].begin();
      }
    }
    timers_mut[bucket].unlock();
  }

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

void UserTimer::setTimer(int id, double seconds, const string& session_id) {
  struct timeval tval;
  gettimeofday(&tval,NULL);

  struct timeval diff;
  diff.tv_sec = trunc(seconds);
  diff.tv_usec = 1000000.0*(double)seconds - 1000000.0*trunc(seconds);
  timeradd(&tval, &diff, &tval);

  setTimer(id, &tval, session_id);
}

void UserTimer::setTimer(int id, struct timeval* t, 
			 const string& session_id) 
{
  unsigned int bucket = hash(session_id);

  timers_mut[bucket].lock();
  
  // erase old timer if exists
  unsafe_removeTimer(id, session_id, bucket);

  // add new
  timers[bucket].insert(AmTimer(id, session_id, t));
  
  timers_mut[bucket].unlock();
}


void UserTimer::removeTimer(int id, const string& session_id) {
  unsigned int bucket = hash(session_id);
  timers_mut[bucket].lock();
  unsafe_removeTimer(id, session_id, bucket);
  timers_mut[bucket].unlock();
}


unsigned int UserTimer::hash(const string& s1)
{
  return hashlittle(s1.c_str(),s1.length(),0)
    & (TIMERS_LOCKSTRIPE_BUCKETS-1);
}

void UserTimer::unsafe_removeTimer(int id, const string& session_id, unsigned int bucket)
{
  // erase old timer if exists
  std::multiset<AmTimer>::iterator it = timers[bucket].begin();
  while (it != timers[bucket].end()) {
    if ((it->id == id)&&(it->session_id == session_id)) {
      timers[bucket].erase(it);
      break;
    }
    it++;
  }
}

void UserTimer::removeTimers(const string& session_id) {
  //  DBG("removing timers for <%s>\n", session_id.c_str());
  unsigned int bucket = hash(session_id);
  timers_mut[bucket].lock();
  std::multiset<AmTimer>::iterator it = timers[bucket].begin();
  while (it != timers[bucket].end()) {
    if (it->session_id == session_id) {
      std::multiset<AmTimer>::iterator d_it = it;
      it++;
      timers[bucket].erase(d_it);
      //  DBG("    o timer removed.\n");
    } else {
      it++;
    }
  }
  timers_mut[bucket].unlock();
}

void UserTimer::removeUserTimers(const string& session_id) {
  //  DBG("removing User timers for <%s>\n", session_id.c_str());
  unsigned int bucket = hash(session_id);
  timers_mut[bucket].lock();
  std::multiset<AmTimer>::iterator it = timers[bucket].begin();
  while (it != timers[bucket].end()) {
    if ((it->id > 0)&&(it->session_id == session_id)) {
      std::multiset<AmTimer>::iterator d_it = it;
      it++;
      timers[bucket].erase(d_it);
      //  DBG("    o timer removed.\n");
    } else {
      it++;
    }
  }
  timers_mut[bucket].unlock();
}

void UserTimer::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  if(method == "setTimer"){
    if (isArgInt(args.get(1))) {
      setTimer(args.get(0).asInt(),
	       args.get(1).asInt(),
	       args.get(2).asCStr());
    } else if (isArgDouble(args.get(1))) {
      setTimer(args.get(0).asInt(),
	       args.get(1).asDouble(),
	       args.get(2).asCStr());
    } else if (isArgBlob(args.get(1))) {
      ArgBlob* blob = args.get(1).asBlob();
      if(blob->len != sizeof(struct timeval)) {
	ERROR("unsupported data in blob in '%s', expected struct timeval\n",
	      AmArg::print(args).c_str());
      }
      else {
      setTimer(args.get(0).asInt(),
	       (struct timeval*)blob->data,
	       args.get(2).asCStr());
      }
      } else {
	ERROR("unsupported timeout type in '%s'\n", AmArg::print(args).c_str());
      }
  }
  else if(method == "removeTimer"){
    removeTimer(args.get(0).asInt(),
		args.get(1).asCStr());
  }
  else if(method == "removeUserTimers"){
    removeUserTimers(args.get(0).asCStr());
  }
  else if(method == "stop"){
    _running = false;
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}
