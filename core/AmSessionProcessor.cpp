/*
 * $Id: AmSessionProcessor.cpp 1585 2009-10-28 22:31:08Z sayer $
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

#include "AmSessionProcessor.h"
#include "AmSession.h"

#include <vector>
#include <list>

vector<AmSessionProcessorThread*> AmSessionProcessor::threads;
AmMutex AmSessionProcessor::threads_mut;

vector<AmSessionProcessorThread*>::iterator 
AmSessionProcessor::threads_it = AmSessionProcessor::threads.begin();

AmSessionProcessorThread* AmSessionProcessor::getProcessorThread() {
  threads_mut.lock();
  if (!threads.size()) {
    ERROR("requesting Session processing thread but none available\n");
    threads_mut.unlock();
    return NULL;
  }

  // round robin
  if (threads_it == threads.end())
    threads_it = threads.begin();

  AmSessionProcessorThread* res = *threads_it;
  threads_it++;
  threads_mut.unlock();
  return res;
}

void AmSessionProcessor::addThreads(unsigned int num_threads) {
  DBG("starting %u session processor threads\n", num_threads);
  threads_mut.lock();
  for (unsigned int i=0; i < num_threads;i++) {
    threads.push_back(new AmSessionProcessorThread());
    threads.back()->start();
  }
  threads_it = threads.begin();
  DBG("now %zd session processor threads running\n",  threads.size());
  threads_mut.unlock();
}


AmSessionProcessorThread::AmSessionProcessorThread() 
  : events(this), runcond(false)
{
}

AmSessionProcessorThread::~AmSessionProcessorThread() {
}

void AmSessionProcessorThread::notify(AmEventQueue* sender) {
  process_sessions_mut.lock();
  runcond.set(true);
  process_sessions.insert(sender);
  process_sessions_mut.unlock();
}

void AmSessionProcessorThread::run() {

  stop_requested = false;
  while(!stop_requested.get()){

    runcond.wait_for();

    DBG("running processing loop\n");

    process_sessions_mut.lock();
    runcond.set(false);
    // get the list of session s that need processing
    std::set<AmEventQueue*> pending_process_sessions 
      = process_sessions;
    process_sessions.clear();
    process_sessions_mut.unlock();

    // process control events (AmSessionProcessorThreadAddEvent)
    events.processEvents();

    // startup all new sessions
    if (!startup_sessions.empty()) {
      DBG("starting up %zd sessions\n", startup_sessions.size());

      for (std::vector<AmSession*>::iterator it=
	     startup_sessions.begin(); 
	   it != startup_sessions.end(); it++) {
	
	DBG("starting up [%s|%s]: [%p]\n",
	    (*it)->getCallID().c_str(), (*it)->getLocalTag().c_str(),*it);
	if ((*it)->startup()) {
	  sessions.push_back(*it); // startup successful
	  // make sure this session is being processed for startup events
	  pending_process_sessions.insert(*it);
	}
      }

      startup_sessions.clear();
    }

    std::vector<AmSession*> fin_sessions;

    DBG("processing events for  up to %zd sessions\n",
	pending_process_sessions.size());

    std::list<AmSession*>::iterator it=sessions.begin();
    while (it != sessions.end()) {
      if ((pending_process_sessions.find(*it)!=
	      pending_process_sessions.end()) &&
	  (!(*it)->processingCycle())) {
	fin_sessions.push_back(*it);
	std::list<AmSession*>::iterator d_it = it;
	it++;
	sessions.erase(d_it);
      } else {
	it++;
      }
    }

    if (fin_sessions.size()) {
      DBG("finalizing %zd sessions\n", fin_sessions.size());
      for (std::vector<AmSession*>::iterator it=fin_sessions.begin(); 
	   it != fin_sessions.end(); it++) {
	DBG("finalizing session [%p/%s/%s]\n",
	    *it, (*it)->getCallID().c_str(), (*it)->getLocalTag().c_str());
	(*it)->finalize();
      }
    }
  }
}

void AmSessionProcessorThread::on_stop() {
  INFO("requesting session to stop.\n");
  stop_requested.set(true);
}

// AmEventHandler interface
void AmSessionProcessorThread::process(AmEvent* e) {
  AmSessionProcessorThreadAddEvent* add_ev = 
    dynamic_cast<AmSessionProcessorThreadAddEvent*>(e);

  if (NULL==add_ev) {
    ERROR("received wrong event in AmSessionProcessorThread\n");
    return;
  }

  startup_sessions.push_back(add_ev->s);
}

void AmSessionProcessorThread::startSession(AmSession* s) {
  // register us to be notified if some event comes to the session
  s->setEventNotificationSink(this);

  // add this to be scheduled
  events.postEvent(new AmSessionProcessorThreadAddEvent(s));

  // trigger processing of events already in queue at startup
  notify(s);

  // wakeup the thread
  runcond.set(true);
}

#endif
