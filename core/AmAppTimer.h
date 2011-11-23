#ifndef _AmAppTimer_h_
#define _AmAppTimer_h_

#include "sip/wheeltimer.h"
#include <string>
using std::string;

#include <map>
class app_timer : public timer {
 public:
  app_timer(const string& q_id, int timer_id, unsigned int expires);
  ~app_timer();
  int get_id();
  string get_q_id();
};


class _AmAppTimer : public _wheeltimer {

  AmMutex user_timers_mut;
  std::map<string, std::map<int, app_timer*> > user_timers;

  /** creates timer object and inserts it into our container */
  app_timer* create_timer(const string& q_id, int id, unsigned int expires);
  /** erases timer - does not delete timer object @return timer object pointer, if found */
  app_timer* erase_timer(const string& q_id, int id);

 public:
  _AmAppTimer();
  ~_AmAppTimer();

  /** set a timer for event queue eventqueue_name with id timer_id and timeout (s) */
  void setTimer(const string& eventqueue_name, int timer_id, double timeout);
  /** remove timer for event queue eventqueue_name with id timer_id */
  void removeTimer(const string& eventqueue_name, int timer_id);
  /** remove all timers for event queue eventqueue_name */
  void removeTimers(const string& eventqueue_name);

  void app_timer_cb(timer* t, unsigned int data1, void* data2);
};

typedef singleton<_AmAppTimer> AmAppTimer;

#endif
