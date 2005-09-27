/*
 * $Id: AmSIPEventDaemon.cpp,v 1.6.2.1 2005/09/02 13:47:46 rco Exp $
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

#include "AmSIPEventDaemon.h"
#include "log.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "AmSession.h"

void AmResourceState::getAuthorization(const AmRequestUAS& req, AmSubscription::SubstateValue& subscriptionState, 
				       AmSubscription::EventReasonValue& ReasonValue, int& expires, int& retry_after) { 
    if (expires) {
	subscriptionState = AmSubscription::Active; 
	ReasonValue = AmSubscription::NoReason; 
	retry_after = -1;
    } else {
	subscriptionState = AmSubscription::Terminated; 
	ReasonValue = AmSubscription::Timeout; 
    }
}

void AmResourceState::refreshAuthorization(AmSubscription* existingSubscription, const AmRequestUAS& req, 
					   AmSubscription::SubstateValue& subscriptionState, 
					   AmSubscription::EventReasonValue& ReasonValue, int& expires, int& retry_after) {
// handle it by default like a new subscription
    getAuthorization( req, subscriptionState, ReasonValue, expires, retry_after);
}


string AmResourceState::getState(const AmSubscription* subs, string& content_type) {
    if ((subs->subscriptionState == AmSubscription::Active) 
	||  ((subs->subscriptionState == AmSubscription::Terminated) && (subs->reason == AmSubscription::Timeout))){
	content_type = default_content_type;
	return r_state;
    }
    return "";
}

bool AmResourceState::setState(AmResourceState* new_state) {
    bool res = false;
    if (new_state->r_state != r_state) {
	res = true;
	r_state = new_state->r_state;
    }
    return res;
}

AmSIPEvent::AmSIPEvent(EventType event_type, const AmRequestUAS& request, 
		       const string& hash_str, const string& sess_key)
    : AmEvent(int(event_type)), request(request), 
      hash_str(hash_str), sess_key(sess_key)
{
}

void AmSIPEvent::dbgEvent() {
    DBG("SIPEvent *** \n hash_str = %s, sess_key = %s\n EventPackage = %s, ID = %s,\n"
	"CMD: To = %s, User = %s, Domain = %s, dstip = %s, \n"
	"     port = %s, r_uri = %s, from_uri = %s, from = %s\n"
	"     callid = %s, from_tag = %s, to_tag = %s\n",
	hash_str.c_str(), sess_key.c_str(), getEventPackage().c_str(), getEventPackageParameter("ID").c_str(),
	request.cmd.to.c_str(), request.cmd.user.c_str(), request.cmd.domain.c_str(), request.cmd.dstip.c_str(),
	request.cmd.port.c_str(), request.cmd.r_uri.c_str(), request.cmd.from_uri.c_str(),request.cmd.from.c_str(),
	request.cmd.callid.c_str(), request.cmd.from_tag.c_str(), request.cmd.to_tag.c_str());
}

string AmSIPEvent::getEventPackage() {
    string eventHeader = request.cmd.getHeader("Event");
    size_t pos = eventHeader.find(";");
    if (pos != string::npos) {
	eventHeader.erase(pos, eventHeader.size());
    }
    return eventHeader;
}

string AmSIPEvent::getEventPackageParameter(const string parameterName) {
    string eventHeader = request.cmd.getHeader("Event");
    string parameq(";"+parameterName+"=");
    string res = "";
    string::size_type p = eventHeader.find(parameq);
    if(p != string::npos){
	p += parameq.size();
	unsigned int p_end = p;
	while(p_end < eventHeader.length()){
	    if( eventHeader[p_end] == ';' )
		break;
	    p_end++;
	}
	res = eventHeader.substr(p,p_end-p);
    }
    return res;
//     parameq+="=";
//     int paraPos = eventHeader.find(parameq, eventHeader.find(";"));
//     if (!paraPos) 
// 	return "";
//     int paraEndPos = eventHeader.find(";", paraPos + parameq.size());
//     if (!paraEndPos) 
// 	paraEndPos = eventHeader.size();
//     return eventHeader.substr(paraPos+parameq.size(), paraEndPos);
}

AmResourceEvent::AmResourceEvent(EventType type, string eventPackage, 
		string resourceID, AmResourceState* resourceState) 
    : AmEvent((int)type), eventPackage(eventPackage), 
      resourceID(resourceID), resourceState(resourceState)
{
}

const string AmSubscription::getReasonString(const EventReasonValue& r) const {
    switch (r) {
	case Deactivated: return "Deactivated";
	case Probation: return "Probation"; 
	case Rejected: return "Rejected";
	case Timeout: return "Timeout";
	case Giveup: return "Giveup";
	case Noresource: return "Noresource";
	default: return "";
    }
}

// the daemon

AmSIPEventDaemon::AmSIPEventDaemon() 
    : AmEventQueue(this), _run_cond(false)
{
  AmSessionContainer::instance()->registerDaemon("AmSIPEventDaemon", this);
}

AmSIPEventDaemon::~AmSIPEventDaemon() {
    // clean up resource states
    // todo ?: check for disposability ?
    for (EventPackageMapIter evp_it = res_states.begin(); 
	 evp_it != res_states.end(); evp_it++) {
	for (ResourceStateMapIter rs_it = evp_it->second.begin(); 
	     rs_it != evp_it->second.end(); rs_it++) {
	    delete rs_it->second;
	}
    }
}

AmSIPEventDaemon* AmSIPEventDaemon::_instance=0;

AmSIPEventDaemon* AmSIPEventDaemon::instance()
{
    if(!_instance)
	_instance = new AmSIPEventDaemon();
    return _instance;
}

void AmSIPEventDaemon::run() {
    unsigned int check_cnt = 0;
    while(1){
	//_run_cond.wait_for();
#define SIPEVENTDAEMON_CHECK_INTERVAL_MS 30 // check every 30 millisec
	usleep(SIPEVENTDAEMON_CHECK_INTERVAL_MS * 1000);
	processEvents();
	
	if (! ((check_cnt++) % (1000 / SIPEVENTDAEMON_CHECK_INTERVAL_MS )) ) { // check about every second
//	    DBG("SIPEvent daemon checking expirations...\n");

	    for (PackageSubscriptionMapIter p_it = a_subscriptions.begin(); 
		 p_it != a_subscriptions.end(); p_it ++) {    // all packages
//		DBG("p_it->first = <%s>\n",p_it->first.c_str());

		for (SubscriptionIDMapIter resID_it = p_it->second.begin(); // all resources
		     resID_it != p_it->second.end(); resID_it++) {
//		    DBG("resID_it->first = <%s>\n", resID_it->first.c_str());

		    SubscriptionTableIter s_it = resID_it->second.begin();
		    while (s_it != resID_it->second.end()) { // all subscriptions
			AmSubscription* s = *s_it;
// 			DBG("package <%s>, resource <%s>, subscriptionID <%s> ends <%d>\n", 
// 			    p_it->first.c_str(),
// 			    resID_it->first.c_str(),
// 			    s->ID.c_str(), s->getExpires());
			
			if (s->timeout()) {
			    DBG("timeout for subscription to <%s> package <%s>\n", resID_it->first.c_str(), p_it->first.c_str());
			    s->checkTimeout();
			    AmResourceState* cur_state = getResourceState(p_it->first, resID_it->first);
			    //res_states[p_it->first][resID_it->first];
			    if (cur_state) {
				DBG("sending notify...\n");
				sendNotify(p_it->first, s, cur_state);
				DBG("erasing from subscription map...\n");
				s_it = resID_it->second.erase(s_it);
				DBG("deleting subscription...\n");
				delete s;
			    } else {
				// send "moved" notify and delete subscription
				s->subscriptionState = AmSubscription::Terminated;
				s->reason = AmSubscription::Noresource;
				DBG("resource state disappeared, sending empty notify \n");
				sendNotify(p_it->first, s, 0);
				DBG("erasing from subscription map...\n");
				s_it = resID_it->second.erase(s_it);
				DBG("deleting subscription...\n");
				delete s;
			    }			    
			} else {
			    s_it ++; 
			}
		    }
		}
	    }
//	    DBG("SIPEvent daemon checking expirations done.\n");
	}	
	//_run_cond.set(false);
    }
}

AmResourceState* AmSIPEventDaemon::getResourceState(string eventPackage, string resourceID) {
    EventPackageMapIter evp_it = res_states.find(eventPackage);
    if (evp_it!= res_states.end()) {
	ResourceStateMapIter rs_it = evp_it->second.find(resourceID);
	if (rs_it != evp_it->second.end()) { 
	    return rs_it->second;
	}
    }
    return NULL;
}


void AmSIPEventDaemon::on_stop()
{
}

void AmSIPEventDaemon::process(AmEvent* event) {
    AmSIPEvent* sip_ev = dynamic_cast<AmSIPEvent* >(event);
    if (sip_ev) {
	DBG("AmSIPEventDaemon received sip event.\n");
	sip_ev->dbgEvent();
	if (sip_ev->event_id == AmSIPEvent::Subscribe) {
	    onSubscribe(sip_ev);
	} else {
	    ERROR("SIPEvent with wrong type in SIPEventDaemon queue.\n");
	}
	return;
    }

    AmResourceEvent* res_ev = dynamic_cast<AmResourceEvent* >(event);
    if (res_ev) {
	if (res_ev->event_id == AmResourceEvent::RegisterResource) {
	    onRegisterResource(res_ev);
	} else if (res_ev->event_id == AmResourceEvent::ChangeResourceState) {
	    onChangeResourceState(res_ev);
	} else if (res_ev->event_id == AmResourceEvent::RemoveResource) {
	    onRemoveResource(res_ev);
	} else {
	    ERROR("ResourceEvent with wrong type in SIPEventDaemon queue.\n");
	}
	return;
    }

    AmRequestUACStatusEvent* reqst_ev = dynamic_cast<AmRequestUACStatusEvent* >(event);
    if (reqst_ev) {
	onUACRequestStatus(reqst_ev);
	return;
    }

    ERROR("Event with wrong type in SIPEventDaemon queue.\n");
}


void AmSIPEventDaemon::onRegisterResource(AmResourceEvent* event) {
    res_states[event->eventPackage][event->resourceID] = event->resourceState;
    DBG("registerresource: added new resource with package <%s> and ID <%s>.\n",
	event->eventPackage.c_str(), event->resourceID.c_str());    
}

void AmSIPEventDaemon::onRemoveResource(AmResourceEvent* event) {
    if ((res_states.find(event->eventPackage) == res_states.end()) ||
	res_states[event->eventPackage].find(event->resourceID) ==  
	res_states[event->eventPackage].end()) { // this resource is not known
	DBG("resource %s event package %s not found.\n", 
	    event->resourceID.c_str(), event->eventPackage.c_str());
	return;
    }
    vector<AmSubscription* > s;
    if (filterSubscriptions(event->eventPackage, event->resourceID, s)) {
	for (vector<AmSubscription* >::iterator it = s.begin(); it != s.end(); it++) {
	// send "moved" notify and delete subscription
	    (*it)->subscriptionState = AmSubscription::Terminated;
	    (*it)->reason = AmSubscription::Noresource;
	    sendNotify(event->eventPackage, *it, 0);
	    DBG("erasing subscription...\n");
	    removeSubscription(event->resourceID, event->eventPackage, (*it)->ID);
	}
    }
}


void AmSIPEventDaemon::onChangeResourceState(AmResourceEvent* event) {
    if ((res_states.find(event->eventPackage) == res_states.end()) ||
	res_states[event->eventPackage].find(event->resourceID) ==  
	res_states[event->eventPackage].end()) 
    { // this resource is new
	DBG("added new resource with package <%s> and ID <%s> on change resource state (!)\n"
	    "Please check implementation of the Event Daemon user.\n",
	    event->eventPackage.c_str(), event->resourceID.c_str());
	res_states[event->eventPackage][event->resourceID] = event->resourceState;
	return;
    }
    AmResourceState* cur_state = res_states[event->eventPackage][event->resourceID];
    if (cur_state->setState(event->resourceState)) {
	DBG("State of resource %s has changed (package %s)\n", 
	    event->resourceID.c_str(), event->eventPackage.c_str());
	// notify all subscribers to this resource...
	vector<AmSubscription*> fit_subscriptions;
	if (filterSubscriptions(event->eventPackage, event->resourceID, fit_subscriptions)) {
	    DBG("Notifying %ld subscribers about change in <%s> of resID <%s>\n",
		(long)fit_subscriptions.size(), event->eventPackage.c_str(), event->resourceID.c_str());
	    for (vector<AmSubscription*>::iterator it = fit_subscriptions.begin(); 
		 it != fit_subscriptions.end(); it++) {
		sendNotify(event->eventPackage, *it, cur_state);
	    }
	}
    }
    delete event->resourceState;
    event->resourceState = 0;
}

void AmSIPEventDaemon::onUACRequestStatus(AmRequestUACStatusEvent* event) {
    if (event->event_id == AmRequestUACStatusEvent::Accepted) {
	DBG("request accepted (method <%s>, from_uri <%s>)\n", 
	    event->request.cmd.method.c_str(), event->request.cmd.from_uri.c_str());
    } else {
	// todo: check other responses
	DBG("request not  accepted (method <%s>, from_uri <%s>): %d %s\n", 
	    event->request.cmd.method.c_str(), event->request.cmd.from_uri.c_str(),
	    event->code, event->reason.c_str());
	AmCmd& cmd = event->request.cmd;
	if (cmd.method == "NOTIFY" && event->code >= 400) { // remove subscription
	    string user = cmd.user;
	    string eventPackage = getEventPackage(cmd);
	    string subscriptionID = getEventPackageParameter(cmd, "ID");
	    
	    DBG("about to remove subscription for <%s> to <%s> ID <%s>\n", 
		user.c_str(), eventPackage.c_str(), subscriptionID.c_str());
	    if (!removeSubscription(user, eventPackage, subscriptionID)) {
		DBG("subscription not found.\n");
	    }
	}
    }
    
}

string AmSIPEventDaemon::getNewId() {
    return int2hex(getpid()) + int2hex(rand());
}

void AmSIPEventDaemon::onSubscribe(AmSIPEvent* event) {
    DBG("Searching Subscription...(hash = %s, key = %s)\n", event->hash_str.c_str(),event->sess_key.c_str());
    string subscriptionID = event->getEventPackageParameter("ID");
    AmSubscription* subs = AmSIPEventDaemon::getSubscription(event->hash_str,event->sess_key, subscriptionID);

    if (!subs) { // create new subscription
	DBG("No subscription found.\n");

	if (event->sess_key.empty()) {    // create new to-tag 
	    event->request.cmd.to_tag = getNewId();
	    event->sess_key = event->request.cmd.to_tag;
	}
    }

    string eventPackage = event->getEventPackage();
    EventPackageMapIter it = res_states.find(eventPackage);
    if (it == res_states.end()) { // unknown event package
	event->request.reply(489, "Bad Event");
	return;
    }
	
    string resourceID = event->getResourceID();
    DBG("searching for resource ID <%s>\n", resourceID.c_str());
    ResourceStateMapIter r_it = it->second.find(resourceID);
    if (r_it == it->second.end()) { // is this the correct reply if resource not found?
	DBG("resource not found. replying 404.\n");
	event->request.reply(404, "Not found"); 
	return;
    }

    AmResourceState* r_state = r_it->second;

    int expires = 0;
    sscanf(event->request.cmd.getHeader("Expires").c_str(), "%d", &expires);
    DBG("Got expires %d from \"Expires\" Header <%s> \n", expires, event->request.cmd.getHeader("Expires").c_str());
	
//    expires = 20;

    AmSubscription::SubstateValue sub_state;
    AmSubscription::EventReasonValue event_reason;
    int retry_after = -1;
	
    if (!subs)
	r_state->getAuthorization(event->request, sub_state, event_reason, expires, retry_after);
    else 
	r_state->refreshAuthorization(subs, event->request, sub_state, event_reason, expires, retry_after);
	
    bool send_notify = false;
    bool subscribe_this = false;
    if (sub_state == AmSubscription::Active) {
	event->request.reply(200, "OK");
	send_notify = true;
	subscribe_this = true;
    } else if (sub_state == AmSubscription::Pending) { // waiting for authorization
	subscribe_this = true;
	event->request.reply(202, "Accepted");
    } else if (sub_state == AmSubscription::Terminated){
	if (event_reason == AmSubscription::Timeout) {
	    send_notify = true;
	    event->request.reply(200, "OK");
	}
    }
    
    AmSubscription* new_subscription;
    if (!subs)
	new_subscription = new AmSubscription(event->request, event->hash_str, 
					      event->sess_key, subscriptionID, 
					      expires, sub_state, event_reason, retry_after );
    else 
	subs->refresh(expires, sub_state, event_reason, retry_after);

    if (send_notify) 
	sendNotify(eventPackage, subs?subs:new_subscription, r_state);

    if (!subs && subscribe_this)
	addSubscription(eventPackage, resourceID, new_subscription);
}

void AmSIPEventDaemon::addSubscription(const string& eventPackage,const string& resourceID, 
		     AmSubscription* subs) {
    a_subscriptions[eventPackage][resourceID].push_back(subs);
}

AmCmd AmSIPEventDaemon::sendNotify(string eventPackage, AmSubscription* subs, 
				   AmResourceState* r_state) {
    string body = "";
    string content_type = "";

    AmCmd cmd;
    cmd.cmd       = "eventd";
    cmd.method    = "NOTIFY";//+ subs->req.cmd.from_uri;
    cmd.r_uri = subs->req.cmd.from_uri;
    cmd.user      = subs->req.cmd.user;
    cmd.dstip    = AmConfig::LocalIP;
    cmd.from      = subs->req.cmd.to;
    cmd.from_uri  = subs->req.cmd.from_uri;
    cmd.from_tag  = subs->req.cmd.from_tag;
    cmd.to        = subs->req.cmd.to;      //from;
    cmd.to_tag    = subs->req.cmd.to_tag; //from_tag;
    cmd.callid    = subs->req.cmd.callid;
    cmd.cseq      = 20;

    cmd.hdrs     += "Event:"+eventPackage;
    if (!subs->ID.empty()) {
	cmd.hdrs += ";ID="+subs->ID;
    }
    cmd.hdrs+= "\n";
    
    cmd.hdrs  += "Subscription-State:";
    switch (subs->subscriptionState) {
	case AmSubscription::Active : {
	    cmd.hdrs  += "Active;expires="+int2str(subs->getExpires()) + "\n";

	    if (r_state)
		body = r_state->getState(subs, content_type);
	}; break;
	case AmSubscription::Pending : {
	    cmd.hdrs  += "Pending;expires="+int2str(subs->getExpires());
	    if (subs->reason != AmSubscription::NoReason) 
		cmd.hdrs += ";Reason="+subs->getReasonString(subs->reason);
	    cmd.hdrs += "\n";
	}; break;
	case AmSubscription::Terminated : {
	    cmd.hdrs  += "Terminated";
	    if (subs->reason != AmSubscription::NoReason) 
		cmd.hdrs += ";Reason="+subs->getReasonString(subs->reason);
	    cmd.hdrs += "\n";
	    if (r_state)
		body = r_state->getState(subs, content_type);
	}; break;
    }

    DBG("NOTIFY built with content_type = <%s>, body = <%s>; starting ASyncRequest\n", 
	content_type.c_str(), body.c_str());
    
    AmASyncRequestUAC* req = new AmASyncFullRequestUAC(cmd, body, content_type, DAEMON_TYPE_ID, "AmSIPEventDaemon");
    req->start();
    AmThreadWatcher::instance()->add(req);

    return cmd;
}

// unsafe!!!
AmSubscription* AmSIPEventDaemon::getSubscription(const string& hash_str, const string& sess_key, 
						  const string& subscriptionID)
{
    for (PackageSubscriptionMapIter p_it = a_subscriptions.begin(); 
	 p_it != a_subscriptions.end(); p_it ++) {
	for (SubscriptionIDMapIter resID_it = p_it->second.begin(); 
	     resID_it != p_it->second.end(); resID_it++) {
	    for (SubscriptionTableIter s_it = resID_it->second.begin();  
		 s_it != resID_it->second.end(); s_it ++ ) {
		if (((*s_it)->hash_str == hash_str) && 
		    ((*s_it)->sess_key == sess_key) &&
		    ((*s_it)->ID == subscriptionID)) {
		    return *s_it;
		}
	    }
	}
    }
    return NULL;
}

/** get a list of subscriptions to a resource and dialog package */
bool AmSIPEventDaemon::filterSubscriptions(const string& eventPackage, const string& resourceID, 
			 vector<AmSubscription*>& result) {
    DBG("trying to get subscriptions for eventPackage %s, resource ID %s\n",
	eventPackage.c_str(), resourceID.c_str());
    bool res = false;

    PackageSubscriptionMapIter p_it = a_subscriptions.find(eventPackage);
    if (p_it != a_subscriptions.end()) {
	DBG("found event Package %s...\n", eventPackage.c_str());

	SubscriptionIDMapIter resID_it = p_it->second.find(resourceID);
	if (resID_it != p_it->second.end()) {
	    DBG("found resource %s.\n", resourceID.c_str());

	    for (SubscriptionTableIter sID_it = resID_it->second.begin(); 
		 sID_it != resID_it->second.end(); sID_it++) {
		result.push_back(*sID_it);
		res = true;
	    }
	}
    }

    return res;
}


void AmSIPEventDaemon::registerResource(const string& eventPackage, const string& resourceID, 
					 AmResourceState* resourceState) {
    postEvent(new AmResourceEvent(AmResourceEvent::RegisterResource, eventPackage, 
				  resourceID, resourceState));
}

void AmSIPEventDaemon::changeResourceState(const string& eventPackage, const string& resourceID, 
			 AmResourceState* resourceState) {
    postEvent(new AmResourceEvent(AmResourceEvent::ChangeResourceState, eventPackage, 
				  resourceID, resourceState));
}

void AmSIPEventDaemon::removeResource(const string& eventPackage, const string& resourceID) { 
    postEvent(new AmResourceEvent(AmResourceEvent::RemoveResource, eventPackage, 
				  resourceID, 0));
}

bool AmSIPEventDaemon::postRequest(const string& hash_str,string& sess_key,
				 AmRequestUAS* req) {
    AmSIPEvent* s;

    if (req->cmd.method == "SUBSCRIBE") 
     s = new AmSIPEvent(AmSIPEvent::Subscribe, *req, hash_str, sess_key);
    else {
	ERROR("trying to post Event to AmSIPEventDaemon with wrong method.\n");
	return false;
    }
    // post event to eventQueue
    postEvent(s);

    return true;
}

string AmSIPEventDaemon::getEventPackageParameter(AmCmd& cmd, const string parameterName) {
    string eventHeader = cmd.getHeader("Event");
    string parameq(";"+parameterName+"=");
    string res = "";
    string::size_type p = eventHeader.find(parameq);
    if(p != string::npos){
	p += parameq.size();
	unsigned int p_end = p;
	while(p_end < eventHeader.length()){
	    if( eventHeader[p_end] == ';' )
		break;
	    p_end++;
	}
	res = eventHeader.substr(p,p_end-p);
    }
    return res;
}

string  AmSIPEventDaemon::getEventPackage(AmCmd& cmd) {
    string eventHeader = cmd.getHeader("Event");
    size_t pos = eventHeader.find(";");
    if (pos != string::npos) {
	eventHeader.erase(pos, eventHeader.size());
    }
    return eventHeader;
}

bool AmSIPEventDaemon::removeSubscription(const string& resourceID, const string& eventPackage, 
					  const string& subscriptionID) {
    PackageSubscriptionMapIter p_it = a_subscriptions.find(eventPackage);
    if (p_it != a_subscriptions.end()) {
	DBG("found event Package %s...\n", eventPackage.c_str());

	SubscriptionIDMapIter resID_it = p_it->second.find(resourceID);
	if (resID_it != p_it->second.end()) {
	    DBG("found resource %s.\n", resourceID.c_str());

	    for (SubscriptionTableIter sID_it = resID_it->second.begin(); 
		 sID_it != resID_it->second.end(); sID_it++) {
		if ((*sID_it)->ID == subscriptionID) {
		    DBG("found subscription %s, deleteing.\n", subscriptionID.c_str());

		    AmSubscription* s = *sID_it;
		    resID_it->second.erase(sID_it);
		    delete s;
		    return true;
		}
	    }
	}
    }

    return false;
}
