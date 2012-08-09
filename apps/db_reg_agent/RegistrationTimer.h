/*
 * Copyright (C) 2011 Stefan Sayer
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

#ifndef _RegistrationTimer_h_
#define _RegistrationTimer_h_

#include <list>
#include <vector>

#include <sys/time.h>

#include "log.h"
#include "AmThread.h"

#define TIMER_BUCKET_LENGTH 10     // 10 sec
#define TIMER_BUCKETS       40000  // 40000 buckets (400000 sec, 111 hrs)

// 100 ms == 100000 us
#define TIMER_RESOLUTION 100000

class RegTimer;
typedef void (*timer_cb)(RegTimer*, long /*data1*/,int /*data2*/);

class RegTimerBucket;

class RegTimer {
 public:
    time_t expires;

    timer_cb       cb;
    long           data1;
    int            data2;

    RegTimer()
      : expires(0), cb(0), data1(0), data2(0) { }
};

class RegTimerBucket {
 public:
  std::list<RegTimer*> timers;

 RegTimerBucket() { }
};

/**
  Additionally to normal timer operation (setting and removing timer,
  fire the timer when it is expired), this RegistrationTimer timers 
  class needs to support insert_timer_leastloaded() which should insert 
  the timer in some least loaded interval between from_time and to_time
  in order to flatten out re-register spikes (due to restart etc).

  Timer granularity is seconds.

  Timers are saved in buckets of TIMER_BUCKET_LENGTH seconds. the buckets
  array is a circular one, the current bucket starts from the time 
  current_bucket_start (in seconds as in time(2)).

  The timer object is owned by the caller, and MUST be valid until it is
  fired or removed.
 */

class RegistrationTimer
: public AmThread
{
  time_t current_bucket_start;
  // every bucket contains TIMER_BUCKET_LENGTH seconds of timers
  RegTimerBucket buckets[TIMER_BUCKETS];
  unsigned int current_bucket;
  AmMutex buckets_mut;

  int get_bucket_index(time_t tv);
  void place_timer(RegTimer* timer, int bucket_index);
  void fire_timer(RegTimer* timer);
  void run_timers();

 protected:
  void run();
  void on_stop();

 public:
  bool insert_timer(RegTimer* timer);
  bool remove_timer(RegTimer* timer);

  bool insert_timer_leastloaded(RegTimer* timer,
				time_t from_time,
				time_t to_time);

  RegistrationTimer(); 
  bool _timer_thread_running;
  bool _shutdown_finished;
};

#endif
