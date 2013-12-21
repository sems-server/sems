/*
 * Copyright (C) 2002-2003 Fhg Fokus
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
/** @file AmSessionContainer.h */
#ifndef AmSessionContainer_h
#define AmSessionContainer_h

#include "AmThread.h"
#include "AmSession.h"

#include "ampi/MonitoringAPI.h"

#include <string>
#include <queue>
#include <map>

using std::string;

/**
 * \brief Centralized session container.
 *
 * This is the register for all active and dead sessions.
 * If has a deamon which wakes up only if it has work. 
 * Then, it kills all dead sessions and try to go to bed 
 * (it cannot sleep if one or more sessions are still alive).
 */
class AmSessionContainer : public AmThread
{
  static AmSessionContainer* _instance;

  typedef std::queue<AmSession*>      SessionQueue;

  /** Container for dead sessions */
  SessionQueue d_sessions;
  /** Mutex to protect the dead session container */
  AmMutex      ds_mut;

  /** is container closed for new sessions? */
  AmCondition<bool> _container_closed;

  /** the daemon only runs if this is true */
  AmCondition<bool> _run_cond;

  /** We are a Singleton ! Avoid people to have their own instance. */
  AmSessionContainer();


  bool enable_unclean_shutdown;

  /**
   * Tries to stop the session and queue it destruction.
   */
  void stopAndQueue(AmSession* s);

  /** @see AmThread::run() */
  void run();
  /** @see AmThread::on_stop() */
  void on_stop();

  bool clean_sessions();

  typedef std::queue<struct timeval> TimevalQueue;

  /** Container for cps timevals*/
  TimevalQueue cps_queue;
  /** Maximum cps since the lasd getMaxCPS()*/
  unsigned int max_cps;
  /** Mutex to protect the cps container */
  AmMutex      cps_mut;

  enum { CPS_SAMPLERATE = 5 };

  unsigned int CPSLimit;
  unsigned int CPSHardLimit;

  bool check_and_add_cps();

 public:
  static AmSessionContainer* instance();

  static void dispose();

  enum AddSessionStatus {
    ShutDown,
    Inserted,
    AlreadyExist
  };

  /**
   * Creates a new session.
   * @param req local request
   * @return a new session or NULL on error.
   */
  AmSession* createSession(const AmSipRequest& req, 
			   string& app_name,
			   AmArg* session_params = NULL);

  /**
   * Adds a session to the container (UAS only).
   * @return true if the session is new within the container.
   */
  AddSessionStatus addSession(const string& callid,
			      const string& remote_tag,
			      const string& local_tag,
			      const string& via_branch,
			      AmSession* session);

  /**
   * Adds a session to the container.
   * @return true if the session is new within the container.
   */
  AddSessionStatus addSession(const string& local_tag,
			      AmSession* session);

  /** 
   * Constructs a new session and adds it to the active session container. 
   * @param req client's request
   */
  void startSessionUAS(AmSipRequest& req);

  /** 
   * Constructs a new session and adds it to the active session container. 
   * @param req client's request
   */
  string startSessionUAC(const AmSipRequest& req, 
			 string& app_name,
			 AmArg* session_params = NULL);

  /**
   * Detroys a session.
   */
  void destroySession(AmSession* s);

  /**
   * post an event into the event queue of the identified dialog.
   * @return false if session doesn't exist 
   */
  bool postEvent(const string& callid, 
		 const string& remote_tag,
		 const string& via_branch,
		 AmEvent* event);

  /**
   * post a generic event into the event queue of the identified dialog.
   * sess_key is local_tag (to_tag)
   * note: if hash_str is known, use 
   *          postGenericEvent(hash_str,sess_key,event);
   *       for better performance.
   * @return false if session doesn't exist 
   */
  bool postEvent(const string& local_tag, AmEvent* event);

  /** 
   * broadcasts a server shutdown system event to all sessions
   */
  void  broadcastShutdown();

  /** enable unclean shutdown (will not broadcastShutdown event) */
  void enableUncleanShutdown();

  /** Set the maximum number of calls per second to be accepted */
  void setCPSLimit(unsigned int limit);

  /** Set the maximum number of calls per second to be accepted as a percent 
   * of the current CPS. Intented to be used by the components. 0 means turning off
   * the soft limit.
   */
  void setCPSSoftLimit(unsigned int percent);

  /** Return the maximum number of calls per second to be accepted */
  pair<unsigned int, unsigned int> getCPSLimit();

  /**
   * Gets the timeaverage of calls per second in the last CPS_SAMPLERATE sec window
   */
  unsigned int getAvgCPS();
  /**
   * Gets the maximum of calls per second since last query
   */
  unsigned int getMaxCPS();

  void initMonitoring();

  _MONITORING_DEFINE_INTERFACE;

};

#endif
