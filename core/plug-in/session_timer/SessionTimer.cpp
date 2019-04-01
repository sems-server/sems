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

#include "SessionTimer.h"
#include "AmUtils.h"
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
   min_se(0),
   session_interval(0),
   session_refresher(refresh_remote),
   accept_501_reply(true)
{
}

SessionTimer::~SessionTimer(){
  if (NULL != s)
    removeTimers(s);
}

bool SessionTimer::process(AmEvent* ev)
{
  assert(ev);
  AmTimeoutEvent* timeout_ev = dynamic_cast<AmTimeoutEvent*>(ev);
  if (timeout_ev) {
    if (timeout_ev->data.get(0).asInt() >= ID_SESSION_TIMER_TIMERS_START &&
	timeout_ev->data.get(0).asInt() <=ID_SESSION_TIMER_TIMERS_END) {
      DBG("received timeout Event with ID %d\n", timeout_ev->data.get(0).asInt());
      onTimeoutEvent(timeout_ev);
    }
    return true;
  }

  return false;
}

bool SessionTimer::onSipRequest(const AmSipRequest& req)
{
  updateTimer(s,req);
  return false;
}

bool SessionTimer::onSipReply(const AmSipRequest& req, const AmSipReply& reply, 
			      AmBasicSipDialog::Status old_dlg_status)
{
  if (session_timer_conf.getEnableSessionTimer() &&
      ((reply.cseq_method == SIP_METH_INVITE) || 
       (reply.cseq_method == SIP_METH_UPDATE))) {
    if ((reply.code == 422) &&
	(s->dlg->getStatus() != AmSipDialog::Connected)) {
      // get Min-SE
      unsigned int i_minse;
      string min_se_hdr = getHeader(reply.hdrs, SIP_HDR_MIN_SE, true);
      if (!min_se_hdr.empty()) {
	if (str2i(strip_header_params(min_se_hdr), i_minse)) {
	  WARN("error while parsing " SIP_HDR_MIN_SE " header value '%s'\n",
	       strip_header_params(min_se_hdr).c_str());
	} else {
	  if (i_minse <= session_timer_conf.getMaximumTimer()) {
	    session_interval = i_minse;
	    unsigned int new_cseq = s->dlg->cseq;
	    DBG("old dialog status is: %s\n",
		AmBasicSipDialog::getStatusStr(old_dlg_status));
	    // resend request with interval i_minse
	    std::map<unsigned int, SIPRequestInfo>::iterator ri = 
	      sent_requests.find(reply.cseq);
	    if (ri != sent_requests.end()) {
	      s->dlg->setRemoteTag("");
	      if (s->dlg->sendRequest(req.method, &(ri->second.body),
				      ri->second.hdrs) == 0) {
		DBG("request with new Session Interval %u successfully sent\n",
		    i_minse);
		// undo SIP dialog status change
		DBG("new dialog status is: %s\n", s->dlg->getStatusStr());
		if (s->dlg->getStatus() != old_dlg_status)
		  s->dlg->setStatus(old_dlg_status);
		s->updateUACTransCSeq(reply.cseq, new_cseq);

		// processed
		DBG("erasing %d from sent_requests\n", req.cseq);
		sent_requests.erase(reply.cseq);
		return true;
	      } else {
		ERROR("failed to send request with new Session Interval\n");
	      }
	    } else {
	      ERROR("could not find sent request with cseq %u\n", reply.cseq);
	    }
	  } else {
	    DBG("other side requests too high Min-SE: %u (our limit %u)\n",
		i_minse, session_timer_conf.getMaximumTimer());
	  }
	}
      } else {
	WARN("mandatory Min-SE header missing in 422 reply\n");
      }
    }
    if ((reply.code >= 200) &&
	(strcmp(s->dlg->getStatusStr(old_dlg_status), "Connected") != 0)) {
      DBG("erasing %d from sent_requests\n", req.cseq);
      sent_requests.erase(reply.cseq);
    }
  }
  
  if ((reply.cseq_method == SIP_METH_INVITE) || 
      (reply.cseq_method == SIP_METH_UPDATE)) {
    updateTimer(s,reply);
  }

  return false;
}

bool SessionTimer::onSendRequest(AmSipRequest& req, int& flags)
{
  if (req.method == "BYE") {
    removeTimers(s);
    return false;
  }

  if (session_timer_conf.getEnableSessionTimer() &&
      ((req.method == SIP_METH_INVITE) || (req.method == SIP_METH_UPDATE)) &&
      (s->dlg->getStatus() == AmSipDialog::Disconnected)) {
    // save initial INVITE and UPDATE so we can resend on 422 reply
    DBG("adding %d to sent_requests\n", req.cseq);
    sent_requests[req.cseq] = SIPRequestInfo(req.method, &req.body, req.hdrs);
  }

  addOptionTag(req.hdrs, SIP_HDR_SUPPORTED, TIMER_OPTION_TAG);
  if  ((req.method != SIP_METH_INVITE) && (req.method != SIP_METH_UPDATE))
    return false; // session-expires / min-se only in INV/UPD

  removeHeader(req.hdrs, SIP_HDR_SESSION_EXPIRES);
  removeHeader(req.hdrs, SIP_HDR_MIN_SE);
  req.hdrs += SIP_HDR_COLSP(SIP_HDR_SESSION_EXPIRES) + int2str(session_interval) + CRLF
    + SIP_HDR_COLSP(SIP_HDR_MIN_SE) + int2str(min_se) + CRLF;

  return false;
}


bool SessionTimer::onSendReply(const AmSipRequest& req,
			       AmSipReply& reply, int& flags)
{
  // only in 2xx responses to INV/UPD
  if  (((reply.cseq_method != SIP_METH_INVITE) && 
	(reply.cseq_method != SIP_METH_UPDATE)) ||
       (reply.code < 200) || (reply.code >= 300))
    return false;

  addOptionTag(reply.hdrs, SIP_HDR_SUPPORTED, TIMER_OPTION_TAG);

  if (((session_refresher_role==UAC) && (session_refresher==refresh_remote))
      || ((session_refresher_role==UAS) && remote_timer_aware)) {
    addOptionTag(reply.hdrs, SIP_HDR_REQUIRE, TIMER_OPTION_TAG);
  } else {
    removeOptionTag(reply.hdrs, SIP_HDR_REQUIRE, TIMER_OPTION_TAG);
  }

  // remove (possibly existing) Session-Expires header
  removeHeader(reply.hdrs, SIP_HDR_SESSION_EXPIRES);

  reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SESSION_EXPIRES) +
    int2str(session_interval) + ";refresher="+
    (session_refresher_role==UAC ? "uac":"uas")+CRLF;

  return false;
}

int SessionTimer::configure(AmConfigReader& conf)
{
  if(session_timer_conf.readFromConfig(conf))
    return -1;

  session_interval = session_timer_conf.getSessionExpires();
  min_se = session_timer_conf.getMinimumTimer();

  DBG("Configured session with EnableSessionTimer = %s, "
      "SessionExpires = %u, MinimumTimer = %u\n",
      session_timer_conf.getEnableSessionTimer() ? "yes":"no", 
      session_timer_conf.getSessionExpires(),
      session_timer_conf.getMinimumTimer()
      );

  if (conf.hasParameter("session_refresh_method")) {
    string refresh_method_s = conf.getParameter("session_refresh_method");
    if (refresh_method_s == "UPDATE") {
      s->refresh_method = AmSession::REFRESH_UPDATE;
    } else if (refresh_method_s == "UPDATE_FALLBACK_INVITE") {
      s->refresh_method = AmSession::REFRESH_UPDATE_FB_REINV;
    } else if (refresh_method_s == "INVITE") {
      s->refresh_method = AmSession::REFRESH_REINVITE;
    } else {
      ERROR("unknown setting for 'session_refresh_method' config option.\n");
      return -1;
    }
    DBG("set session refresh method: %d.\n", s->refresh_method);
  }

  if (conf.getParameter("accept_501_reply")=="no")
    accept_501_reply = false;

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
      key_in_list(getHeader(req.hdrs, SIP_HDR_SUPPORTED, SIP_HDR_SUPPORTED_COMPACT),
		  TIMER_OPTION_TAG);
    
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

    // the greates minimum limit of both
    if (i_minse > min_se) min_se = i_minse;

    // calculate actual se
    session_interval = session_timer_conf.getSessionExpires();

    if (rem_has_sess_expires) {
      if (rem_sess_expires < session_interval) {
        session_interval = rem_sess_expires;
      }
      if (session_interval < min_se) {
        session_interval = min_se;
      }
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

  // only update timer on positive reply, or 501 if config'd
  if (((reply.code < 200) || (reply.code >= 300)) &&
      (!(accept_501_reply && reply.code == 501)))
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

  s->setTimer(ID_SESSION_INTERVAL_TIMER, session_interval);
    
  // set session refresh action timer, after half the expiration
  if (session_refresher == refresh_local) {
    DBG("Setting session refresh timer: %ds, tag '%s'\n", session_interval/2, 
	s->getLocalTag().c_str());
    s->setTimer(ID_SESSION_REFRESH_TIMER, session_interval/2);
  }
}

void SessionTimer::retryRefreshTimer(AmSession* s) {
  DBG("Retrying session refresh timer: T-2s, tag '%s' \n",
      s->getLocalTag().c_str());

  s->setTimer(ID_SESSION_REFRESH_TIMER, 2);
}


void SessionTimer::removeTimers(AmSession* s) 
{
  s->removeTimer(ID_SESSION_REFRESH_TIMER);
  s->removeTimer(ID_SESSION_INTERVAL_TIMER);
}

void SessionTimer::onTimeoutEvent(AmTimeoutEvent* timeout_ev) 
{

  int timer_id = timeout_ev->data.get(0).asInt();

  if (s->dlg->getStatus() == AmSipDialog::Disconnecting ||
      s->dlg->getStatus() == AmSipDialog::Disconnected) {
    DBG("ignoring SST timeout event %i in Disconnecting/-ed session\n",
	timer_id);
    return;
  }

  if (timer_id == ID_SESSION_REFRESH_TIMER) {
    if (session_refresher == refresh_local) {
      DBG("Session Timer: initiating session refresh\n");
      if (!s->refresh()) {
	retryRefreshTimer(s);
      }
    } else {
      DBG("need session refresh but remote session is refresher\n");
    }
  } else if (timer_id == ID_SESSION_INTERVAL_TIMER) {
    s->onSessionTimeout();
  } else {
    DBG("unknown timeout event received.\n");
  }

  return;
}

AmSessionTimerConfig::AmSessionTimerConfig()
  : EnableSessionTimer(DEFAULT_ENABLE_SESSION_TIMER), 
    SessionExpires(SESSION_EXPIRES), 
    MinimumTimer(MINIMUM_TIMER),
    MaximumTimer(MAXIMUM_TIMER)
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

  if (cfg.hasParameter("maximum_timer")){
    int maximum_timer = 0;
    if (!str2int(cfg.getParameter("maximum_timer"), maximum_timer) ||
	maximum_timer<=0) {
      ERROR("invalid value for maximum_timer '%s'\n",
	    cfg.getParameter("maximum_timer").c_str());
      return -1;
    }
    MaximumTimer = (unsigned int) maximum_timer;
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
