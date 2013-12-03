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

#include "AmEventQueueProcessor.h"
#include "AmEventQueue.h"

#include <deque>
using std::deque;

AmEventQueueProcessor::AmEventQueueProcessor()
{
  threads_it = threads.begin();
}

AmEventQueueProcessor::~AmEventQueueProcessor()
{
  threads_mut.lock();
  threads_it = threads.begin();
  while(threads_it != threads.end()) {
    (*threads_it)->stop();
    (*threads_it)->join();
    delete (*threads_it);
    threads_it++;
  }
  threads_mut.unlock();
}

EventQueueWorker* AmEventQueueProcessor::getWorker()
{
  threads_mut.lock();
  if (!threads.size()) {
    ERROR("requesting EventQueue processing thread but none available\n");
    threads_mut.unlock();
    return NULL;
  }

  // round robin
  if (threads_it == threads.end())
    threads_it = threads.begin();

  EventQueueWorker* res = *threads_it;
  threads_it++;
  threads_mut.unlock();

  return res;
}

int AmEventQueueProcessor::startEventQueue(AmEventQueue* q) 
{
  EventQueueWorker* worker = getWorker();
  if(!worker) return -1;

  worker->startEventQueue(q);
  return 0;
}

void AmEventQueueProcessor::addThreads(unsigned int num_threads) 
{
  DBG("starting %u session processor threads\n", num_threads);
  threads_mut.lock();
  for (unsigned int i=0; i < num_threads;i++) {
    threads.push_back(new EventQueueWorker());
    threads.back()->start();
  }
  threads_it = threads.begin();
  DBG("now %zd session processor threads running\n",  threads.size());
  threads_mut.unlock();
}


EventQueueWorker::EventQueueWorker() 
  : runcond(false)
{
}

EventQueueWorker::~EventQueueWorker() {
}

void EventQueueWorker::notify(AmEventQueue* sender) 
{
  process_queues_mut.lock();
  process_queues.push_back(sender);
  inc_ref(sender);
  runcond.set(true);
  process_queues_mut.unlock();
}

void EventQueueWorker::run()
{
  stop_requested = false;
  while(!stop_requested.get()){

    runcond.wait_for();

    DBG("running processing loop\n");
    process_queues_mut.lock();
    while(!process_queues.empty()) {

      AmEventQueue* ev_q = process_queues.front();
      process_queues.pop_front();
      process_queues_mut.unlock();

      if(!ev_q->processingCycle()) {
	ev_q->setEventNotificationSink(NULL);
	if(!ev_q->is_finalized())
	  ev_q->finalize();
      }
      dec_ref(ev_q);

      process_queues_mut.lock();
    }

    runcond.set(false);
    process_queues_mut.unlock();
  }
}

void EventQueueWorker::on_stop() 
{
  INFO("requesting worker to stop.\n");
  stop_requested.set(true);
  runcond.set(true);
}

void EventQueueWorker::startEventQueue(AmEventQueue* q) 
{
  if(q->startup())
    // register us to be notified if some event comes to the session
    q->setEventNotificationSink(this);
}
