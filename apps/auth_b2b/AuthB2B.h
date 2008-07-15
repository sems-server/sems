/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 * Based on the concept of sw_prepaid_sip, Copyright (C) 2007 Sipwise GmbH
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

#ifndef _AUTH_B2B_H
#define _AUTH_B2B_H

#include "AmB2BSession.h"
#include "ampi/UACAuthAPI.h"

using std::string;

class AuthB2BFactory: public AmSessionFactory
{
/*   AmDynInvokeFactory* user_timer_fact; */

 public:
  AuthB2BFactory(const string& _app_name);
  
  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
};

class AuthB2BDialog : public AmB2BCallerSession
{
  enum {
    BB_Init = 0,
    BB_Dialing,
    BB_Connected,
    BB_Teardown
  } CallerState;

  int m_state;

  string domain;
  string user;
  string password;
  string from;
  string to;
  
/*   AmDynInvoke* m_user_timer; */

 public:

  AuthB2BDialog(); //AmDynInvoke* user_timer);
  ~AuthB2BDialog();
  
  void process(AmEvent* ev);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel();
  

 protected:
  
  bool onOtherReply(const AmSipReply& reply);
  void onOtherBye(const AmSipRequest& req);

  void createCalleeSession();
};

class AuthB2BCalleeSession 
: public AmB2BCalleeSession, public CredentialHolder
{
  UACAuthCred credentials;
  AmSessionEventHandler* auth;

 protected:
  void onSipReply(const AmSipReply& reply);
  void onSendRequest(const string& method, const string& content_type,
		     const string& body, string& hdrs, int flags, unsigned int cseq);

 public:
  AuthB2BCalleeSession(const AmB2BCallerSession* caller, const string& user, const string& pwd); 
  ~AuthB2BCalleeSession();

  inline UACAuthCred* getCredentials();
  
  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }
};
#endif                           
