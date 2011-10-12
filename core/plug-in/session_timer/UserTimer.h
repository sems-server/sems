/*
 * Timer class with seconds granularity 
 */
#ifndef AM_SESSION_TIMER_H
#define AM_SESSION_TIMER_H

// if the CallTimer should run as separate thread, 
// define this one, otherwise call 
// checkTimers periodically
#define SESSION_TIMER_THREAD

#define TIMEOUT_EVENT_ID 99

// 2^5 = 32 timer buckets - should alleviate any lock contention
#define TIMERS_LOCKSTRIPE_POWER   5
#define TIMERS_LOCKSTRIPE_BUCKETS (1<<TIMERS_LOCKSTRIPE_POWER)

#ifdef SESSION_TIMER_THREAD
#include "AmThread.h"
#endif 
#include "AmSessionContainer.h"

#include <set>

/**
 * Timer Event: Name
 */
#define TIMEOUTEVENT_NAME "timer_timeout"

/**
 * \brief User Timer Event
 * data[0]: int timer_id
 */
class AmTimeoutEvent : public AmPluginEvent 
{
 public:
  AmTimeoutEvent(int timer_id);
};

/**
 * \brief Timer struct containing the alarm time.
 */
struct AmTimer
{
  int id;
  string session_id;

  struct timeval time;
    
  AmTimer(int id, const string& session_id, struct timeval* tval)
    : id(id), session_id(session_id), time(*tval) {}
};



bool operator < (const AmTimer& l, const AmTimer& r);

/**
 * \brief user timer class.
 * 
 * Implements a timer with session granularity.
 * On timeout an AmTimeoutEvent with the ID is posted.
 */
class UserTimer: public AmDynInvoke
#ifdef SESSION_TIMER_THREAD
,public AmThread
#endif
{
  static UserTimer* _instance;

  std::multiset<AmTimer> timers[TIMERS_LOCKSTRIPE_BUCKETS];
  AmMutex                timers_mut[TIMERS_LOCKSTRIPE_BUCKETS];

  unsigned int hash(const string& s1);

  void unsafe_removeTimer(int id, const string& session_id, unsigned int bucket);

 public:
  UserTimer();
  ~UserTimer();

  static UserTimer* instance();

  bool _running;

  /** set timer with ID id, fire after s seconds event in 
      session session_id  */
  void setTimer(int id, int seconds, const string& session_id);
  /** set timer with ID id, fire at time t event in session session_id */
  void setTimer(int id, struct timeval* t, const string& session_id);

  /** remove timer with ID id */
  void removeTimer(int id, const string& session_id);
  /** remove all timers belonging to the session session_id */
  void removeTimers(const string& session_id);
  /** remove all timers belonging to the session session_id with an ID > 0 */
  void removeUserTimers(const string& session_id);

  /** ifndef SESSION_TIMER_THREAD, this routine must be 
   * periodically called. */
  void checkTimers();

#ifdef SESSION_TIMER_THREAD
  void run();
  void on_stop();
#endif

  /** DI API */
  void invoke(const string& method, const AmArg& args, AmArg& ret);
};

#endif //AM_SESSION_TIMER_H
