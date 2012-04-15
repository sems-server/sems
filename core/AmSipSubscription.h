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

#ifndef _AmSipSubscription_h_
#define _AmSipSubscription_h_

#include <string>
using std::string;

#include <map>
#include <memory>

#include "ampi/UACAuthAPI.h"
#include "AmSessionEventHandler.h"
#include "AmMimeBody.h"

struct AmSipSubscriptionInfo {
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

struct SIPSubscriptionEvent : public AmEvent {

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
		      unsigned int code=0, const string& reason="")
   : AmEvent(E_SIP_SUBSCRIPTION), status(status), handle(handle),
    expires(expires), code(code), reason(reason), notify_body(0) {}

  const char* getStatusText() {
    switch (status) {
    case SubscribeActive: return "active";
    case SubscribeFailed: return "failed";
    case SubscribeTerminated: return "terminated";
    case SubscribePending: return "pending";
    case SubscriptionTimeout: return "timeout";
    }
    return "unknown";
  }

};

class AmSipSubscription 
: public AmSipDialogEventHandler,
  public DialogControl,
  public CredentialHolder
{

 public:
  enum SipSubscriptionState {
    SipSubStateIdle,
    SipSubStatePending,
    SipSubStateSubscribed,
    SipSubStateTerminated,
  };

 protected:

  AmSipDialog dlg;
  UACAuthCred cred;

  AmSipSubscriptionInfo info;

  // session to post events to 
  string sess_link;      

  AmSessionEventHandler* seh;

  AmSipRequest req;

  SipSubscriptionState sub_state;
  time_t sub_begin;
  unsigned int sub_expires;

  unsigned int wanted_expires;

  void setReqFromInfo();
  string getSubscribeHdrs();

  void onSubscriptionExpired();

 public:
  AmSipSubscription(const string& handle,
		    const AmSipSubscriptionInfo& info,
		    const string& sess_link);
  ~AmSipSubscription();

  void setSubscriptionInfo(const AmSipSubscriptionInfo& _info);
  string getDescription();

  void setSessionEventHandler(AmSessionEventHandler* new_seh);

  void setExpiresInterval(unsigned int desired_expires);

  /** subscribe with the info set */
  bool doSubscribe();
  /** re-subscribe with the info set */
  bool reSubscribe();
  /** unsubscribe */
  bool doUnsubscribe();

  /** return the state of the subscription */
  SipSubscriptionState getState(); 

  /** return the expires left for the subscription */
  unsigned int getExpiresLeft();

  const string& getEventSink() { return sess_link; }
  const string& getHandle() { return req.from_tag; }

  void onSendRequest(AmSipRequest& req, int flags);
  void onSendReply(AmSipReply& req, int flags);

  void onRxReply(const AmSipReply& reply);
  void onRxRequest(const AmSipRequest& req);

  // DialogControl if
  AmSipDialog* getDlg() { return &dlg; }
  // CredentialHolder	
  UACAuthCred* getCredentials() { return &cred; }

  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
  void onSipRequest(const AmSipRequest& req);
  void onInvite2xx(const AmSipReply&) {}
  void onRemoteDisappeared(const AmSipReply&) {}
  void onInvite1xxRel(const AmSipReply &){}
  void onNoAck(unsigned int) {}
  void onNoPrack(const AmSipRequest &, const AmSipReply &) {}
  void onPrack2xx(const AmSipReply &){}
  void onFailure(AmSipDialogEventHandler::FailureCause cause, 
      const AmSipRequest*, const AmSipReply*){}
  bool getSdpOffer(AmSdp&) {return false;}
  bool getSdpAnswer(const AmSdp&, AmSdp&) {return false;}
  int  onSdpCompleted(const AmSdp&, const AmSdp&) {return -1;}
  void onEarlySessionStart() {}
  void onSessionStart() {}
};

typedef std::map<string, AmSipSubscription*> AmSipSubscriptionMap;
typedef AmSipSubscriptionMap::iterator AmSipSubscriptionMapIter;
typedef AmSipSubscriptionMap::const_iterator AmSipSubscriptionMapConstIter;

#endif
