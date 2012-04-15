/*
 * Copyright (C) 2012 FRAFOS GmbH
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
#include "AmSession.h"
#include "AmSessionContainer.h"
AmSipSubscription::AmSipSubscription(const string& handle,
				     const AmSipSubscriptionInfo& info,
				     const string& sess_link) 
  : info(info),
    dlg(this),
    cred(info.domain, info.from_user, info.pwd),
    sub_begin(0),
    sub_expires(0),
    wanted_expires(0),
    sess_link(sess_link),
    seh(NULL),
    sub_state(SipSubStateIdle)
{

  setReqFromInfo();
  req.from_tag = handle;

  // clear dlg.callid? ->reregister?
  dlg.setOAEnabled(false);
  dlg.initFromLocalRequest(req);
  dlg.cseq = 50;
}

AmSipSubscription::~AmSipSubscription() {
  setSessionEventHandler(NULL);
}

void AmSipSubscription::setReqFromInfo() {
  req.user     = info.user;
  req.method   = "SUBSCRIBE";
  req.domain   = info.domain;
  req.r_uri    = "sip:"+info.user+"@"+info.domain;
  req.from     = "<sip:"+info.from_user+"@"+info.domain+">";
  req.from_uri = "sip:"+info.from_user+"@"+info.domain;
  req.to       = "<sip:"+info.user+"@"+info.domain+">";
  req.to_tag   = "";
  req.callid   = AmSession::getNewId(); 
}

void AmSipSubscription::setSubscriptionInfo(const AmSipSubscriptionInfo& _info) {
  DBG("updating subscription info for '%s@%s'\n",
      _info.user.c_str(), _info.domain.c_str());
  info = _info;

  setReqFromInfo();

  // to trigger setting dlg identifiers
  dlg.callid.clear();
  dlg.contact_uri.clear();

  dlg.initFromLocalRequest(req);
}

void AmSipSubscription::setExpiresInterval(unsigned int desired_expires) {
  wanted_expires = desired_expires;
}

void AmSipSubscription::setSessionEventHandler(AmSessionEventHandler* new_seh) {
  if (seh)
    delete seh;
  seh = new_seh;
}

string AmSipSubscription::getDescription() {
  return "'"+info.user+"@"+info.domain+", Event: "+info.event+"'";
}

string AmSipSubscription::getSubscribeHdrs() {
  string hdrs;
  hdrs += SIP_HDR_COLSP(SIP_HDR_EVENT) + info.event;
  if (!info.id.empty())
    hdrs += ";id="+info.id;
  hdrs += CRLF;
  if (!info.accept.empty()) {
    hdrs += SIP_HDR_COLSP(SIP_HDR_ACCEPT) + info.accept + CRLF;
  }
  if (wanted_expires) {
    hdrs += SIP_HDR_COLSP(SIP_HDR_EXPIRES) + int2str(wanted_expires)  + CRLF;
  }

  return hdrs;
}

bool AmSipSubscription::doSubscribe() 
{
  bool res = true;

  req.to_tag     = "";
  dlg.remote_tag = "";
  dlg.remote_uri = req.r_uri;
    
  // set outbound proxy as next hop 
  if (!info.proxy.empty()) {
    dlg.outbound_proxy = info.proxy;
  } else if (!AmConfig::OutboundProxy.empty()) {
    dlg.outbound_proxy = AmConfig::OutboundProxy;
  }

  if (dlg.sendRequest(req.method, NULL, getSubscribeHdrs()) < 0) {
    WARN("failed to send subscription to '%s' (proxy '%s').\n",
	 dlg.remote_uri.c_str(), dlg.outbound_proxy.c_str());
    return false;
  }

  switch (sub_state) {
  case SipSubStateIdle:
  case SipSubStatePending:
  case SipSubStateTerminated:
    sub_state = SipSubStatePending;
    break;
  case SipSubStateSubscribed: break;
  }

  return res;
}

bool AmSipSubscription::reSubscribe() 
{
  if (dlg.sendRequest(req.method, NULL, getSubscribeHdrs()) < 0) {
    WARN("failed to send subscription to '%s' (proxy '%s').\n",
	 dlg.remote_uri.c_str(), dlg.outbound_proxy.c_str());
    return false;
  }

  return true;
}

bool AmSipSubscription::doUnsubscribe()
{
  if (sub_state == SipSubStateTerminated || sub_state == SipSubStateIdle) {
    DBG("not subscribed - not unsubscribing\n");
    return true;
  }

  req.to_tag     = "";
  dlg.remote_tag = "";
  dlg.remote_uri = req.r_uri;
    
  // set outbound proxy as next hop 
  if (!info.proxy.empty()) {
    dlg.outbound_proxy = info.proxy;
  } else if (!AmConfig::OutboundProxy.empty()) 
    dlg.outbound_proxy = AmConfig::OutboundProxy;
  //else 
  //    dlg.outbound_proxy = "";

  string hdrs;
  hdrs += SIP_HDR_COLSP(SIP_HDR_EVENT) + info.event;
  if (!info.id.empty())
    hdrs += ";id="+info.id;
  hdrs += CRLF;
  if (!info.accept.empty()) {
    hdrs += SIP_HDR_COLSP(SIP_HDR_ACCEPT) + info.accept + CRLF;
  }
  hdrs += SIP_HDR_COLSP(SIP_HDR_EXPIRES) "0" CRLF;

  if (dlg.sendRequest(req.method, NULL, hdrs) < 0) {
    WARN("failed to send unsubscription to '%s' (proxy '%s').\n",
	 dlg.remote_uri.c_str(), dlg.outbound_proxy.c_str());
    return false;
  }

  return true;
}

void AmSipSubscription::onSendRequest(AmSipRequest& req, int flags) 
{
  if (seh)
    seh->onSendRequest(req,flags);
}
	
void AmSipSubscription::onSendReply(AmSipReply& reply, int flags) {
  if (seh)
    seh->onSendReply(reply,flags);
}

AmSipSubscription::SipSubscriptionState AmSipSubscription::getState() {
  return sub_state;
}

unsigned int AmSipSubscription::getExpiresLeft() {
  if (sub_state != SipSubStateSubscribed)
    return 0;

  long diff = sub_begin + sub_expires  - time(NULL);
  if (diff < 0) 
    return 0;

  return diff;
}

void AmSipSubscription::onSubscriptionExpired() {
  if (sess_link.length()) {
    AmSessionContainer::instance()->
      postEvent(sess_link, new SIPSubscriptionEvent(SIPSubscriptionEvent::
						    SubscriptionTimeout, req.from_tag));
  }
  DBG("Subscription '%s' expired.\n", getDescription().c_str());
}

void AmSipSubscription::onRxReply(const AmSipReply& reply) {
  dlg.onRxReply(reply);
}

void AmSipSubscription::onRxRequest(const AmSipRequest& req) {
  dlg.onRxRequest(req);
}

void AmSipSubscription::onSipReply(const AmSipReply& reply, 
				   AmSipDialog::Status old_dlg_status)
{
  if ((seh!=NULL) && seh->onSipReply(reply, old_dlg_status))
    return;

  AmSipTransaction* sip_trans = dlg.getUACTrans(reply.cseq);
  if (!sip_trans) {
    WARN("No UAC transaction found for reply '%s', ignoring (%s)\n",
	 reply.print().c_str(), getDescription().c_str());
    return;
  }

  if (reply.code < 200) {
    DBG("Provisional reply received for subscription '%s'\n", getDescription().c_str());
    return;
  }

  if ((reply.code>=200)&&(reply.code<300)) {
      DBG("Positive reply to SUBSCRIBE\n");

      unsigned int expires_i = 0;
      string expires = getHeader(reply.hdrs, SIP_HDR_EXPIRES);
      if (expires.length()) {
	str2i(trim(expires, " \t"), expires_i);
      }

      AmSessionContainer::instance()->
	postEvent(sess_link,
		  new SIPSubscriptionEvent(SIPSubscriptionEvent::SubscribeActive,
					   req.from_tag, expires_i,
					   reply.code, reply.reason));
  } else if (reply.code >= 300) {
    DBG("Subscription '%s' failed\n", getDescription().c_str());
    if (sess_link.length()) {
      AmSessionContainer::instance()->
	postEvent(sess_link,
		  new SIPSubscriptionEvent(SIPSubscriptionEvent::SubscribeFailed,
					   req.from_tag, 0,
					   reply.code, reply.reason));
    }
    sub_state = SipSubStateTerminated;
  }
}

void AmSipSubscription::onSipRequest(const AmSipRequest& req) {
  DBG("SIP Request received: '%s'\n", req.print().c_str());
  if (req.method != SIP_METH_NOTIFY) {
    WARN("received %s Request in subscription???\n", req.method.c_str());
    dlg.reply(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR); // todo: other err code?
    sub_state = SipSubStateTerminated;
    return;
  }

  string h_subscription_state = getHeader(req.hdrs, SIP_HDR_SUBSCRIPTION_STATE);
  if (h_subscription_state.empty()) {
    WARN("received NOTIFY without Subscription-State (hdrs='%s')\n", req.hdrs.c_str());
    dlg.reply(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR); // todo: other err code?
    sub_state = SipSubStateTerminated;
    return;
  }
  size_t pos = h_subscription_state.find(";");
  string subscription_state = h_subscription_state;
  string subscription_state_params;
  unsigned int expires = 0;
  string reason;
  
  if (pos != string::npos) {
    subscription_state = h_subscription_state.substr(0, pos);
    subscription_state_params = h_subscription_state.substr(pos+1);
  }
  vector<string> params = explode(subscription_state_params, ";");
  for (vector<string>::iterator it=params.begin(); it != params.end(); it++) {
    vector<string> pv = explode(trim(*it, " \t"), "=");
    if (pv.size()) {
      if (trim(pv[0], " \t")=="expires") {
	if (pv.size()>1)
	  str2i(trim(pv[1], " \t"), expires);
      } else if (trim(pv[0], " \t")=="reason") {
	if (pv.size()>1)
	  reason = trim(pv[1], " \t");
      }
    }
  }

  DBG("subscription state: '%s', expires: %u, reason: '%s'\n",
      subscription_state.c_str(), expires, reason.c_str());

  SIPSubscriptionEvent* sub_ev = 
    new SIPSubscriptionEvent(SIPSubscriptionEvent::SubscribeFailed, req.from_tag);

  if (subscription_state == "active") {
    sub_begin = time(0);
    sub_ev->expires = sub_expires = expires;
    sub_state = SipSubStateSubscribed;
    sub_ev->status = SIPSubscriptionEvent::SubscribeActive;
  } else if (subscription_state == "pending") {
    sub_begin = time(0);
    sub_expires = expires;
    sub_state = SipSubStatePending;
    sub_ev->status = SIPSubscriptionEvent::SubscribePending;
  } else if (subscription_state == "terminated") {
    sub_expires = 0;
    sub_state = SipSubStateTerminated;
    sub_ev->status = SIPSubscriptionEvent::SubscribeTerminated;
  } else {
    ERROR("unknown subscription_state '%s'\n", subscription_state.c_str());
  }

  if (!req.body.empty())
    sub_ev->notify_body.reset(new AmMimeBody(req.body));

  DBG("posting event to session '%s'\n", sess_link.c_str());
  AmSessionContainer::instance()->postEvent(sess_link, sub_ev);

  dlg.reply(req, 200, "OK");
}
