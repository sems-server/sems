#include "SessionTimer.h"
#include "AmUtils.h"
#include "UserTimer.h"

EXPORT_SESSION_EVENT_HANDLER_FACTORY(SessionTimerFactory, MOD_NAME);

int SessionTimerFactory::onLoad()
{
    return 0;
}

bool SessionTimerFactory::onInvite(const AmSipRequest& req)
{
    if(!checkSessionExpires(req))
	return true;
    return false;
}


AmSessionEventHandler* SessionTimerFactory::getHandler(AmSession* s)
{
    return new SessionTimer(s);
}


SessionTimer::SessionTimer(AmSession* s)
    :AmSessionEventHandler(s),
     session_interval(0), 
     session_refresher(refresh_remote)
{}

bool SessionTimer::process(AmEvent* ev)
{
    /* Session Timer: -ssa */
    AmTimeoutEvent* timeout_ev = dynamic_cast<AmTimeoutEvent*>(ev);
    if (timeout_ev) {
	DBG("received timeout Event with ID %d\n", timeout_ev->data.get(0).asInt());
	onTimeoutEvent(timeout_ev);
	return true;
    }

    return false;
}

bool SessionTimer::onSipEvent(AmSipEvent* ev)
{
    return false;
}

bool SessionTimer::onSipRequest(const AmSipRequest& req)
{
    updateTimer(s,req);
    return false;
}

bool SessionTimer::onSipReply(const AmSipReply& reply)
{
    updateTimer(s,reply);
    return false;
}

bool SessionTimer::onSendRequest(const string& method, 
				 const string& content_type,
				 const string& body,
				 string& hdrs)
{
//   if (!session_timer_conf.getEnableSessionTimer())
//     return "";
  
  string m_hdrs = "Supported: timer\n";
  if  ((method != "INVITE") && (method != "UPDATE"))
      goto end;
  
  m_hdrs += "Session-Expires: "+ int2str(session_timer_conf.getSessionExpires()) +"\n"
      + "Min-SE: " + int2str(session_timer_conf.getMinimumTimer()) + "\n";

 end:
  hdrs += m_hdrs;
  return false;
}


bool SessionTimer::onSendReply(const AmSipRequest& req,
			       unsigned int  code,const string& reason,
			       const string& content_type,const string& body,
			       string& hdrs)
{
    //if (!session_timer_conf.getEnableSessionTimer())
    // 	return "";

    string m_hdrs = "Supported: timer\n";
    if  ((req.method != "INVITE") && (req.method != "UPDATE")) 
	return false;
    
    // only in 2xx responses to INV/UPD
    m_hdrs  += "Session-Expires: " + int2str(session_interval) + ";refresher="+
	(session_refresher_role==UAC ? "uac":"uas")+"\n";
    
    if (((session_refresher_role==UAC) && (session_refresher==refresh_remote)) 
	|| ((session_refresher_role==UAS) && remote_timer_aware))
	m_hdrs += "Required: timer\n";
    
    hdrs += m_hdrs;

    return false;
}


/* Session Timer: -ssa */
void SessionTimer::configureSessionTimer(const AmSessionTimerConfig& conf) 
{
  session_timer_conf = conf;
  DBG("Configured session with EnableSessionTimer = %s, SessionExpires = %u, MinimumTimer = %u\n", 
      session_timer_conf.getEnableSessionTimer() ? "yes":"no", 
      session_timer_conf.getSessionExpires(),
      session_timer_conf.getMinimumTimer()
      );
}
/** 
 * check if UAC requests too low Session-Expires 
 *   (<locally configured Min-SE)                  
 * Throws SessionIntervalTooSmallException if too low
 */

bool SessionTimerFactory::checkSessionExpires(const AmSipRequest& req) 
{
    //if (session_timer_conf.getEnableSessionTimer()) {
    string session_expires = getHeader(req.hdrs, "Session-Expires", "x");

    if (session_expires.length()) {
      unsigned int i_se;
      if (!str2i(strip_header_params(session_expires), i_se)) {
	  //if (i_se < session_timer_conf.getMinimumTimer()) {
	  //TODO: reply_error...
	  //throw SessionTimerException(session_timer_conf.getMinimumTimer());
	  //}
      } else
	throw AmSession::Exception(500,"internal error"); // malformed request?
    }
    //}

    return true;
}

void SessionTimer::updateTimer(AmSession* s, const AmSipRequest& req) {

//   if (!session_timer_conf.getEnableSessionTimer())
//     return;

  if((req.method == "INVITE")||(req.method == "UPDATE")){
    
    remote_timer_aware = 
      key_in_list(getHeader(req.hdrs, "Supported"),"timer");
    
    // determine session interval
    string sess_expires_hdr = getHeader(req.hdrs, "Session-Expires", "x");
    
    //session_interval = get_session_interval_from(req);
    if (!sess_expires_hdr.empty()) {
      if (str2i(strip_header_params(sess_expires_hdr),
		session_interval)) {
	WARN("error while parsing Session-Expires header value '%s'\n", 
	     strip_header_params(sess_expires_hdr).c_str()); // exception?
	session_interval = session_timer_conf.getSessionExpires();  
      }
    } else {
      session_interval = session_timer_conf.getSessionExpires();  
    }

    // get Min-SE
    unsigned int i_minse = session_timer_conf.getMinimumTimer();
    string min_se_hdr = getHeader(req.hdrs, "Min-SE");
    if (!min_se_hdr.empty()) {
      if (str2i(strip_header_params(min_se_hdr),
		i_minse)) {
	WARN("error while parsing Min-SE header value '%s'\n", 
	     strip_header_params(min_se_hdr).c_str()); // exception?
      }
    }

    // calculate actual se
    unsigned int min = session_timer_conf.getMinimumTimer();
    if (i_minse > min)
      min = i_minse;
    if ((session_timer_conf.getSessionExpires() < min)||
	(session_interval<min)) {
      session_interval = min;
    } else {
      if (session_timer_conf.getSessionExpires() < session_interval)
	session_interval = session_timer_conf.getSessionExpires();
    }
     
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
  string sess_expires_hdr = getHeader(reply.hdrs, "Session-Expires");
  if (sess_expires_hdr.empty())
    sess_expires_hdr = getHeader(reply.hdrs, "x"); // compact form
  
  session_interval = session_timer_conf.getSessionExpires();
  session_refresher = refresh_local;
  session_refresher_role = UAC;
  
  if (!sess_expires_hdr.empty()) {
    //session_interval = get_session_interval_from(req);
    unsigned int sess_i_tmp = 0;
    if (str2i(strip_header_params(sess_expires_hdr),
	      sess_i_tmp)) {
      WARN("error while parsing Session-Expires header value '%s'\n", 
	   strip_header_params(sess_expires_hdr).c_str()); // exception?
    } else {
      // this is forbidden by rfc, but to be sure against 'rogue' proxy/uas
      if (sess_i_tmp < session_timer_conf.getMinimumTimer()) {
	session_interval = session_timer_conf.getMinimumTimer();
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
//   if (!session_timer_conf.getEnableSessionTimer())
//     return;
    int timer_id = timeout_ev->data.get(0).asInt();

    if (timer_id == ID_SESSION_REFRESH_TIMER) {
	if (session_refresher == refresh_local)
	    s->sendReinvite();
	else
	    WARN("need session refresh but remote session is refresher\n");
    } else if (timer_id == ID_SESSION_INTERVAL_TIMER) {
//     // let the session know it got timeout
//     onTimeout();
	
	s->dlg.bye();
	s->setStopped();

    } else {
	DBG("unknown timeout event received.\n");
    }

    return;
}

// void AmSession::onTimeout() 
// {
//   DBG("Session %s timed out, stopping.\n", getLocalTag().c_str());
//   setStopped();
// }
