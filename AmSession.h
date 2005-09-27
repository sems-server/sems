/*
 * $Id: AmSession.h,v 1.20.2.5 2005/08/31 13:54:29 rco Exp $
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

#ifndef _AmSession_h_
#define _AmSession_h_

#include "AmRtpStream.h"
#include "AmThread.h"
#include "AmRequest.h"
#include "AmEventQueue.h"
#include "AmRtpAudio.h"
#include "AmDtmfDetector.h"

#include <string>
#include <vector>
#include <queue>
#include <map>
using std::string;
using std::vector;
using std::queue;
using std::map;
using std::pair;

#define DAEMON_TYPE_ID "__daemon__"

class AmStateFactory;
class AmDialogState;
class AmDtmfEvent;

class AmSessionEvent: public AmEvent
{
public:
    AmRequestUAS request;

    enum EventType { Bye,Refer,Notify,ReInvite };
    AmSessionEvent(EventType event_type, const AmRequestUAS& request);
};

/**
 *  NOTIFY Event
 */
class AmNotifySessionEvent: public AmSessionEvent
{
    string body;
    string eventPackage;
    string subscriptionState;
    string contentType;
    string contentLength;
    int    cseq;

public:
    AmNotifySessionEvent(AmRequestUAS& request);

    string getBody() { return body; }
    string getEventPackage() { return eventPackage; }
    string getSubscriptionState() { return subscriptionState; }
    string getContentType() { return contentType; }
    string getContentLength() { return contentLength; }
    int    getCSeq() { return cseq; }
};

/**
 * The session class.
 * It implements the behavior.
 * The session is identified by Call-ID, From-Tag and To-Tag.
 */
class AmSession : public AmThread
{
    AmMutex      audio_mut;
    AmAudio*     input;
    AmAudio*     output;
    SdpPayload*  payload;
    bool         first_negotiation;

    auto_ptr<AmDialogState> dialog_st;
    
    AmEventQueue m_dtmfOutputQueue;
    AmDtmfEventQueue m_dtmfEventQueue;
    bool m_dtmfDetectionEnabled;

    /** @see AmThread::run() */
    void run();

    /** @see AmThread::on_stop() */
    void on_stop();

    AmCondition<bool> sess_stopped;
    AmCondition<bool> detached;

    friend class AmSessionScheduler;

public:

    struct Exception {
	int code;
	string reason;
	Exception(int c, string r) : code(c), reason(r) {}
    };

    AmRtpAudio          rtp_str;
    auto_ptr<AmRequest> req;

    /** 
     * Session constructor.
     * @param _req  request which initiated the session 
     * @param _d_st application's dialog state
     */
    AmSession(AmRequest* _req, AmDialogState* _d_st);
    virtual ~AmSession();

    /**
     * Lock and unlock audio input & output
     */
    void lockAudio();
    void unlockAudio();

    /**
     * Audio input & output get methods.
     * Note: audio must be locked.
     */
    AmAudio* getInput() { return input; }
    AmAudio* getOutput(){ return output;}

    /**
     * Audio input & output set methods.
     * Note: audio will be locked by the methods.
     */
    void setInput(AmAudio* in);
    void setOutput(AmAudio* out);
    void setInOut(AmAudio* in, AmAudio* out);

    /**
     * Clears input & ouput (no need to lock)
     */
    void clearAudio();

    /** @see AmThread::onIdle() */
//     void onIdle();

    /** Gets the Session's call ID */
    const string& getCallID();

    /** Gets the Session's remote tag */
    const string& getFromTag();

    /** Gets the Session's local tag */
    const string& getToTag();

    /** Gets the dialog state */
    AmDialogState* getDialogState();

    /** Gets the current RTP payload */
    const SdpPayload* getPayload();

    /** Gets the port number of the remote part of the session */
    int getRPort();

    /** handle SDP negotiation: only for INVITEs & re-INVITEs */
    void negotiate(AmRequest* request);

    /**
     * Destroy the session.
     * It causes the session to be erased from the active session list
     * and added to the dead session list.
     * @see AmSessionContainer
     */
    void destroy();


    /**
     * Signals the session it should stop.
     * This will cause the session to be able 
     * to exit the main loop.
     */
    void setStopped() { sess_stopped.set(true); }

    /**
     * Has the session already been stopped ?
     */
    bool getStopped() { return sess_stopped.get(); }

    /**
     * Creates a new Id which can be used within sessions.
     */
    static string getNewId();

    /**
     * Entry point for DTMF events
     */
    void postDtmfEvent(AmDtmfEvent *);

    void processDtmfEvents();
    bool isDtmfDetectionEnabled() { return m_dtmfDetectionEnabled; }
    void putDtmfAudio(const unsigned char *buf, int size, int user_ts);
    void onDtmf(int event, int duration);
};


/**
 * Centralized session container.
 * This is the register for all active and dead sessions.
 * If has a deamon which wakes up only if it has work. 
 * Then, it kills all dead sessions and try to go to bed 
 * (it cannot sleep if one or more sessions are still alive).
 */
class AmSessionContainer : public AmThread
{
    static AmSessionContainer* _SessionContainer;

    // some typedefs ...
    typedef pair<string,AmSession*> SessionTableEntry;

    typedef vector<SessionTableEntry> SessionTable;
    typedef vector<SessionTableEntry>::iterator SessionTableIter;

    typedef map<string,SessionTable> SessionMap;
    typedef map<string,SessionTable>::iterator SessionMapIter;

    typedef pair<SessionMapIter,SessionTableIter> SessionSearchRes;

    typedef queue<AmSession*> SessionQueue;

    /** Container for active sessions */
    SessionMap a_sessions;
    /** Mutex to protect the active session container */
    AmMutex    as_mut;

    /** Container for dead sessions */
    SessionQueue d_sessions;
    /** Mutex to protect the dead session container */
    AmMutex    ds_mut;

    typedef map<string, AmEventQueue*> DaemonMap;
    /** if daemons are to receive events e.g. about the status of their requests,
     they can register in this map */ 
    DaemonMap  daemon_map;
    AmMutex    daemon_map_mutex;

    /** the daemon only runs if this is true */
    AmCondition<bool> _run_cond;

    /** We are a Singleton ! Avoid people to have their own instance. */
    AmSessionContainer();

    /** 
     * Search for a session in the active session container.
     * @param hash_str Call-ID + From-tag
     * @param sess_key [in,out]To-tag
     * @return the search results...
     */
    SessionSearchRes findSession(const string& hash_str, string& sess_key);

    /**
     * Destroys a session.
     * Processes the result value of findSession().
     * @param sess_pair see findSession().
     * @param sessionsAreLocked is the active session container locked ?
     */
    void destroySession(pair<SessionMapIter,SessionTableIter>& sess_pair, bool sessionsAreLocked);
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
     * Search the container for a session coresponding to hash_str and sess_key.
     * @param hash_str  Call-ID + From-tag
     * @param sess_key  To-tag
     * @param lock      Should the container get locked while searching ?
     * @return the session related to hash_str & sess_key 
     *         or NULL if none has been found.
     */
    AmSession* getSession(const string& hash_str, string& sess_key);

    /**
     * Search the container for a session coresponding to sess_key.
     * NOTE: if the hash_str is known use getSession(hash_str, sess_key)
     *       for better performance!
     *
     * @param sess_key  To-tag (local-tag)
     * @param lock      Should the container get locked while searching ?
     * @return the session related to sess_key 
     *         or NULL if none has been found.
     */
     AmSession* getSession(string& sess_key);

    /**
     * Creates a new session.
     * @param req local request
     * @return a new session or NULL on error.
     */
    AmSession* createSession(const string& app_name, AmRequest* req);
    /**
     * Adds a session to the container.
     * @param hash_str Call-ID + remote-tag
     * @param sess_key local-tag
     * @return true if the session is new within the container.
     */
    bool addSession(const string& hash_str,const string& sess_key,
		    AmSession* session);
    /** 
     * Constructs a new session and adds it to the active session container. 
     * @param hash_str Call-ID + remote-tag
     * @param sess_key local-tag
     * @param req client's request
     */
    void startSession(const string& hash_str,string& sess_key, AmRequestUAS* req);

    /** @return false if session doesn't exist */
    bool postEvent(const string& hash_str,string& sess_key,
		   AmSessionEvent* event);

    /**
     * post a generic event into the event queue of the identified dialog.
     * @return false if session doesn't exist 
     */
    bool postGenericEvent(const string& hash_str,string& sess_key,
			  AmEvent* event);

    /**
     * post a generic event into the event queue of the identified dialog.
     * sess_key is local_tag (to_tag)
     * note: if hash_str is known, use 
     *          postGenericEvent(hash_str,sess_key,event);
     *       for better performance.
     * @return false if session doesn't exist 
     */
    bool postGenericEvent(string& sess_key, AmEvent* event);

      /** register a daemon to receive events. 
     *  note: there is no de-registering!!! daemons are to be always there! */
    void registerDaemon(const string& daemon_name, AmEventQueue* event_queue);

    /**
     * Seek & destroy session.
     * @param hash_str Call-ID + From-tag
     * @param sess_key [in,out]To-tag
     * @return true if session exists
     */
    bool sadSession(const string& hash_str, string& sess_key); // seek & sestroy session
    /**
     * Detroys a session.
     */
    void destroySession(AmSession* s);
};

#endif

// Local Variables:
// mode:C++
// End:

