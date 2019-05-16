/*
 *  (c) 2007 iptego GmbH
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
/** @file AmCallWatcher.h */
#ifndef _AM_CALL_WATCHER_H
#define _AM_CALL_WATCHER_H

//
// States are put into map on an Initialize event.
// States are held in a map identified by call_id
// (opaque identifier) and updated with Update events.
// Once an Obsolete event is received, the states are 
// moved to soft-state map, where they are held until
// queried to a maximum of WATCHER_SOFT_EXPIRE_SECONDS

#define WATCHER_SOFT_EXPIRE_SECONDS  5

#include <string>
using std::string;

#include <map>

#include <utility>
using std::pair;

#include "AmEventQueue.h"
#include "AmEvent.h"
#include "AmThread.h"

class CallStatus;

/** 
 * \brief event that carries out call status update
 */
class CallStatusUpdateEvent : public AmEvent {
  string call_id;

  CallStatus* init_status;

 public:
  enum UpdateType {
    Initialize = 0,
    Update,
    Obsolete
  };

  CallStatusUpdateEvent(UpdateType t, const string& call_id)
    : AmEvent(t), call_id(call_id)  { }

  // implicit: initialize
  CallStatusUpdateEvent(const string& call_id, CallStatus* init_status)
    : AmEvent(Initialize), call_id(call_id), init_status(init_status)  { }

  ~CallStatusUpdateEvent() { }

  string get_call_id() { return call_id; }
  CallStatus* get_init_status() { return init_status; }
};

/** 
 * \brief interface for an update-able call status (AmCallWatcher)
 */
class CallStatus
{
 public:
  CallStatus()  { }
  virtual ~CallStatus() { }

  /** update from an event */
  virtual void update(CallStatusUpdateEvent* e) = 0;

  /** get a copy of self with relevant data */
  virtual CallStatus* copy() = 0;
  virtual void dump() { }
};

class AmCallWatcherGarbageCollector;
/**
 * \brief manages call status to be queried by external processes 
 * call watcher is an entity for managing call status
 * via events that change status. Events are executed in a 
 * separate thread serially by processing the event queue, 
 * so synchronous status queries do not block the thread 
 * reporting the status change.
 */
class AmCallWatcher
: public AmThread, 
  public AmEventQueue,
  public AmEventHandler
{
 public:
  typedef std::map<string, CallStatus*> CallStatusMap;
  typedef std::map<string, pair<CallStatus*, time_t> > CallStatusTimedMap;

 private:
  CallStatusMap states;
  AmMutex       states_mut;


  CallStatusTimedMap soft_states;
  AmMutex            soft_states_mut;
  AmCallWatcherGarbageCollector* garbage_collector;

 public:
  AmCallWatcher();
  ~AmCallWatcher();

  // thread
  void run();
  void on_stop();

  // eventhandler
  void process(AmEvent*);

  CallStatus* getStatus(const string& call_id);

  // dump all states
  void dump();
};

/** 
 * \brief garbage collector for the AmCallWatcher
 * 
 * checks garbage every two seconds. 
 * A bit inefficient with two threads, but AmCallWatcher
 * shouldn't be blocked by event.
 */
class AmCallWatcherGarbageCollector 
: public AmThread
{
  AmMutex& mut;
  AmCallWatcher::CallStatusTimedMap& garbage;
 public: 
  AmCallWatcherGarbageCollector(AmMutex& mut,
				AmCallWatcher::CallStatusTimedMap& garbage)
    : mut(mut), garbage(garbage) {}
  void run();
  void on_stop() { }
};

#endif
