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

#ifndef SessionTimer_h
#define SessionTimer_h

#include "AmApi.h"
#include "AmSession.h"

#define MOD_NAME "session_timer"

/* Session Timer: -ssa */
class AmTimeoutEvent;
// these are the timer IDs for session timer
// Caution: do not use these for other purposes
#define ID_SESSION_INTERVAL_TIMER -1
#define ID_SESSION_REFRESH_TIMER  -2

/* Session Timer defaul configuration: */
#define DEFAULT_ENABLE_SESSION_TIMER 1
#define SESSION_EXPIRES              120  // seconds
#define MINIMUM_TIMER                90   //seconds


/** \brief Factory of the session timer event handler */
class SessionTimerFactory: public AmSessionEventHandlerFactory
{
  bool checkSessionExpires(const AmSipRequest& req, AmConfigReader& cfg);

 public:
  SessionTimerFactory(const string& name)
    : AmSessionEventHandlerFactory(name) {}

  int onLoad();
  bool onInvite(const AmSipRequest& req, AmConfigReader& cfg);

  AmSessionEventHandler* getHandler(AmSession* s);
};

/** \brief config for the session timer */
class AmSessionTimerConfig
{

  /** Session Timer: enable? */
  int EnableSessionTimer;
  /** Session Timer: Desired Session-Expires */
  unsigned int SessionExpires;
  /** Session Timer: Minimum Session-Expires */
  unsigned int MinimumTimer;
    
public:
  AmSessionTimerConfig();
  ~AmSessionTimerConfig();
  

  /** Session Timer: Enable Session Timer?
      returns 0 on invalid value */
  int setEnableSessionTimer(const string& enable);
  /** Session Timer: Setter for Desired Session-Expires, 
      returns 0 on invalid value */
  int setSessionExpires(const string& se);
  /** Session Timer: Setter for Minimum Session-Expires, 
      returns 0 on invalid value */
  int setMinimumTimer(const string& minse);

  bool getEnableSessionTimer() { return EnableSessionTimer; }
  unsigned int getSessionExpires() { return SessionExpires; }
  unsigned int getMinimumTimer() { return MinimumTimer; }

  int readFromConfig(AmConfigReader& cfg);
};

/** \brief SessionEventHandler for implementing session timer logic for a session */
class SessionTimer: public AmSessionEventHandler
{
  AmSessionTimerConfig session_timer_conf;
  AmSession* s;

  enum SessionRefresher {
    refresh_local,
    refresh_remote
  };
  enum SessionRefresherRole {
    UAC,
    UAS
  };

  bool                 remote_timer_aware;
  unsigned int         min_se;
  unsigned int         session_interval;  
  SessionRefresher     session_refresher;
  SessionRefresherRole session_refresher_role;
  bool                 accept_501_reply;

  void updateTimer(AmSession* s,const AmSipRequest& req);
  void updateTimer(AmSession* s,const AmSipReply& reply);
    
  void setTimers(AmSession* s);
  void retryRefreshTimer(AmSession* s);
  void removeTimers(AmSession* s);

  string getReplyHeaders(const AmSipRequest& req);
  string getRequestHeaders(const string& method);

  /* Session Timer: -ssa */

  // @return true if OK
  void onTimeout();
  void onTimeoutEvent(AmTimeoutEvent* timeout_ev);

 public:
  SessionTimer(AmSession*);
  virtual ~SessionTimer(){}

  /* @see AmSessionEventHandler */
  virtual int  configure(AmConfigReader& conf); 
  virtual bool process(AmEvent*);

  virtual bool onSipRequest(const AmSipRequest&);
  virtual bool onSipReply(const AmSipReply&, int old_dlg_status,
			  const string& trans_method);

  virtual bool onSendRequest(const string& method, 
			     const string& content_type,
			     const string& body,
			     string& hdrs,
			     int flags,
			     unsigned int cseq);

  virtual bool onSendReply(const AmSipRequest& req,
			   unsigned int  code,
			   const string& reason,
			   const string& content_type,
			   const string& body,
			   string& hdrs,
			   int flags);
};


#endif
