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
/** @file AmPeriodicThread.h */
#ifndef _AmPeriodicThread_h_
#define _AmPeriodicThread_h_

#include "AmThread.h"

class AmPeriodicThread: public AmThread
{
protected:
  AmPeriodicThread() {}
  virtual ~AmPeriodicThread() {}

  /*
   * Start the infinite loop. The loop will
   * do its best to execute looping_step() at regular
   * intervals defined by 'tick'. The time spent in looping_step()
   * is subtracted from the 'tick' to calculate the next execution
   * time.
   *
   * @param tick       execution time interval.
   * @param max_ticks_behind maximum forward clock drift in ticks.
   * @param usr_data   pointer that will be passed to looping_step().
   */
  void infinite_loop(struct timeval* tick,
		     unsigned int max_ticks_behind,
		     void* usr_data);

  /*
   * This method is executed periodically by
   * infinite loop.
   * @return true to continue the loop, false to stop it.
   */
  virtual bool looping_step(void* usr_data)=0;
};

#endif
