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

#ifndef _AmEventProcessingThread_h_
#define _AmEventProcessingThread_h_

#include "AmThread.h"
#include "AmEventQueue.h"
#include "AmEvent.h"


/**
   AmEventProcessingThread processes events posted
   in the event queue until either server shutdown
   is signaled or processing is requested to stop
   with the stop_processing function.

   Override the onEvent(AmEvent* ev) method, create
   an object, start() it and post events to it.

   If you need queue policing (e.g. overflow protection),
   override police_event() function.
 */

class AmEventProcessingThread
: public AmThread,
  public AmEventQueue,
  public AmEventHandler
{
  
  bool processing_events;

  void process(AmEvent* ev);

 protected:
  void run();
  void on_stop();

  virtual void onEvent(AmEvent* ev) { }

  virtual bool police_event(AmEvent* ev);

 public:
  AmEventProcessingThread();
  ~AmEventProcessingThread();

  void postEvent(AmEvent* ev);

  void stop_processing();

};

#endif
