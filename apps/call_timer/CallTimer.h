/*
 * Copyright (C) 2008 iptego GmbH
 * Based on the concept of auth_b2b, Copyright (C) 2008 iptego GmbH
 * Based on the concept of sw_prepaid_sip, Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of mycc, Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _AUTH_B2B_H
#define _AUTH_B2B_H

#include "AmB2BSession.h"

using std::string;

class CallTimerFactory: public AmSessionFactory
{
 public:
  static unsigned int DefaultCallTimer; 
  static bool UseAppParam;

  CallTimerFactory(const string& _app_name);
  ~CallTimerFactory();

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
};

class CallTimerDialog : public AmB2BCallerSession
{
  enum {
    BB_Init = 0,
    BB_Dialing,
    BB_Connected,
    BB_Teardown
  } CallerState;

  int m_state;
  
  unsigned int call_time;

 public:

  CallTimerDialog(unsigned int call_time);
  ~CallTimerDialog();
  
  void process(AmEvent* ev);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel();
  

 protected:
  
  bool onOtherReply(const AmSipReply& reply);
  void onOtherBye(const AmSipRequest& req);
};
#endif                           
