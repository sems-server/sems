/*
 * Copyright (C) 2012 Frafos GmbH
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

#include "AmSipSubscription.h"
#include "AmEventQueue.h"
#include "AmSipHeaders.h"
#include "AmAppTimer.h"
#include "AmUtils.h"
#include "jsonArg.h"

#include "AmSession.h" // getNewId()
#include "AmSessionContainer.h"

#include "sip/sip_timers.h"
#include "log.h"

#include <assert.h>

#define DEFAULT_SUB_EXPIRES 600

// TIMER N should first expire once transaction timer has hit
// in case we receive no reply to SUBSCRIBE.
#define RFC6665_TIMER_N_DURATION ((64 + 4)*T1_TIMER)/1000.0

#define SIP_HDR_SUBSCRIPTION_STATE "Subscription-State"
#define SIP_HDR_EVENT              "Event"

const char* __timer_id_str[2] = {
  "RFC6665_TIMER_N",
  "SUBSCRIPTION_EXPIRE" 
};

const char* __sub_state_str[] = {
  "init",
  "notify_wait",
  "pending",
  "active",
  "terminated"
};

SingleSubscription::SingleSubscription(AmSipSubscription* subs, Role role,
				       const string& event, const string& id)
    : sub_state(SubState_init), pending_subscribe(0), expires(0), timer_n(this,RFC6665_TIMER_N), timer_expires(this,SUBSCRIPTION_EXPIRE),
      subs(subs), event(event),
      id(id),role(role)
{
  assert(subs); 
}

AmBasicSipDialog* SingleSubscription::dlg()
{
  return subs->dlg;
}

void SingleSubscription::onTimer(int timer_id)
{
  DBG("[%p] tag=%s;role=%s timer_id = %s\n",this,
      dlg()->getLocalTag().c_str(),
      role ? "Notifier" : "Subscriber",
      __timer_id_str[timer_id]);

  switch(timer_id){
  case RFC6665_TIMER_N:
  case SUBSCRIPTION_EXPIRE:
    if(subs->ev_q) {
      AmEvent* ev = new SingleSubTimeoutEvent(subs->dlg->getLocalTag(),
					      timer_id,this);
      subs->ev_q->postEvent(ev);
    }
    return;
  }
}

void SingleSubscription::terminate()
{
  setState(SubState_terminated);
}

bool SingleSubscription::terminated()
{
  return getState() == SubState_terminated;
}

SingleSubscription* 
AmSipSubscription::newSingleSubscription(SingleSubscription::Role role,
					 const string& event, const string& id)
{
  return new SingleSubscription(this,role,event,id);
}

SingleSubscription* AmSipSubscription::makeSubscription(const AmSipRequest& req,
							bool uac)
{
  SingleSubscription::Role role = 
    uac ? 
    SingleSubscription::Subscriber : 
    SingleSubscription::Notifier;

  string event;
  string id;

  if(req.method == SIP_METH_SUBSCRIBE) {
    // fetch Event-HF
    event = getHeader(req.hdrs,SIP_HDR_EVENT,true);
    id = get_header_param(event,"id");
    event = strip_header_params(event);
  }
  else if(req.method == SIP_METH_REFER) {
    //TODO: fetch Refer-Sub-HF (RFC 4488)
    event = "refer";
    id = int2str(req.cseq);
  } 
  else {
    DBG("subscription are only created by SUBSCRIBE or REFER requests\n");
    // subscription are only created by SUBSCRIBE or REFER requests
    // and we do not support unsolicited NOTIFYs
    return NULL;
  }

  return newSingleSubscription(role,event,id);
}

SingleSubscription::~SingleSubscription()
{
  // just to be sure...
  AmAppTimer::instance()->removeTimer(&timer_n);
  // this one should still be active
  AmAppTimer::instance()->removeTimer(&timer_expires);
}

void SingleSubscription::requestFSM(const AmSipRequest& req)
{
  if((req.method == SIP_METH_SUBSCRIBE) ||
     (req.method == SIP_METH_REFER)) {

    if(getState() == SubState_init) {
      setState(SubState_notify_wait);
    }

    // start Timer N (RFC6665/4.1.2)
    DBG("setTimer(%s,RFC6665_TIMER_N)\n",dlg()->getLocalTag().c_str());
    AmAppTimer::instance()->setTimer(&timer_n,RFC6665_TIMER_N_DURATION);
  }
  else if(req.method == SIP_METH_NOTIFY) {
    subs->onNotify(req,this);
  }
}

bool SingleSubscription::onRequestIn(const AmSipRequest& req)
{
  if((req.method == SIP_METH_SUBSCRIBE) ||
     (req.method == SIP_METH_REFER)) {

    if(pending_subscribe) {
      dlg()->reply(req,500, SIP_REPLY_SERVER_INTERNAL_ERROR, NULL,
		   SIP_HDR_COLSP(SIP_HDR_RETRY_AFTER) 
		   + int2str(get_random() % 10) + CRLF);
      return false;
    }
    pending_subscribe++;
  }

  requestFSM(req);
  return true;
}

void SingleSubscription::onRequestSent(const AmSipRequest& req)
{
  //TODO: check pending_subscribe in onSendRequest
  if((req.method == SIP_METH_SUBSCRIBE) ||
     (req.method == SIP_METH_REFER)) {
    pending_subscribe++;
  }
  requestFSM(req);
}

void SingleSubscription::replyFSM(const AmSipRequest& req, const AmSipReply& reply)
{
  if(reply.code < 200)
    return;

  if((req.method == SIP_METH_SUBSCRIBE) ||
     (req.method == SIP_METH_REFER)) {
    // final reply

    if(reply.code >= 300) {
      if(getState() == SubState_notify_wait) {
	// initial SUBSCRIBE failed
	terminate();
	subs->onFailureReply(reply,this);
      }
      else {
	// subscription refresh failed
	// from RFC 5057: terminate usage
	switch(reply.code){
	case 405:
	case 489:
	case 481:
	case 501:
	  terminate();
	  subs->onFailureReply(reply,this);
	  break;
	}
      }
    }
    else {
      // success
      
      // set dialog identifier if not yet set
      if(dlg()->getRemoteTag().empty()) {
	dlg()->setRemoteTag(reply.to_tag);
	dlg()->setRouteSet(reply.route);
      }

      // check Expires-HF
      string expires_txt = getHeader(reply.hdrs,SIP_HDR_EXPIRES,true);
      expires_txt = strip_header_params(expires_txt);

      int sub_expires=0;
      if(!expires_txt.empty() && str2int(expires_txt,sub_expires)){
	if(sub_expires){
	  DBG("setTimer(%s,SUBSCRIPTION_EXPIRE)\n",dlg()->getLocalTag().c_str());
	  AmAppTimer::instance()->setTimer(&timer_expires,(double)sub_expires);
	  expires = sub_expires + AmAppTimer::instance()->unix_clock.get();

	  DBG("removeTimer(%s,RFC6665_TIMER_N)\n",dlg()->getLocalTag().c_str());
	  AmAppTimer::instance()->removeTimer(&timer_n);
	}
	else {
	  // we do not care too much, as timer N is set
	  // for each SUBSCRIBE request
	  DBG("Expires-HF equals 0\n");
	}
      }
      else if(reply.cseq_method == SIP_METH_SUBSCRIBE){
	// Should we really enforce that?
	// -> we still have timer N...

	// replies to SUBSCRIBE MUST contain a Expires-HF
	// if not, or if not readable, we should probably 
	// quit the subscription
	DBG("replies to SUBSCRIBE MUST contain a Expires-HF\n");
	terminate();
	subs->onFailureReply(reply,this);
      }
    }

    pending_subscribe--;
  }
  else if(reply.cseq_method == SIP_METH_NOTIFY) {

    if(reply.code >= 300) {
      // final error reply
      // from RFC 5057: terminate usage
      switch(reply.code){
      case 405:
      case 481:
      case 489:
      case 501:
	terminate();
	subs->onFailureReply(reply,this);
	break;
	
      default:
	// all other response codes:
	// only the transaction fails
	break;
      }

      return;
    }
    
    // check Subscription-State-HF
    string sub_state_txt = getHeader(req.hdrs,SIP_HDR_SUBSCRIPTION_STATE,true);
    string expires_txt = get_header_param(sub_state_txt,"expires");
    int notify_expire=0;
  
    if(!expires_txt.empty())
      str2int(expires_txt,notify_expire);

    // Kill timer N
    DBG("removeTimer(%s,RFC6665_TIMER_N)\n",dlg()->getLocalTag().c_str());
    AmAppTimer::instance()->removeTimer(&timer_n);

    sub_state_txt = strip_header_params(sub_state_txt);
    if(notify_expire && (sub_state_txt == "active")) {
      setState(SubState_active);
    }
    else if(notify_expire && (sub_state_txt == "pending")){
      setState(SubState_pending);
    }
    else {
      terminate();
      //subs->onFailureReply(reply,this);
      return;
    }
    
    // reset expire timer
    DBG("setTimer(%s,SUBSCRIPTION_EXPIRE)\n",dlg()->getLocalTag().c_str());
    AmAppTimer::instance()->setTimer(&timer_expires,(double)notify_expire);
    expires = notify_expire + AmAppTimer::instance()->unix_clock.get();
  }

  return;
}

void SingleSubscription::setExpires(unsigned long exp)
{
  double notify_expire = exp - AmAppTimer::instance()->unix_clock.get();
  if(notify_expire > 0.0) {
    AmAppTimer::instance()->setTimer(&timer_expires,notify_expire);
    expires = exp;
  }
  else {
    DBG("new 'expires' is already expired: sending event");
    onTimer(SUBSCRIPTION_EXPIRE);
  }
}

void SingleSubscription::setState(unsigned int st)
{
  DBG("st = %s\n",__sub_state_str[st]);

  if(sub_state == SubState_terminated)
    return;

  if(st == SubState_terminated) {
    sub_state = SubState_terminated;
    dlg()->decUsages();
  }
  else {
    sub_state = st;
  }
}

string SingleSubscription::to_str()
{
  return "[" 
    + str2json(event) + ","
    + str2json(id) + ","
    + (role == Subscriber ? str2json("SUB") : str2json("NOT")) + ","
    + str2json(__sub_state_str[sub_state]) + "]";
}

/**
 * AmSipSubscription
 */

AmSipSubscription::AmSipSubscription(AmBasicSipDialog* dlg, AmEventQueue* ev_q)
  : dlg(dlg), ev_q(ev_q)
{
  assert(dlg);
}

AmSipSubscription::~AmSipSubscription()
{
  while(!subs.empty()) {
    DBG("removing single subscription");
    removeSubscription(subs.begin());
  }
}

bool AmSipSubscription::isActive()
{
  for(Subscriptions::iterator it=subs.begin();
      it != subs.end(); it++) {
    if((*it)->getState() == SingleSubscription::SubState_active)
      return true;
  }

  return false;
}

void AmSipSubscription::terminate()
{
  for(Subscriptions::iterator it=subs.begin();
      it != subs.end(); it++) {
    (*it)->terminate();
  }
}

bool AmSipSubscription::subscriptionExists(SingleSubscription::Role role,
					   const string& event, const string& id)
{
  return findSubscription(role,event,id) != subs.end();
}

AmSipSubscription::Subscriptions::iterator 
AmSipSubscription::findSubscription(SingleSubscription::Role role,
				    const string& event, const string& id)
{
  Subscriptions::iterator match = subs.end();
  bool no_id = id.empty() && (event == "refer");

  DBG("searching for event='%s'; id='%s'; no_id=%i",
      event.c_str(),id.c_str(),no_id);

  for(Subscriptions::iterator it = subs.begin();
      it != subs.end(); it++) {

    SingleSubscription* sub = *it;

    DBG("role='%s';event='%s';id='%s'",
	sub->role ? "Notifier" : "Subscriber",
	sub->event.c_str(), sub->id.c_str());

    if( (sub->role == role) &&
	(sub->event == event) &&
	(no_id || (sub->id == id)) ){

      match = it;
      DBG("\tmatched!");
      break;
    }
  }

  if((match != subs.end()) && (*match)->terminated()) {
    DBG("matched terminated subscription: deleting it first\n");
    removeSubscription(match);
    match = subs.end();
  }

  return match;
}

AmSipSubscription::Subscriptions::iterator
AmSipSubscription::createSubscription(const AmSipRequest& req, bool uac)
{
  SingleSubscription* sub = makeSubscription(req,uac);
  if(!sub){
    return subs.end();
  }

  dlg->incUsages();
  DBG("new subscription: %s",sub->to_str().c_str());
  return subs.insert(subs.end(),sub);
}

void AmSipSubscription::removeSubFromUACCSeqMap(Subscriptions::iterator sub)
{
  for (CSeqMap::iterator i = uac_cseq_map.begin(); i != uac_cseq_map.end();) {
    if (i->second == sub) {
      DBG("removing UAC subnot transaction with cseq=%i",i->first);
      CSeqMap::iterator del_i = i; ++i;
      uac_cseq_map.erase(del_i);
      continue;
    }
    ++i;
  }
}

void AmSipSubscription::removeSubFromUASCSeqMap(Subscriptions::iterator sub)
{
  for (CSeqMap::iterator i = uas_cseq_map.begin(); i != uas_cseq_map.end();) {
    if (i->second == sub) {

      unsigned int cseq = i->first;
      CSeqMap::iterator del_i = i; ++i;
      uas_cseq_map.erase(del_i);

      DBG("removed UAS subnot transaction with cseq=%i",cseq);

      // reply pending UAS transaction
      AmSipRequest* req = dlg->getUASTrans(cseq);
      if(req) {
	DBG("found request(cseq=%i): replying 481 to pending UAS transaction",
	    req->cseq);
	dlg->reply(*req,481,SIP_REPLY_NOT_EXIST);
      }
      else {
	DBG("request not found: could not reply 481 to pending UAS transaction");
      }

      continue;
    }

    ++i;
  }
}

void AmSipSubscription::removeSubscription(Subscriptions::iterator sub)
{
  removeSubFromUACCSeqMap(sub);
  removeSubFromUASCSeqMap(sub);

  delete *sub;
  subs.erase(sub);
}

/**
 * match single subscription
 * if none, create one
 */
AmSipSubscription::Subscriptions::iterator
AmSipSubscription::matchSubscription(const AmSipRequest& req, bool uac)
{
  if((!uac && req.to_tag.empty()) || (uac && dlg->getRemoteTag().empty())
     || (req.method == SIP_METH_REFER) || subs.empty()) {

    DBG("no to-tag, REFER or subs empty: create new subscription\n");
    return createSubscription(req,uac);
  }

  SingleSubscription::Role role;
  string event;
  string id;

  if(req.method == SIP_METH_SUBSCRIBE) {
    role = uac ? SingleSubscription::Subscriber : SingleSubscription::Notifier;
  }
  else if(req.method == SIP_METH_NOTIFY){
    role = uac ? SingleSubscription::Notifier : SingleSubscription::Subscriber;
  }
  else {
    DBG("unsupported request\n");
    return subs.end();
  }

  // parse Event-HF
  event = getHeader(req.hdrs,SIP_HDR_EVENT,true);
  id = get_header_param(event,"id");
  event = strip_header_params(event);

  Subscriptions::iterator match = findSubscription(role,event,id);
  if(match == subs.end()){
    if(req.method == SIP_METH_SUBSCRIBE) {
      // no match... new subscription?
      DBG("no match found, SUBSCRIBE: create new subscription\n");
      return createSubscription(req,uac);
    }
  }

  return match;
}

bool AmSipSubscription::onRequestIn(const AmSipRequest& req)
{
  // UAS side
  Subscriptions::iterator sub_it = matchSubscription(req,false);
  if((sub_it == subs.end()) || (*sub_it)->terminated()) {

    if((sub_it == subs.end()) && (req.method == SIP_METH_NOTIFY)
       && allow_subless_notify) {
      return true;
    }

    dlg->reply(req, 481, SIP_REPLY_NOT_EXIST);
    return false;
  }
  
  // process request;
  uas_cseq_map[req.cseq] = sub_it;
  return (*sub_it)->onRequestIn(req);
}

void AmSipSubscription::onRequestSent(const AmSipRequest& req)
{
  // UAC side
  Subscriptions::iterator sub_it = matchSubscription(req,true);
  if(sub_it == subs.end()){

    if((req.method == SIP_METH_NOTIFY)
       && allow_subless_notify) {
      return;
    }

    // should we exclude this case in onSendRequest???
    ERROR("we just sent a request for which we could obtain no subscription\n");
    return;
  }

  // process request;
  uac_cseq_map[req.cseq] = sub_it;
  (*sub_it)->onRequestSent(req);
}

bool AmSipSubscription::onReplyIn(const AmSipRequest& req,
				  const AmSipReply& reply)
{
  // UAC side
  CSeqMap::iterator cseq_it = uac_cseq_map.find(req.cseq);
  if(cseq_it == uac_cseq_map.end()){

    if((req.method == SIP_METH_NOTIFY)
       && allow_subless_notify) {
      return true;
    }

    DBG("could not find %i in our uac_cseq_map\n",req.cseq);
    return false;
  }

  Subscriptions::iterator sub_it = cseq_it->second;
  SingleSubscription* sub = *sub_it;
  uac_cseq_map.erase(cseq_it);

  sub->replyFSM(req,reply);
  if(sub->terminated()){
    removeSubscription(sub_it);
  }

  return true;
}

void AmSipSubscription::onReplySent(const AmSipRequest& req,
				    const AmSipReply& reply)
{
  // UAS side
  CSeqMap::iterator cseq_it = uas_cseq_map.find(req.cseq);
  if(cseq_it == uas_cseq_map.end())
    return;

  Subscriptions::iterator sub_it = cseq_it->second;
  SingleSubscription* sub = *sub_it;
  uas_cseq_map.erase(cseq_it);

  sub->replyFSM(req,reply);
  if(sub->terminated()){
    removeSubscription(sub_it);
  }
}

void AmSipSubscription::onTimeout(int timer_id, SingleSubscription* sub)
{
  Subscriptions::iterator it = subs.begin();
  for(; it != subs.end(); it++) {
    if(*it == sub) break;
  }
  if(it == subs.end())
    return; // no match...

  sub->terminate();
  removeSubscription(it);
}

void AmSipSubscription::debug()
{
  DBG("subscriptions with lt=%s:",dlg->getLocalTag().c_str());
  for(Subscriptions::iterator it = subs.begin(); it != subs.end(); it++) {
    DBG("\t%s",(*it)->to_str().c_str());
  }
}


SIPSubscriptionEvent::SIPSubscriptionEvent(SubscriptionStatus status, 
					   const string& handle,
					   unsigned int expires,
					   unsigned int code, 
					   const string& reason)
  : AmEvent(E_SIP_SUBSCRIPTION), handle(handle),
    code(code), reason(reason), status(status),
    expires(expires), notify_body(nullptr)
{}
  
const char* SIPSubscriptionEvent::getStatusText() 
{
  switch (status) {
  case SubscribeActive: return "active";
  case SubscribeFailed: return "failed";
  case SubscribeTerminated: return "terminated";
  case SubscribePending: return "pending";
  case SubscriptionTimeout: return "timeout";
  }
  return "unknown";
}

AmSipSubscriptionDialog::AmSipSubscriptionDialog(const AmSipSubscriptionInfo& info,
						 const string& sess_link,
						 AmEventQueue* ev_q)
  : AmBasicSipDialog(this),
    AmSipSubscription(this,ev_q),
    sess_link(sess_link)
{
  user   = info.user;
  domain = info.domain;

  local_uri    = "sip:"+info.from_user+"@"+info.domain;
  local_party  = "<"+local_uri+">";

  remote_uri   = "sip:"+info.user+"@"+info.domain;
  remote_party = "<"+remote_uri+">";

  callid    = AmSession::getNewId();
  local_tag = AmSession::getNewId();

  event    = info.event;
  event_id = info.id;
  accept   = info.accept;
}

int AmSipSubscriptionDialog::subscribe(int expires)
{
  string hdrs;

  if(!event.empty()){
    hdrs += SIP_HDR_COLSP(SIP_HDR_EVENT) + event;
    if(!event_id.empty())
      hdrs += ";id=" + event_id;
    hdrs += CRLF;
  }

  if (!accept.empty()) {
    hdrs += SIP_HDR_COLSP(SIP_HDR_ACCEPT) + accept + CRLF;
  }
  if (expires >= 0) {
    hdrs += SIP_HDR_COLSP(SIP_HDR_EXPIRES) + int2str(expires)  + CRLF;
  }

  return sendRequest(SIP_METH_SUBSCRIBE,NULL,hdrs);
}

string AmSipSubscriptionDialog::getDescription()
{
  return "'"+user+"@"+domain+", Event: "+event+"/"+event_id+"'";
}

void AmSipSubscriptionDialog::onNotify(const AmSipRequest& req,
				       SingleSubscription* sub)
{
  assert(sub);

  // subscription state is update after the reply has been sent
  reply(req, 200, "OK");

  SIPSubscriptionEvent* sub_ev = 
    new SIPSubscriptionEvent(SIPSubscriptionEvent::SubscribeFailed, local_tag);
  
  switch(sub->getState()){
  case SingleSubscription::SubState_pending:
    sub_ev->status = SIPSubscriptionEvent::SubscribePending;
    sub_ev->expires = sub->getExpires();
    break;
  case SingleSubscription::SubState_active:
    sub_ev->status = SIPSubscriptionEvent::SubscribeActive;
    sub_ev->expires = sub->getExpires();
    break;
  case SingleSubscription::SubState_terminated:
    sub_ev->status = SIPSubscriptionEvent::SubscribeTerminated;
    break;
  default:
    break;
  }

  if(!req.body.empty())
    sub_ev->notify_body.reset(new AmMimeBody(req.body));

  DBG("posting event to '%s'\n", sess_link.c_str());
  AmSessionContainer::instance()->postEvent(sess_link, sub_ev);
}

void AmSipSubscriptionDialog::onFailureReply(const AmSipReply& reply,
					     SingleSubscription* sub)
{
  assert(sub);
  SIPSubscriptionEvent* sub_ev =
    new SIPSubscriptionEvent(SIPSubscriptionEvent::SubscribeFailed, 
			     local_tag, 0, reply.code, reply.reason);

  DBG("posting event to '%s'\n", sess_link.c_str());
  AmSessionContainer::instance()->postEvent(sess_link, sub_ev);
}

void AmSipSubscriptionDialog::onTimeout(int timer_id, SingleSubscription* sub)
{
  AmSipSubscription::onTimeout(timer_id,sub);

  // possibly we've got a timeout for an already destroyed subscription.
  // however, it only happens if the subscription has been destroyed right
  // before this event (same processEvents() call).

  SIPSubscriptionEvent* sub_ev =
    new SIPSubscriptionEvent(SIPSubscriptionEvent::SubscriptionTimeout, local_tag);

  DBG("posting event to '%s'\n", sess_link.c_str());
  AmSessionContainer::instance()->postEvent(sess_link, sub_ev);
}
