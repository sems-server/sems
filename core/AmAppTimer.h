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

class app_timer;

class _AmAppTimer 
  : public _wheeltimer 
{
  typedef std::map<int, app_timer*>   AppTimers;
  typedef std::map<string, AppTimers> TimerQueues;

  AmMutex user_timers_mut;
  TimerQueues user_timers;

  /** creates timer object and inserts it into our container */
  app_timer* create_timer(const string& q_id, int id, unsigned int expires);
  /** erases timer - does not delete timer object @return timer object pointer, if found */
  app_timer* erase_timer(const string& q_id, int id);

  /* callback used by app_timer */
  void app_timer_cb(app_timer* at);
  friend class app_timer;

 public:
  _AmAppTimer();
  ~_AmAppTimer();

  /** set a timer for event queue eventqueue_name with id timer_id and timeout (s) */
  void setTimer(const string& eventqueue_name, int timer_id, double timeout);
  /** remove timer for event queue eventqueue_name with id timer_id */
  void removeTimer(const string& eventqueue_name, int timer_id);
  /** remove all timers for event queue eventqueue_name */
  void removeTimers(const string& eventqueue_name);

};

typedef singleton<_AmAppTimer> AmAppTimer;

#endif
