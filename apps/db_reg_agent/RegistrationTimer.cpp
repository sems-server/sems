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

#include "RegistrationTimer.h"
#include <stdlib.h>

RegistrationTimer::RegistrationTimer()
 : current_bucket(0)
{
  struct timeval now;
  gettimeofday(&now, 0);
  current_bucket_start = now.tv_sec; 
}

// unsafe!
int RegistrationTimer::get_bucket_index(time_t tv) {
  time_t buckets_start_time = current_bucket_start;

  if (tv < buckets_start_time)
    return -1;

  // offset
  int bucket_index =  (tv - buckets_start_time);
  bucket_index /= TIMER_BUCKET_LENGTH;
  
  if (bucket_index > TIMER_BUCKETS) { // too far in the future
    ERROR("requested timer too far in the future (index %d vs %d TIMER_BUCKETS)\n",
	  bucket_index, TIMER_BUCKETS);
    return -2;
  }

  bucket_index += current_bucket;
  bucket_index %= TIMER_BUCKETS; // circular array

  return bucket_index;
}

void RegistrationTimer::place_timer(RegTimer* timer, int bucket_index) {
  if (bucket_index < 0) {
    ERROR("trying to place_timer with negative index (%i)\n", bucket_index);
    return;
  }

  if (bucket_index > TIMER_BUCKETS) {
    ERROR("trying to place_timer with too high index (%i vs %i)\n",
	  bucket_index, TIMER_BUCKETS);
    return;
  }

  std::list<RegTimer*>::iterator it = buckets[bucket_index].timers.begin();
  while (it != buckets[bucket_index].timers.end() &&
	 (timer->expires > (*it)->expires))
    it++;
  
  buckets[bucket_index].timers.insert(it, timer);
  size_t b_size = buckets[bucket_index].timers.size();
 
 DBG("inserted timer [%p] in bucket %i (now sized %zd)\n",
      timer, bucket_index, b_size);
}

void RegistrationTimer::fire_timer(RegTimer* timer) {
  if (timer && timer->cb) {
    DBG("firing timer [%p]\n", timer);
    timer->cb(timer, timer->data1, timer->data2);
  }
}

bool RegistrationTimer::insert_timer(RegTimer* timer) {
  if (!timer)
    return false;

  buckets_mut.lock();
  int bucket_index = get_bucket_index(timer->expires);

  if (bucket_index == -1) {
    // already expired, fire timer
    buckets_mut.unlock();
    DBG("inserting already expired timer [%p], firing\n", timer);
    fire_timer(timer);
    return false;
  }

  if (bucket_index == -2) {
    ERROR("trying to place timer too far in the future\n");
    buckets_mut.unlock();
    return false;
  }

  place_timer(timer, bucket_index);

  buckets_mut.unlock();

 return true;
}

bool RegistrationTimer::remove_timer(RegTimer* timer) {
  if (!timer)
    return false;

  bool res = false;

  buckets_mut.lock();
  int bucket_index = get_bucket_index(timer->expires);

  if (bucket_index < 0) {
    buckets_mut.unlock();
    return false;
  }

  std::list<RegTimer*>& timerlist = buckets[bucket_index].timers;

  for (std::list<RegTimer*>::iterator it = timerlist.begin();
       it != timerlist.end(); it++) {
    if (*it == timer) {
      timerlist.erase(it);
      res = true;
      break;
    }
  }

  buckets_mut.unlock();  

  if (res) {
    DBG("successfully removed timer [%p]\n", timer);
  } else {
    DBG("timer [%p] not found for removing\n", timer);
  }
  return res;
}

void RegistrationTimer::run_timers() {
  std::list<RegTimer*> timers_tbf;

  struct timeval now;
  gettimeofday(&now, 0);

  buckets_mut.lock();

  // bucket over?
  if (now.tv_sec > current_bucket_start + TIMER_BUCKET_LENGTH) {
    timers_tbf.insert(timers_tbf.begin(),
		      buckets[current_bucket].timers.begin(),
		      buckets[current_bucket].timers.end());
    buckets[current_bucket].timers.clear();
    current_bucket++;
    current_bucket %= TIMER_BUCKETS;
    current_bucket_start += TIMER_BUCKET_LENGTH;
    // DBG("turned bucket to %i\n", current_bucket);
  }

  // move timers from current_bucket
  RegTimerBucket& bucket = buckets[current_bucket];
  std::list<RegTimer*>::iterator it = bucket.timers.begin();
  while (it != bucket.timers.end() &&
	 now.tv_sec > (*it)->expires) {
    std::list<RegTimer*>::iterator c_it = it;
    it++;
    timers_tbf.push_back(*c_it);
    bucket.timers.erase(c_it);
  }

  buckets_mut.unlock();

  if (!timers_tbf.empty()) {
    DBG("firing %zd timers\n", timers_tbf.size());
    for (std::list<RegTimer*>::iterator it=timers_tbf.begin();
	 it != timers_tbf.end(); it++) {
      fire_timer(*it);
    }
  }
}

void RegistrationTimer::run()
{
  struct timeval now,next_tick,diff,tick;
  _shutdown_finished = false;

  tick.tv_sec = 0;
  tick.tv_usec = TIMER_RESOLUTION;
  
  gettimeofday(&now, NULL);
  timeradd(&tick,&now,&next_tick);

  _timer_thread_running = true;

  while(_timer_thread_running){

    gettimeofday(&now,NULL);

    if(timercmp(&now,&next_tick,<)){

      struct timespec sdiff,rem;
      timersub(&next_tick, &now,&diff);
      
      sdiff.tv_sec = diff.tv_sec;
      sdiff.tv_nsec = diff.tv_usec * 1000;

      if(sdiff.tv_nsec > 2000000) // 2 ms 
	nanosleep(&sdiff,&rem);
    }
    //else {
    //printf("missed one tick\n");
    //}

    run_timers();
    timeradd(&tick,&next_tick,&next_tick);
  }

  DBG("RegistrationTimer thread finishing.\n");
  _shutdown_finished = true;
}

void RegistrationTimer::on_stop() {
}

bool RegistrationTimer::insert_timer_leastloaded(RegTimer* timer,
						 time_t from_time,
						 time_t to_time) {

  buckets_mut.lock();

  int from_index = get_bucket_index(from_time);
  int to_index = get_bucket_index(to_time);

  if (from_index < 0 && to_index < 0) {
    ERROR("could not find timer bucket indices - "
	  "from_index = %d, to_index = %d, from_time = %ld, to_time %ld, "
	  "current_bucket_start = %ld\n",
	  from_index, to_index, from_time, to_time, current_bucket_start);
    buckets_mut.unlock();
    return false;
  }

  if (from_index < 0) {
    // use now .. to_index
    DBG("from_time (%ld) in the past - searching load loaded from now()\n", from_time);
    from_index  = current_bucket;
  }
  // find least loaded bucket
  int res_index = from_index;
  size_t least_load = buckets[from_index].timers.size();

  int i = from_index;
  while  (i != to_index) {
    if (buckets[i].timers.size() <= least_load) {
      least_load = buckets[i].timers.size();
      res_index = i;
    }

    i++;
    i %= TIMER_BUCKETS;
  }
  DBG("found bucket %i with least load %zd (between %i and %i)\n",
      res_index, least_load, from_index, to_index);

  // update expires to some random value inside the selected bucket
  int diff = (unsigned)res_index - current_bucket;

  if ((unsigned)res_index < current_bucket) {
    diff+=TIMER_BUCKETS;
  }
  
  timer->expires = current_bucket_start + 
    diff * TIMER_BUCKET_LENGTH + // bucket start
    rand() % TIMER_BUCKET_LENGTH;
  DBG("setting expires to %ld (between %ld and %ld)\n",
      timer->expires, from_time, to_time);

  place_timer(timer, res_index);
     
  buckets_mut.unlock();

  return false;
}




