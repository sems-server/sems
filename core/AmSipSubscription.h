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
#ifndef _AmSipSubscription_h_
#define _AmSipSubscription_h_

#include "AmSipMsg.h"
#include "AmBasicSipDialog.h"
#include "AmEvent.h"
#include "AmEventQueue.h"

#include <map>
#include <list>
using std::map;
using std::list;

class SingleSubscription;
class AmBasicSipDialog;
class AmEventQueue;

/**
 * \brief SIP Subscription collection
 *
 * This class contains all the suscriptions sharing
 * the same dialog.
 */
class AmSipSubscription
{
  typedef list<SingleSubscription*> Subscriptions;
  typedef map<unsigned int,Subscriptions::iterator> CSeqMap;

  AmBasicSipDialog* dlg;
  AmEventQueue*    ev_q;
  Subscriptions    subs;
  CSeqMap  uas_cseq_map;
  CSeqMap  uac_cseq_map;

  Subscriptions::iterator createSubscription(const AmSipRequest& req, bool uac);
  Subscriptions::iterator matchSubscription(const AmSipRequest& req, bool uac);

  friend class SingleSubscription;

public:
  AmSipSubscription(AmBasicSipDialog* dlg, AmEventQueue* ev_q);
  ~AmSipSubscription();

  void terminate();
  
  bool onRequestIn(const AmSipRequest& req);
  void onRequestSent(const AmSipRequest& req);
  bool onReplyIn(const AmSipRequest& req, const AmSipReply& reply);
  void onReplySent(const AmSipRequest& req, const AmSipReply& reply);

  virtual void onNotify(const AmSipRequest& req, SingleSubscription* sub) {}
  virtual void onFailureReply(const AmSipReply& reply, SingleSubscription* sub) {}
  virtual void onTimeout(int timer_id, SingleSubscription* sub);
};

struct SIPSubscriptionEvent
  : public AmEvent
{

  enum SubscriptionStatus {
    SubscribeActive=0,
    SubscribeFailed,
    SubscribeTerminated,
    SubscribePending,
    SubscriptionTimeout
  };

  string handle;
  unsigned int code;
  string reason;
  SubscriptionStatus status;
  unsigned int expires;
  std::auto_ptr<AmMimeBody> notify_body;

  SIPSubscriptionEvent(SubscriptionStatus status, const string& handle,
		       unsigned int expires = 0,
		       unsigned int code=0, const string& reason="");
  
  const char* getStatusText();
};

struct AmSipSubscriptionInfo
{
  string domain;
  string user;
  string from_user;
  string pwd;
  string proxy;
  string event;
  string accept;
  string id;

  AmSipSubscriptionInfo(const string& domain,
			const string& user,
			const string& from_user,
			const string& pwd,
			const string& proxy,
			const string& event)
  : domain(domain),user(user),
    from_user(from_user),pwd(pwd),proxy(proxy),
    event(event)
  { }
};

class AmSipSubscriptionDialog
  : public AmBasicSipDialog,
    public AmBasicSipEventHandler,
    public AmSipSubscription
{
  string event;
  string event_id;
  string accept;
  string sess_link;

public:
  AmSipSubscriptionDialog(const AmSipSubscriptionInfo& info,
			  const string& sess_link,
			  AmEventQueue* ev_q = NULL);

  int subscribe(int expires);

  string getDescription();

  /** AmSipSubscription interface */
  void onNotify(const AmSipRequest& req, SingleSubscription* sub);
  void onFailureReply(const AmSipReply& reply, SingleSubscription* sub);
  void onTimeout(int timer_id, SingleSubscription* sub);

  /** AmBasicDialogEventHandler interface */
  void onSipRequest(const AmSipRequest& req)
  { AmSipSubscription::onRequestIn(req); }

  void onRequestSent(const AmSipRequest& req)
  { AmSipSubscription::onRequestSent(req); }

  void onSipReply(const AmSipRequest& req, const AmSipReply& reply,
		  AmBasicSipDialog::Status old_status)
  { AmSipSubscription::onReplyIn(req,reply); }

  void onReplySent(const AmSipRequest& req, const AmSipReply& reply)
  { AmSipSubscription::onReplySent(req,reply); }
};

#endif
