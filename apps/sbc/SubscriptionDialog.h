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
#ifndef _SubscriptionDialog_h_
#define _SubscriptionDialog_h_

#include "AmSipSubscription.h"
#include "SBCSimpleRelay.h"

/**
 * \brief B2B Subscription UA
 *
 * This class represents one side of a B2BUA
 * supporting SIP standalone subscriptions.
 */
class SubscriptionDialog
  : public SimpleRelayDialog
{
  AmSipSubscription* subs;

public:
  SubscriptionDialog(SBCCallProfile &profile, vector<AmDynInvoke*> &cc_modules,
		     AmSipSubscription* subscription=NULL,
		     atomic_ref_cnt* parent_obj=NULL);
  ~SubscriptionDialog();

  // SimpleRelayDialog interface
  bool terminated();

  // AmBasicSipEventHandler interface
  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);

  void onRequestSent(const AmSipRequest& req);
  void onReplySent(const AmSipRequest& req, const AmSipReply& reply);

  void onRemoteDisappeared(const AmSipReply& reply);
  void onLocalTerminate(const AmSipReply& reply);
};

#endif
