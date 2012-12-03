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
};


#endif
