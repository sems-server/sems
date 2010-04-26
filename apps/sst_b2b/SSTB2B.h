/*
 * $Id: AuthB2B.h 1252 2009-02-01 12:51:06Z sayer $
 *
 * Copyright (C) 2010 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _SST_B2B_H
#define _SST_B2B_H

#include "AmB2BSession.h"
#include "ampi/UACAuthAPI.h"

#include "AmConfigReader.h"

using std::string;

class SSTB2BFactory: public AmSessionFactory
{
/*   AmDynInvokeFactory* user_timer_fact; */


 public:
  SSTB2BFactory(const string& _app_name);
  
  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
  static string user;
  static string domain;
  static string pwd;

  static AmConfigReader cfg;
  static AmSessionEventHandlerFactory* session_timer_fact;
};

class SSTB2BDialog : public AmB2BCallerSession
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

  string last_otherleg_content_type;
  string last_otherleg_body;

  string last_content_type;
  string last_body;

/*   AmDynInvoke* m_user_timer; */
  /* set<int> b2b_reinvite_cseqs; // cseqs of reinvite we sent from the middle */

 public:

  SSTB2BDialog(); //AmDynInvoke* user_timer);
  ~SSTB2BDialog();
  
  void process(AmEvent* ev);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel();

  void sendReinvite(bool updateSDP, const string& headers);

 protected:
    void onSipReply(const AmSipReply& reply, int old_dlg_status);
  void onSipRequest(const AmSipRequest& req);  

 protected:
  
  bool onOtherReply(const AmSipReply& reply);
  void onOtherBye(const AmSipRequest& req);

  void createCalleeSession();
};

class SSTB2BCalleeSession 
: public AmB2BCalleeSession, public CredentialHolder
{
  UACAuthCred credentials;
  AmSessionEventHandler* auth;

  /* string last_otherleg_content_type; */
  /* string last_otherleg_body; */

 protected:
  void onSipRequest(const AmSipRequest& req);
    void onSipReply(const AmSipReply& reply, int old_dlg_status);
  void onSendRequest(const string& method, const string& content_type,
		     const string& body, string& hdrs, int flags, unsigned int cseq);

  /* bool onOtherReply(const AmSipReply& reply); */

 public:
  SSTB2BCalleeSession(const AmB2BCallerSession* caller, const string& user, const string& pwd); 
  ~SSTB2BCalleeSession();

  inline UACAuthCred* getCredentials();
  
  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }

  void sendReinvite(bool updateSDP, const string& headers);
};
#endif                           
