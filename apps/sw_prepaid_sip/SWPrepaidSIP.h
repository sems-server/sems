/*
 * $Id$
 *
 * Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of mycc, Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _SW_PREPAID_SIP_H
#define _SW_PREPAID_SIP_H

#include "AmB2BSession.h"

#include <time.h>
#include <string>
using std::string;

class SWPrepaidSIPFactory: public AmSessionFactory
{
  AmDynInvokeFactory* user_timer_fact;
  AmDynInvokeFactory* cc_acc_fact;

  public:
    SWPrepaidSIPFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req);
};

class SWPrepaidSIPDialog : public AmB2BCallerSession
{
  enum {
    CC_Init = 0,
    CC_Dialing,
    CC_Connected,
    CC_Teardown
  } CallerState;

  int m_state;
  AmSipRequest m_localreq;

  string m_uuid;
  string m_ruri;
  string m_proxy;
  string m_dest;
  time_t m_starttime;
  int m_credit;

  void startAccounting();
  void stopAccounting();
  struct timeval m_acc_start;

  AmDynInvoke* m_user_timer;
  AmDynInvoke* m_cc_acc;

  public:

    SWPrepaidSIPDialog(AmDynInvoke* cc_acc, AmDynInvoke* user_timer);
    ~SWPrepaidSIPDialog();

    void process(AmEvent* ev);
    void onBye(const AmSipRequest& req);
    void onInvite(const AmSipRequest& req);
    void onCancel();

  protected:

    bool onOtherReply(const AmSipReply& reply);
    void onOtherBye(const AmSipRequest& req);
};
#endif                           // _SW_PREPAID_SIP_H
