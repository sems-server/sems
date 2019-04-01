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

#include "AmSessionContainer.h"

#include "AmAppTimer.h"
#include "log.h"

using std::map;

class app_timer : public timer 
{
  string q_id;
  int    timer_id;

 public:
  app_timer(const string& q_id, int timer_id, unsigned int expires)
    : timer(expires), q_id(q_id), timer_id(timer_id) {}

  ~app_timer() {}

  int get_id() { return timer_id; }
  string get_q_id() { return q_id; }

  // timer interface
  void fire() {
    AmAppTimer::instance()->app_timer_cb(this);
  }
};

class direct_app_timer
  : public timer
{
public:
  DirectAppTimer* dt;
  
  direct_app_timer(DirectAppTimer* dt, unsigned int expires)
    : timer(expires), dt(dt) {}

  ~direct_app_timer() {}

  void fire() {
    AmAppTimer::instance()->direct_app_timer_cb(this);
  }
};

_AmAppTimer::_AmAppTimer()
  : direct_timers_mut(true)
{
}

_AmAppTimer::~_AmAppTimer() {
}

void _AmAppTimer::app_timer_cb(app_timer* at)
{
  user_timers_mut.lock();
  app_timer* at_local = erase_timer(at->get_q_id(), at->get_id());

  if (NULL != at_local) {

    if (at_local != at) {
      DBG("timer was reset while expiring - not firing timer\n");
      // we'd better re-insert at_local into user_timers
      // what happens else, when at_local get fired ???
      user_timers[at->get_q_id()][at->get_id()] = at_local;
    } else {
      DBG("timer fired: %d for '%s'\n", at->get_id(), at->get_q_id().c_str());
      AmSessionContainer::instance()->postEvent(at->get_q_id(),
						new AmTimeoutEvent(at->get_id()));
      delete at;
    }

  } else {
    DBG("timer %d for '%s' already removed\n",
	at->get_id(), at->get_q_id().c_str());
    // will be deleted by wheeltimer
  }

  user_timers_mut.unlock();
}

void _AmAppTimer::direct_app_timer_cb(direct_app_timer* t)
{
  DirectAppTimer* dt = t->dt;

  direct_timers_mut.lock();
  DirectTimers::iterator dt_it = direct_timers.find(dt);
  if(dt_it != direct_timers.end()){
    if(dt_it->second != t) {
      // timer has been re-initialized
      // with the same pointer... do not trigger!
      // it will be deleted later by the wheeltimer
    }
    else {
      // everything ok:

      // remove stuff
      direct_timers.erase(dt_it);
      delete t;

      // finally fire this timer!
      dt->fire();
    }
  }
  direct_timers_mut.unlock();
}

app_timer* _AmAppTimer::erase_timer(const string& q_id, int id) 
{
  app_timer* res = NULL;

  map<string, map<int, app_timer*> >::iterator it=user_timers.find(q_id);
  if (it != user_timers.end()) {
    map<int, app_timer*>::iterator t_it = it->second.find(id);
    if (t_it != it->second.end()) {
      res = t_it->second;
      it->second.erase(t_it);
      if (it->second.empty())
	user_timers.erase(it);
    }
  }

  return res;
}

app_timer* _AmAppTimer::create_timer(const string& q_id, int id, 
				     unsigned int expires) 
{
  app_timer* timer = new app_timer(q_id, id, expires);
  if (!timer)
    return NULL;

  user_timers[q_id][id] = timer;

  return timer;
}

#define MAX_TIMER_SECONDS 365*24*3600 // one year, well below 1<<31

void _AmAppTimer::setTimer(const string& eventqueue_name, int timer_id, double timeout) {

  // microseconds
  unsigned int expires;
  if (timeout < 0) { // in the past
    expires = 0;
  } else if (timeout > MAX_TIMER_SECONDS) { // more than one year
    ERROR("Application requesting timer %d for '%s' with timeout %f, "
	  "clipped to maximum of one year\n", timer_id, eventqueue_name.c_str(), timeout);
    expires = (double)MAX_TIMER_SECONDS*1000.0*1000.0 / (double)TIMER_RESOLUTION;
  } else {
    expires = timeout*1000.0*1000.0 / (double)TIMER_RESOLUTION;
  }

  expires += wall_clock;

  user_timers_mut.lock();
  app_timer* t = erase_timer(eventqueue_name, timer_id);
  if (NULL != t) {
    remove_timer(t);
  }
  t = create_timer(eventqueue_name, timer_id, expires);
  if (NULL != t) {
    insert_timer(t);
  }
  user_timers_mut.unlock();
}

void _AmAppTimer::removeTimer(const string& eventqueue_name, int timer_id) 
{
  user_timers_mut.lock();
  app_timer* t = erase_timer(eventqueue_name, timer_id);
  if (NULL != t) {
    remove_timer(t);
  }
  user_timers_mut.unlock();
}

void _AmAppTimer::removeTimers(const string& eventqueue_name) 
{
  user_timers_mut.lock();
  TimerQueues::iterator it=user_timers.find(eventqueue_name);
  if (it != user_timers.end()) {
    for (AppTimers::iterator t_it =
	   it->second.begin(); t_it != it->second.end(); t_it++) {
      if (NULL != t_it->second)
	remove_timer(t_it->second);
    }
    user_timers.erase(it);
  }
  user_timers_mut.unlock();
}

void _AmAppTimer::setTimer_unsafe(DirectAppTimer* t, double timeout)
{
  unsigned int expires = timeout*1000.0*1000.0 / (double)TIMER_RESOLUTION;
  expires += wall_clock;

  direct_app_timer* dt = new direct_app_timer(t,expires);
  if(!dt) return;

  DirectTimers::iterator dt_it = direct_timers.find(t);
  if(dt_it != direct_timers.end()){
    remove_timer(dt_it->second);
    dt_it->second = dt;
  }
  else {
    direct_timers[t] = dt;
  }
  insert_timer(dt);
}

void _AmAppTimer::setTimer(DirectAppTimer* t, double timeout)
{
  direct_timers_mut.lock();
  setTimer_unsafe(t,timeout);
  direct_timers_mut.unlock();
}

void _AmAppTimer::removeTimer_unsafe(DirectAppTimer* t)
{
  DirectTimers::iterator dt_it = direct_timers.find(t);
  if(dt_it != direct_timers.end()){
    remove_timer(dt_it->second);
    direct_timers.erase(dt_it);
  }
}

void _AmAppTimer::removeTimer(DirectAppTimer* t)
{
  direct_timers_mut.lock();
  removeTimer_unsafe(t);
  direct_timers_mut.unlock();
}
