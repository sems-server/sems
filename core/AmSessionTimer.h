
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

#ifdef SESSION_TIMER_THREAD
#include "AmThread.h"
#endif 
#include "AmSessionContainer.h"

#include <set>

class AmTimeoutEvent : public AmEvent {
 public:
  AmTimeoutEvent(int timer_id)
    : AmEvent(timer_id) { }
};

struct AmTimer
{
  int id;
  string session_id;

  struct timeval time;
    
  AmTimer(int id, const string& session_id, struct timeval* tval)
      : id(id), session_id(session_id), time(*tval) {}
};



bool operator < (const AmTimer& l, const AmTimer& r);
bool operator == (const AmTimer& l, const AmTimer& r);

/**
 * session timer class.
 * Implements a timer with session granularity.
 * On timeout an AmTimeoutEvent with the ID is posted.
 */
class AmSessionTimer 
#ifdef SESSION_TIMER_THREAD
: public AmThread
#endif
{
  static AmSessionTimer* _instance;

  std::set<AmTimer> timers;
  AmMutex         timers_mut;

  void unsafe_removeTimer(int id, const string& session_id);
 public:
  AmSessionTimer();
  ~AmSessionTimer();

  static AmSessionTimer* instance();

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
};

#endif //AM_SESSION_TIMER_H
