/*
 * $Id: AmSessionProcessor.h 1585 2009-10-28 22:31:08Z sayer $
 *
 * Copyright (C) 2010 Stefan Sayer
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

#ifdef SESSION_THREADPOOL

#ifndef _AmSessionProcessor_h_
#define _AmSessionProcessor_h_

#include "AmThread.h"
#include "AmEventQueue.h"

#include <vector>
#include <list>
#include <set>
class AmSessionProcessorThread;
class AmSession;

class AmSessionProcessor {
  static vector<AmSessionProcessorThread*> threads;
  static AmMutex threads_mut;
  static vector<AmSessionProcessorThread*>::iterator 
    threads_it;

 public: 
  static AmSessionProcessorThread* getProcessorThread();
  static void addThreads(unsigned int num_threads);
};

struct AmSessionProcessorThreadAddEvent 
  : AmEvent
{
  AmSession* s;
  AmSessionProcessorThreadAddEvent(AmSession* s)
    : s(s), AmEvent(120) { }
};

class AmSessionProcessorThread 
: public AmThread,
  public AmEventHandler,
  public AmEventNotificationSink
{
  AmEventQueue    events;
  std::list<AmSession*> sessions;
  std::vector<AmSession*> startup_sessions;
  AmSharedVar<bool> stop_requested;

  AmCondition<bool> runcond;
  std::set<AmEventQueue*> process_sessions;
  AmMutex process_sessions_mut;

  // AmEventHandler interface
  void process(AmEvent* e);

 public:
  AmSessionProcessorThread();
  ~AmSessionProcessorThread();

  // AmThread interface
  void run();
  void on_stop();

  // AmEventNotificationSink interface
  void notify(AmEventQueue* sender);

  void startSession(AmSession* s);
};

#endif // _AmSessionProcessor_h_

#endif // #ifdef SESSION_THREADPOOL
