/*
 * $Id: AmSIPEventDaemon.h,v 1.3.2.1 2005/09/02 13:47:46 rco Exp $
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

#ifndef AM_SIP_EVENT_DAEMON_H_
#define AM_SIP_EVENT_DAEMON_H_

#include "sys/time.h"

#include "AmThread.h"
#include "AmEventQueue.h"
#include "AmRequest.h"
#include "AmSession.h"
#include <string>

#include <map>
#include <string>
#include <vector>
#include <utility>

struct AmSubscription {

    enum SubstateValue    { Active, Pending, Terminated };
    enum EventReasonValue { NoReason, Deactivated, Probation, 
			    Rejected, Timeout, Giveup, Noresource };

    const std::string getReasonString(const EventReasonValue& r) const;

    AmRequestUAS req;   // the request that triggered this
    std::string ID;          // the subscription ID
    time_t created;
    int expires;
    SubstateValue subscriptionState;
    EventReasonValue reason;
    int retry_after;

    std::string hash_str; 
    std::string sess_key;

    AmSubscription(const AmRequestUAS& req_, const std::string& hash_str_, 
		   const std::string& sess_key_, std::string subID, 
		   int sipExpires, const SubstateValue& subState, 
		   const EventReasonValue& subReason, int retry_after) 
	: req(req_), hash_str(hash_str_), sess_key(sess_key_),
    ID(subID), expires(sipExpires), subscriptionState(subState), 
    reason(subReason), retry_after(retry_after)
	{ created = time(NULL); }

    bool timeout() {
	return (!expires) || ((int)time(NULL) >= expires + (int) created);
    }

    void checkTimeout() {
	if (timeout()) {
	    subscriptionState = Terminated;
	    reason = Timeout;
	}
    }
    
    int getExpires() {
	int ret = expires + (int) created - (int)time(NULL);
	if (ret<0)
	    return 0; 
	else
	    return ret;
    }

    void refresh(int sipExpires, const SubstateValue& subState, 
		   const EventReasonValue& subReason, int retry_after_) {
	expires = sipExpires;
	subscriptionState = subState;
	reason = subReason;
	retry_after = retry_after_;
	created = (int) time(NULL);
    }
};


class AmResourceState {
    std::string r_state;
    std::string default_content_type;
 public:
    
    AmResourceState()  
	: default_content_type("text/plain") { }

    virtual ~AmResourceState() { }
    
    AmResourceState(std::string initial_state)
	: r_state(initial_state), 
	  default_content_type("text/plain") { }
     
    /**
     *  change the state for a registered resource.
     *  @param new_state    [in] the new state to be changed
     *  @return             true if state has changed so notifications should be sent
     */
    virtual bool setState(AmResourceState* new_state);
    
    /**
     *  set Authorization for Request (SUBSCRIBE) 
     *  for subscriptionState == [Pending, Terminated]
     *  set EventReasonValue (see RFC3256 3.2.4. )
     *  
     *  @param req         [in]  request (subscribe or implicit refer)
     *  @param retry_after [out] < 0 (or no change) means: 
     *                           do not include retry-after information
     *  @param expires     [in, out] expires value, in: requested, out: granted
     */ 
    virtual void getAuthorization(const AmRequestUAS& req, AmSubscription::SubstateValue& subscriptionState, 
			  AmSubscription::EventReasonValue& ReasonValue, int& expires, int& retry_after);

    virtual void refreshAuthorization(AmSubscription* existingSubscription, const AmRequestUAS& req,
			      AmSubscription::SubstateValue& subscriptionState, 
			      AmSubscription::EventReasonValue& ReasonValue, int& expires, int& retry_after);
    
    /** 
     *  get the state, to be sent to subscription subs
     *  @param subs          [in]  the subscription
     *  @param content_type  [out] content type for NOTIFY
     */
    virtual std::string getState(const AmSubscription* subs, std::string& content_type);
};


class AmSIPEvent : public AmEvent {
 public:
    AmRequestUAS request;
    std::string hash_str;
    std::string sess_key;

    enum EventType { Subscribe };
    AmSIPEvent(EventType event_type, const AmRequestUAS& request, 
	       const std::string& hash_str, const std::string& sess_key);
    std::string getEventPackage();
    std::string getEventPackageParameter(const std::string parameterName);
    std::string getResourceID() { return request.cmd.user; }
    void dbgEvent();
};

class AmResourceEvent : public AmEvent {
 public:
    enum EventType { RegisterResource, ChangeResourceState, RemoveResource };

    EventType type;
    std::string eventPackage;
    std::string resourceID;
    AmResourceState* resourceState;

    AmResourceEvent(EventType type, std::string eventPackage, 
		    std::string resourceID, AmResourceState* resourceState);
};



/**
 * RFC3265 SIP Event Notifiation Deamon.
 * It is designed as a singleton using a queue to get his work.
 * It wakes up only if there is anything to do.
 */

class AmSIPEventDaemon : public AmThread, 
			 public AmEventQueue, AmEventHandler
{
    static AmSIPEventDaemon* _instance;

    void run();
    void on_stop();

    // some typedefs for the resource states
    typedef std::map<std::string, AmResourceState* > ResourceStateMap;        // resourceID and state
    typedef std::map<std::string, ResourceStateMap > EventPackageMap;        // package name and resourcemap
    typedef ResourceStateMap::iterator ResourceStateMapIter;
    typedef EventPackageMap::iterator EventPackageMapIter;
   

    // some typedefs for the subscription table
    typedef std::vector<AmSubscription* > SubscriptionTable;                // the subscriptions
    typedef std::map<std::string, SubscriptionTable > SubscriptionIDMap;         // resourceID and subscriptions
    typedef std::map<std::string, SubscriptionIDMap > PackageSubscriptionMap;     // package name and subscription map

    typedef SubscriptionTable::iterator SubscriptionTableIter;
    typedef std::map<std::string, std::vector<AmSubscription* > >::iterator SubscriptionIDMapIter;    
    typedef std::map<std::string, SubscriptionIDMap >::iterator PackageSubscriptionMapIter;
    
    /** container for active subscriptions */
    PackageSubscriptionMap a_subscriptions;

    /** the resource states */
    EventPackageMap res_states;

    /** the daemon only runs if this is true */
    AmCondition<bool> _run_cond;
    
    /** add a subscription to the container */
    void addSubscription(const std::string& eventPackage,const std::string& resourceID, 
		     AmSubscription* subs);

    /** get a subscription by the dialog identifiers and the subscription ID */
    AmSubscription* getSubscription(const std::string& hash_str, const std::string& sess_key, 
				    const std::string& subscriptionID);

    /** get a list of subscriptions to a resource and dialog package */
    bool filterSubscriptions(const std::string& eventPackage, const std::string& resourceID, 
			     std::vector<AmSubscription*>& result);

    /** remove a subscription from the container */
    bool removeSubscription(const std::string& resourceID, const std::string& eventPackage, 
			    const std::string& subscriptionID);

    AmResourceState* getResourceState(std::string eventPackage, std::string resourceID);

    void onSubscribe(AmSIPEvent* event);    
    void onRegisterResource(AmResourceEvent* event);
    void onChangeResourceState(AmResourceEvent* event);
    void onUACRequestStatus(AmRequestUACStatusEvent* event);
    void onRemoveResource(AmResourceEvent* event);

    std::string getNewId();
    AmCmd sendNotify(std::string eventPackage, AmSubscription* subs, AmResourceState* r_state);

    std::string getEventPackageParameter(AmCmd& cmd, const std::string parameterName);
    std::string getEventPackage(AmCmd& cmd);

public:
    AmSIPEventDaemon();
    ~AmSIPEventDaemon();

    static AmSIPEventDaemon* instance();

    /** process the events from eventqueue */
    void process(AmEvent* event); 

    /**
     * register a resource
     *
     *  @param eventPackage    [in] event package name
     *  @param resourceID      [in] resource to be registered
     *  @param resourceState   [in] (initial) state of the resource
     *                              eventd takes ownership of resource state!
     */
    void registerResource(const std::string& eventPackage, const std::string& resourceID, 
			  AmResourceState* resourceState);

    /**
     * change the state of  a resource
     * will send notifies if a subscription exists
     *  @param eventPackage    [in] event package name
     *  @param resourceID      [in] resource 
     *  @param resourceState   [in] state of the resource
     *                              eventd takes ownership of resource state!
     */
    void changeResourceState(const std::string& eventPackage, const std::string& resourceID, 
			     AmResourceState* resourceState);

    /**
     * remove a resource
     * will send notifies if a subscription exists
     *  @param eventPackage    [in] event package name
     *  @param resourceID      [in] resource to be removed
     */
    void removeResource(const std::string& eventPackage, const std::string& resourceID);

    /** 
     * post a SIP request (SUBSCRIBE, REFER not yet implemented)) 
     * @return true if subscription found
     * @return false if subscription not found
     */
    bool postRequest(const std::string& hash_str,std::string& sess_key,
		     AmRequestUAS* req);
};
#endif
