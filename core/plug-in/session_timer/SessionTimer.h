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


class SessionTimerFactory: public AmSessionEventHandlerFactory
{
    bool checkSessionExpires(const AmSipRequest& req);

public:
    SessionTimerFactory(const string& name)
	: AmSessionEventHandlerFactory(name) {}

    int onLoad();
    bool onInvite(const AmSipRequest&);

    AmSessionEventHandler* getHandler(AmSession* s);
};

class SessionTimer: public AmSessionEventHandler
{
    /* Session Timer: -ssa */
    enum SessionRefresher {
      refresh_local,
      refresh_remote
    };
    enum SessionRefresherRole {
      UAC,
      UAS
    };

    void configureSessionTimer(const AmSessionTimerConfig& conf);
    AmSessionTimerConfig session_timer_conf;

    bool                 remote_timer_aware;
    unsigned int         session_interval;  
    SessionRefresher     session_refresher;
    SessionRefresherRole session_refresher_role;

    void updateTimer(AmSession* s,const AmSipRequest& req);
    void updateTimer(AmSession* s,const AmSipReply& reply);
    
    void setTimers(AmSession* s);
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
    virtual bool process(AmEvent*);
    virtual bool onSipEvent(AmSipEvent*);
    virtual bool onSipRequest(const AmSipRequest&);
    virtual bool onSipReply(const AmSipReply&);

    virtual bool onSendRequest(const string& method, 
			       const string& content_type,
			       const string& body,
			       string& hdrs,
				   unsigned int cseq);

    virtual bool onSendReply(const AmSipRequest& req,
			     unsigned int  code,
			     const string& reason,
			     const string& content_type,
			     const string& body,
			     string& hdrs);
};


#endif
