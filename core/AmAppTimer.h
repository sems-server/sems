/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
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

#ifndef _AmAppTimer_h_
#define _AmAppTimer_h_

#include "sip/wheeltimer.h"
#include <string>
using std::string;

#include <map>
#include <set>

#define TICKS_PER_SEC (1000000 / TIMER_RESOLUTION)

class app_timer;
class direct_app_timer;

class DirectAppTimer
{
public:
  virtual ~DirectAppTimer() {}
  virtual void fire()=0;
};

class _AmAppTimer 
  : public _wheeltimer 
{
  typedef std::map<int, app_timer*>   AppTimers;
  typedef std::map<string, AppTimers> TimerQueues;
  typedef std::map<DirectAppTimer*,direct_app_timer*> DirectTimers;

  AmMutex user_timers_mut;
  TimerQueues user_timers;

  AmMutex direct_timers_mut;
  DirectTimers direct_timers;

  /** creates timer object and inserts it into our container */
  app_timer* create_timer(const string& q_id, int id, unsigned int expires);
  /** erases timer - does not delete timer object @return timer object pointer, if found */
  app_timer* erase_timer(const string& q_id, int id);

  /* callback used by app_timer */
  void app_timer_cb(app_timer* at);
  friend class app_timer;

  /* callback used by direct_app_timer */
  void direct_app_timer_cb(direct_app_timer* t);
  friend class direct_app_timer;

 public:
  _AmAppTimer();
  ~_AmAppTimer();

  /** set a timer for event queue eventqueue_name with id timer_id and timeout (s) */
  void setTimer(const string& eventqueue_name, int timer_id, double timeout);
  /** remove timer for event queue eventqueue_name with id timer_id */
  void removeTimer(const string& eventqueue_name, int timer_id);
  /** remove all timers for event queue eventqueue_name */
  void removeTimers(const string& eventqueue_name);

  /* set a timer which directly calls your handler */
  void setTimer(DirectAppTimer* t, double timeout);
  /* remove a timer which directly calls your handler */
  void removeTimer(DirectAppTimer* t);

  /* ONLY use this from inside the timer handler of a direct timer */
  void setTimer_unsafe(DirectAppTimer* t, double timeout);
  /* ONLY use this from inside the timer handler of a direct timer */
  void removeTimer_unsafe(DirectAppTimer* t);
};

typedef singleton<_AmAppTimer> AmAppTimer;

#endif
