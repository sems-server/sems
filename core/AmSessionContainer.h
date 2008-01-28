/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
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
  static AmSessionContainer* _SessionContainer;

  // some typedefs ...
  typedef std::map<string,AmSession*> SessionMap;
  typedef SessionMap::iterator SessionMapIter;

  typedef std::map<string,string>     Dictionnary;
  typedef Dictionnary::iterator  DictIter;

  typedef std::queue<AmSession*>      SessionQueue;

  /** 
   * Container for active sessions 
   * local tag -> session
   */
  SessionMap a_sessions;

  /** 
   * Call ID + remote tag -> local tag 
   *  (needed for CANCELs and some provisionnal answers)
   *  (UAS sessions only)
   */
  Dictionnary as_id_lookup;

  /** Mutex to protect the active session container */
  AmMutex    as_mut;


  /** Container for dead sessions */
  SessionQueue d_sessions;
  /** Mutex to protect the dead session container */
  AmMutex      ds_mut;

  /** the daemon only runs if this is true */
  AmCondition<bool> _run_cond;

  /** We are a Singleton ! Avoid people to have their own instance. */
  AmSessionContainer();

  /**
   * Search the container for a session coresponding 
   * to callid and remote_tag. (UAS only).
   *
   * @return the session related to callid & remote_tag
   *         or NULL if none has been found.
   */
  AmSession* getSession(const string& callid, const string& remote_tag);

  /**
   * Search the container for a session coresponding to local_tag.
   *
   * @return the session related to local_tag 
   *         or NULL if none has been found.
   */
  AmSession* getSession(const string& local_tag);

  /**
   * Adds a session to the container. (UAS only)
   * @return true if the session is new within the container.
   */
  bool addSession_unsafe(const string& callid, 
			 const string& remote_tag,
			 const string& local_tag,
			 AmSession* session);

  /**
   * Adds a session to the container.
   * @return true if the session is new within the container.
   */
  bool addSession_unsafe(const string& local_tag,
			 AmSession* session);

  /**
   * Tries to stop the session and queue it destruction.
   */
  void stopAndQueue(AmSession* s);

  /** @see AmThread::run() */
  void run();
  /** @see AmThread::on_stop() */
  void on_stop();

 public:
  static AmSessionContainer* instance();

  /**
   * Creates a new session.
   * @param req local request
   * @return a new session or NULL on error.
   */
  AmSession* createSession(AmSipRequest& req, 
			   AmArg* session_params = NULL);

  /**
   * Adds a session to the container (UAS only).
   * @return true if the session is new within the container.
   */
  bool addSession(const string& callid,
		  const string& remote_tag,
		  const string& local_tag,
		  AmSession* session);

  /**
   * Adds a session to the container.
   * @return true if the session is new within the container.
   */
  bool addSession(const string& local_tag,
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
  AmSession* startSessionUAC(AmSipRequest& req, 
			     AmArg* session_params = NULL);

  /**
   * Detroys a session.
   */
  void destroySession(AmSession* s);
  void destroySession(const string& local_tag);

  /**
   * Query the number of active sessions
   */
  int getSize();

  /**
   * post an event into the event queue of the identified dialog.
   * @return false if session doesn't exist 
   */
  bool postEvent(const string& callid, const string& remote_tag,
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
};

#endif
