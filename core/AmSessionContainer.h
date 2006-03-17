#ifndef AmSessionContainer_h
#define AmSessionContainer_h

#include "AmThread.h"
#include "AmSession.h"

#include <string>
#include <vector>
#include <queue>
#include <map>

using std::string;
using std::vector;
using std::queue;
using std::map;

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
    typedef map<string,AmSession*> SessionMap;
    typedef SessionMap::iterator SessionMapIter;

    typedef map<string,string>     Dictionnary;
    typedef Dictionnary::iterator  DictIter;

    typedef queue<AmSession*>      SessionQueue;

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
    AmSession* createSession(AmSipRequest& req);

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
     * @param hash_str Call-ID + remote-tag
     * @param sess_key local-tag
     * @param req client's request
     */
    void startSessionUAS(AmSipRequest& req);

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

    /** register a daemon to receive events. 
     *  note: there is no de-registering!!! daemons are to be always there! */
    void registerDaemon(const string& daemon_name, AmEventQueue* event_queue);

};

#endif
