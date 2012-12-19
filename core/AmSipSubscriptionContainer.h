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
#ifndef _AmSipSubscriptionContainer_h_
#define _AmSipSubscriptionContainer_h_

#include "AmSipSubscription.h"
#include "AmEventProcessingThread.h"

typedef map<string,AmSipSubscriptionDialog*> AmSipSubscriptionMap;
typedef AmSipSubscriptionMap::iterator AmSipSubscriptionMapIter;


class _AmSipSubscriptionContainer 
: public AmEventProcessingThread
{
  AmSipSubscriptionMap subscriptions;
  AmMutex subscriptions_mut;

  void initialize();
  bool initialized;
 public:
  _AmSipSubscriptionContainer();
  ~_AmSipSubscriptionContainer();

  string createSubscription(const AmSipSubscriptionInfo& info, 
			    const string& sess_link,
			    unsigned int wanted_expires=0);

  bool refreshSubscription(const string& sub_handle, 
			   unsigned int wanted_expires=0);

  void removeSubscription(const string& sub_handle);

  // AmEventProcessingThread
  void onEvent(AmEvent* event);
};

typedef singleton<_AmSipSubscriptionContainer> AmSipSubscriptionContainer;

#endif
