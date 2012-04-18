/*
 * Copyright (C) 2012 FRAFOS GmbH
 *
 * Development sponsored by Sipwise GmbH.
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

#include "AmSipSubscriptionContainer.h"
#include "AmSession.h"
#include "AmEventDispatcher.h"

#define SUBSCRIPTION_CONTAINER_EVQ_ID "_subscription_container_"

_AmSipSubscriptionContainer::_AmSipSubscriptionContainer()
 : initialized(false)
{
}

_AmSipSubscriptionContainer::~_AmSipSubscriptionContainer() {
}

void _AmSipSubscriptionContainer::initialize() {
  if (!initialized) {
    // AmEventDispatcher::instance()->addEventQueue(SUBSCRIPTION_CONTAINER_EVQ_ID, this);
    initialized = true;
    DBG("Starting SIP Subscription client thread ([%p])\n", this);
    start();
  }
}

string _AmSipSubscriptionContainer::createSubscription(const AmSipSubscriptionInfo& info,
						       const string& sess_link,
						       unsigned int wanted_expires) {
  initialize();
  string handle = AmSession::getNewId();
  AmSipSubscription* new_sub = new AmSipSubscription(handle, info, sess_link);
  new_sub->setExpiresInterval(wanted_expires);

  subscriptions_mut.lock();
  subscriptions[handle] = new_sub;
  subscriptions_mut.unlock();

  AmEventDispatcher::instance()->addEventQueue(handle, this);
  if (!new_sub->doSubscribe()) {
    DBG("subscribe failed - removing subscription\b");
    AmEventDispatcher::instance()->delEventQueue(handle);
    subscriptions_mut.lock();
    subscriptions.erase(handle);
    subscriptions_mut.unlock();
    delete new_sub;
    return "";
  }

  return handle;
}

bool _AmSipSubscriptionContainer::refreshSubscription(const string& sub_handle,
						      unsigned int wanted_expires) {
  bool res = true;
  subscriptions_mut.lock();
  AmSipSubscriptionMapIter it = subscriptions.find(sub_handle);
  if (it != subscriptions.end()) {
    DBG("refreshing subscription '%s'\n", sub_handle.c_str());
    it->second->setExpiresInterval(wanted_expires);
    res = it->second->reSubscribe();
  } else {
    DBG("subscription '%s' already removed\n", sub_handle.c_str());
    res = false;
  }
  subscriptions_mut.unlock();
  return res;
}

void _AmSipSubscriptionContainer::removeSubscription(const string& sub_handle) {
  subscriptions_mut.lock();
  AmSipSubscriptionMapIter it = subscriptions.find(sub_handle);
  if (it != subscriptions.end()) {
    DBG("unsubscribing subscription '%s'\n", sub_handle.c_str());
    it->second->doUnsubscribe();
  } else {
    DBG("subscription '%s' already removed - ignoring\n", sub_handle.c_str());
  }
  subscriptions_mut.unlock();
}

// AmEventProcessingThread
void _AmSipSubscriptionContainer::onEvent(AmEvent* event) {
  //  auto_ptr<AmEvent> l_event(event);

  AmSipRequestEvent* sip_req_ev = dynamic_cast<AmSipRequestEvent*>(event);
  if (sip_req_ev) {
    // DBG("got SIP request: '%s'\n", sip_req_ev->req.print().c_str());
    DBG("got SIP request: %s %s\n",
	sip_req_ev->req.method.c_str(), sip_req_ev->req.r_uri.c_str());
    string ltag = sip_req_ev->req.to_tag;

    subscriptions_mut.lock();
    AmSipSubscriptionMapIter it = subscriptions.find(ltag);
    if (it == subscriptions.end()) {
      subscriptions_mut.unlock();
      WARN("got SIP request '%s' for unknown subscription '%s'\n",
	   sip_req_ev->req.print().c_str(), ltag.c_str());
      AmSipDialog::reply_error(sip_req_ev->req, 481, SIP_REPLY_NOT_EXIST);
      return;
    }
    it->second->onRxRequest(sip_req_ev->req);
    if (it->second->getState() == AmSipSubscription::SipSubStateTerminated) {
      DBG("subscription '%s' terminated - removing\n", it->second->getDescription().c_str());
      delete it->second;
      subscriptions.erase(it);
      AmEventDispatcher::instance()->delEventQueue(ltag);
    }
    subscriptions_mut.unlock();
    return;
  }

  AmSipReplyEvent* sip_reply_ev = dynamic_cast<AmSipReplyEvent*>(event);
  if (sip_reply_ev) {
    DBG("got SIP reply: '%s'\n", sip_reply_ev->reply.print().c_str());
    string ltag = sip_reply_ev->reply.from_tag;

    subscriptions_mut.lock();
    AmSipSubscriptionMapIter it = subscriptions.find(ltag);
    if (it == subscriptions.end()) {
      subscriptions_mut.unlock();
      WARN("got SIP reply '%s' for unknown subscription '%s'\n",
	   sip_reply_ev->reply.print().c_str(), ltag.c_str());

      return;
    }
    it->second->onRxReply(sip_reply_ev->reply);
    if (it->second->getState() == AmSipSubscription::SipSubStateTerminated) {
      DBG("subscription '%s' terminated - removing\n", it->second->getDescription().c_str());
      delete it->second;
      subscriptions.erase(it);
      AmEventDispatcher::instance()->delEventQueue(ltag);
    }
    subscriptions_mut.unlock();
    return;
  }

}


