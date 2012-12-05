/*
 * Copyright (C) 2012 Frafos GmbH
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
/** @file AmPeriodicThread.cpp */

#include "AmPeriodicThread.h"
#include <assert.h>

#include "log.h"

void AmPeriodicThread::infinite_loop(struct timeval* tick, unsigned int max_ticks_behind, void* usr_data)
{
  struct timeval now,next_tick,diff;
  unsigned int ticks_passed;
  unsigned long ms_diff, ms_tick;

  assert(tick);
  ms_tick = tick->tv_sec * 1000 + tick->tv_usec / 1000;

  gettimeofday(&next_tick,NULL);

  while (true) {

    gettimeofday(&now,NULL);
    timeradd(tick,&next_tick,&next_tick);

    if(timercmp(&now,&next_tick,<)){

      struct timespec sdiff,rem;
      timersub(&next_tick,&now,&diff);

      // detect backward clockdrift
      ms_diff = diff.tv_sec * 1000 + diff.tv_usec / 1000;
      if(ms_diff / ms_tick > 1) {
	// at least 2 ticks ahead...
	next_tick = now;
	sdiff.tv_sec  = tick->tv_sec;
	sdiff.tv_nsec = tick->tv_usec * 1000;

	WARN("clock drift backwards detected (%lu ticks ahead), "
	     "resetting sw clock\n", ms_diff / ms_tick - 1);
      }
      else {
	// everything ok
	sdiff.tv_sec  = diff.tv_sec;
	sdiff.tv_nsec = diff.tv_usec * 1000;
      }

      if(sdiff.tv_nsec > 2000000) // 2 ms
	nanosleep(&sdiff,&rem);
      
      ticks_passed = 1;
    }
    else {
      // compute missed ticks
      unsigned long ms_diff;
      timersub(&now,&next_tick,&diff);
      ms_diff = diff.tv_sec * 1000 + diff.tv_usec / 1000;
      ticks_passed = ms_diff / ms_tick + 1;

      // missed too many ticks: resync
      if(ticks_passed > max_ticks_behind) {
	// resync to clock if clock is farther than max_behind_tick in the future
	WARN("clock drift detected (missed %d ticks), resetting sw clock\n",
	     ticks_passed);
	next_tick = now;
      }
    }

    // execute looping step code
    if(!looping_step(usr_data))
      break;
  }
}
