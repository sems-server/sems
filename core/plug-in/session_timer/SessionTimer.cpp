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

#include "SessionTimer.h"
#include "AmUtils.h"
#include "UserTimer.h"
#include "AmSipHeaders.h"

EXPORT_SESSION_EVENT_HANDLER_FACTORY(SessionTimerFactory, MOD_NAME);

int SessionTimerFactory::onLoad()
{
  return 0;
}

bool SessionTimerFactory::onInvite(const AmSipRequest& req, AmConfigReader& cfg)
{
  return checkSessionExpires(req, cfg);
}


AmSessionEventHandler* SessionTimerFactory::getHandler(AmSession* s)
{
  return new SessionTimer(s);
}


SessionTimer::SessionTimer(AmSession* s)
  :AmSessionEventHandler(),
   s(s),
   session_interval(0), 
   session_refresher(refresh_remote)
{}

bool SessionTimer::process(AmEvent* ev)
{
  assert(ev);
  AmTimeoutEvent* timeout_ev = dynamic_cast<AmTimeoutEvent*>(ev);
  if (timeout_ev) {
    DBG("received timeout Event with ID %d\n", timeout_ev->data.get(0).asInt());
    onTimeoutEvent(timeout_ev);
    return true;
  }

  return false;
}

bool SessionTimer::onSipRequest(const AmSipRequest& req)
{
  updateTimer(s,req);
  return false;
}

bool SessionTimer::onSipReply(const AmSipReply& reply, int old_dlg_status)
{
  updateTimer(s,reply);
  return false;
}

bool SessionTimer::onSendRequest(const string& method, 
				 const string& content_type,
				 const string& body,
				 string& hdrs,
				 int flags,
				 unsigned int cseq)
{
  string m_hdrs = SIP_HDR_COLSP(SIP_HDR_SUPPORTED)  "timer"  CRLF;
  if  ((method != SIP_METH_INVITE) && (method != SIP_METH_UPDATE))
    goto end;
  
  m_hdrs += SIP_HDR_COLSP(SIP_HDR_SESSION_EXPIRES) + int2str(session_interval) +CRLF
    + SIP_HDR_COLSP(SIP_HDR_MIN_SE) + int2str(min_se) + CRLF;
  
 end:
  hdrs += m_hdrs;
  return false;
}


bool SessionTimer::onSendReply(const AmSipRequest& req,
			       unsigned int  code,const string& reason,
			       const string& content_type,const string& body,
			       string& hdrs,
			       int flags)
{
  string m_hdrs = SIP_HDR_COLSP(SIP_HDR_SUPPORTED)  "timer"  CRLF;
  if  ((req.method != SIP_METH_INVITE) && (req.method != SIP_METH_UPDATE))
    return false;
    
  // only in 2xx responses to INV/UPD
  m_hdrs  += SIP_HDR_COLSP(SIP_HDR_SESSION_EXPIRES) +
    int2str(session_interval) + ";refresher="+
    (session_refresher_role==UAC ? "uac":"uas")+CRLF;
    
  if (((session_refresher_role==UAC) && (session_refresher==refresh_remote)) 
      || ((session_refresher_role==UAS) && remote_timer_aware))
    m_hdrs += SIP_HDR_COLSP(SIP_HDR_REQUIRE)  "timer"  CRLF;
    
  hdrs += m_hdrs;

  return false;
}

int SessionTimer::configure(AmConfigReader& conf)
{
  if(!session_timer_conf.readFromConfig(conf))
    return -1;

  session_interval = session_timer_conf.getSessionExpires();
  min_se = session_timer_conf.getMinimumTimer();

  DBG("Configured session with EnableSessionTimer = %s, "
      "SessionExpires = %u, MinimumTimer = %u\n",
      session_timer_conf.getEnableSessionTimer() ? "yes":"no", 
      session_timer_conf.getSessionExpires(),
      session_timer_conf.getMinimumTimer()
      );

  return 0;
}

/** 
 * check if UAC requests too low Session-Expires 
 *   (<locally configured Min-SE)                  
 * Throws SessionIntervalTooSmallException if too low
 */
bool SessionTimerFactory::checkSessionExpires(const AmSipRequest& req, AmConfigReader& cfg)
{
  AmSessionTimerConfig sst_cfg;
  if (sst_cfg.readFromConfig(cfg)) {
    return false;
  }

  string session_expires = getHeader(req.hdrs, SIP_HDR_SESSION_EXPIRES,
				     SIP_HDR_SESSION_EXPIRES_COMPACT, true);

  if (session_expires.length()) {
    unsigned int i_se;
    if (!str2i(strip_header_params(session_expires), i_se)) {
      if (i_se < sst_cfg.getMinimumTimer()) {
	throw AmSession::Exception(422, "Session Interval Too Small",
				   SIP_HDR_COLSP(SIP_HDR_MIN_SE)+
				   int2str(sst_cfg.getMinimumTimer())+CRLF);
      }
    } else {
      WARN("parsing session expires '%s' failed\n", session_expires.c_str());
      throw AmSession::Exception(400,"Bad Request");
    }
  }

  return true;
}

void SessionTimer::updateTimer(AmSession* s, const AmSipRequest& req) {

  if((req.method == SIP_METH_INVITE)||(req.method == SIP_METH_UPDATE)){
    
    remote_timer_aware = 
      key_in_list(getHeader(req.hdrs, SIP_HDR_SUPPORTED),"timer", true);
    
    // determine session interval
    string sess_expires_hdr = getHeader(req.hdrs, SIP_HDR_SESSION_EXPIRES,
					SIP_HDR_SESSION_EXPIRES_COMPACT, true);
    
    bool rem_has_sess_expires = false;
    unsigned int rem_sess_expires=0; 
    if (!sess_expires_hdr.empty()) {
      if (str2i(strip_header_params(sess_expires_hdr),
		rem_sess_expires)) {
	WARN("error while parsing " SIP_HDR_SESSION_EXPIRES " header value '%s'\n",
	     strip_header_params(sess_expires_hdr).c_str()); // exception?
      } else {
	rem_has_sess_expires = true;
      }
    }

    // get Min-SE
    unsigned int i_minse = min_se;
    string min_se_hdr = getHeader(req.hdrs, SIP_HDR_MIN_SE, true);
    if (!min_se_hdr.empty()) {
      if (str2i(strip_header_params(min_se_hdr),
		i_minse)) {
	WARN("error while parsing " SIP_HDR_MIN_SE " header value '%s'\n",
	     strip_header_params(min_se_hdr).c_str()); // exception?
      }
    }

    // calculate actual se
    session_interval = session_timer_conf.getSessionExpires();

    if (i_minse > min_se)
      min_se = i_minse;

    if (rem_has_sess_expires && (rem_sess_expires < min_se)) {
      session_interval = min_se;
    } else {
      if (rem_has_sess_expires && (rem_sess_expires < session_interval))
	session_interval = rem_sess_expires;
    }
     
    DBG("using actual session interval %u\n", session_interval);

    // determine session refresher -- cf rfc4028 Table 2
    // only if the remote party supports timer and asks 
    // to be refresher we will let the remote party do it. 
    // if remote supports timer and does not specify,
    // could also be refresher=uac
    if ((remote_timer_aware) && (!sess_expires_hdr.empty()) &&
	(get_header_param(sess_expires_hdr, "refresher") == "uac")) {
      DBG("session refresher will be remote UAC.\n");
      session_refresher      = refresh_remote;
      session_refresher_role = UAC;
    } else {
      DBG("session refresher will be local UAS.\n");
      session_refresher      = refresh_local;
      session_refresher_role = UAS;
    }
    
    removeTimers(s);
    setTimers(s);

  } else if (req.method == "BYE") { // remove all timers?
    removeTimers(s);
  }
}

void SessionTimer::updateTimer(AmSession* s, const AmSipReply& reply) 
{
  if (!session_timer_conf.getEnableSessionTimer())
    return;

  // only update timer on positive reply
  if ((reply.code < 200) || (reply.code >= 300))
    return;
  
  // determine session interval
  string sess_expires_hdr = getHeader(reply.hdrs, SIP_HDR_SESSION_EXPIRES,
				      SIP_HDR_SESSION_EXPIRES_COMPACT, true);

  session_refresher = refresh_local;
  session_refresher_role = UAC;
  
  if (!sess_expires_hdr.empty()) {
    unsigned int sess_i_tmp = 0;
    if (str2i(strip_header_params(sess_expires_hdr),
	      sess_i_tmp)) {
      WARN("error while parsing " SIP_HDR_SESSION_EXPIRES " header value '%s'\n",
	   strip_header_params(sess_expires_hdr).c_str()); // exception?
    } else {
      // this is forbidden by rfc, but to be sure against 'rogue' proxy/uas
      if (sess_i_tmp < min_se) {
	session_interval = min_se;
      } else {
	session_interval = sess_i_tmp;
      }
    }
    if (get_header_param(sess_expires_hdr, "refresher") == "uas") {
      session_refresher = refresh_remote;
      session_refresher_role = UAS;
    } 
  }
  
  removeTimers(s);
  setTimers(s);
}

void SessionTimer::setTimers(AmSession* s) 
{
  // set session timer
  DBG("Setting session interval timer: %ds, tag '%s'\n", session_interval, 
      s->getLocalTag().c_str());

  UserTimer::instance()->
    setTimer(ID_SESSION_INTERVAL_TIMER, session_interval, s->getLocalTag());
    
  // set session refresh action timer, after half the expiration
  if (session_refresher == refresh_local) {
    DBG("Setting session refresh timer: %ds, tag '%s'\n", session_interval/2, 
	s->getLocalTag().c_str());
    UserTimer::instance()->
      setTimer(ID_SESSION_REFRESH_TIMER, session_interval/2, s->getLocalTag());
  }
}

void SessionTimer::removeTimers(AmSession* s) 
{
  UserTimer::instance()->
    removeTimer(ID_SESSION_REFRESH_TIMER, s->getLocalTag());
  UserTimer::instance()->
    removeTimer(ID_SESSION_INTERVAL_TIMER, s->getLocalTag());
}

void SessionTimer::onTimeoutEvent(AmTimeoutEvent* timeout_ev) 
{
  int timer_id = timeout_ev->data.get(0).asInt();

  if (timer_id == ID_SESSION_REFRESH_TIMER) {
    DBG("Session Timer: initiating refresh (Re-Invite)\n");
    if (session_refresher == refresh_local) 
      // send reinvite with SDP
      s->sendReinvite(true);
    else
      WARN("need session refresh but remote session is refresher\n");
  } else if (timer_id == ID_SESSION_INTERVAL_TIMER) {
    //     // let the session know it got timeout
    //     onTimeout();
    DBG("Session Timer: Timeout, ending session.\n");
    s->dlg.bye();
    s->setStopped();
  } else {
    DBG("unknown timeout event received.\n");
  }

  return;
}

AmSessionTimerConfig::AmSessionTimerConfig()
  : EnableSessionTimer(DEFAULT_ENABLE_SESSION_TIMER), 
    SessionExpires(SESSION_EXPIRES), 
    MinimumTimer(MINIMUM_TIMER)
{

}
AmSessionTimerConfig::~AmSessionTimerConfig() 
{
}

int AmSessionTimerConfig::readFromConfig(AmConfigReader& cfg)
{
  // enable_session_timer
  if(cfg.hasParameter("enable_session_timer")){
    if(!setEnableSessionTimer(cfg.getParameter("enable_session_timer"))){
      ERROR("invalid enable_session_timer specified\n");
      return -1;
    }
  }

  // session_expires
  if(cfg.hasParameter("session_expires")){
    if(!setSessionExpires(cfg.getParameter("session_expires"))){
      ERROR("invalid session_expires specified\n");
      return -1;
    }
  }

  // minimum_timer
  if(cfg.hasParameter("minimum_timer")){
    if(!setMinimumTimer(cfg.getParameter("minimum_timer"))){
      ERROR("invalid minimum_timer specified\n");
      return -1;
    }
  }
  return 0;
}

int AmSessionTimerConfig::setEnableSessionTimer(const string& enable) {
  if ( strcasecmp(enable.c_str(), "yes") == 0 ) {
    EnableSessionTimer = 1;
  } else if ( strcasecmp(enable.c_str(), "no") == 0 ) {
    EnableSessionTimer = 0;
  } else {
    return 0;
  }	
  return 1;
}		

int AmSessionTimerConfig::setSessionExpires(const string& se) {
  if(sscanf(se.c_str(),"%u",&SessionExpires) != 1) {
    return 0;
  }
  DBG("setSessionExpires(%i)\n",SessionExpires);
  return 1;
} 

int AmSessionTimerConfig::setMinimumTimer(const string& minse) {
  if(sscanf(minse.c_str(),"%u",&MinimumTimer) != 1) {
    return 0;
  }
  DBG("setMinimumTimer(%i)\n",MinimumTimer);
  return 1;
}
