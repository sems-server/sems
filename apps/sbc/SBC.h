/*
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

#ifndef _SBC_H
#define _SBC_H

#include "AmB2BSession.h"

#include "AmConfigReader.h"
#include "AmUriParser.h"
#include "HeaderFilter.h"
#include "SBCCallProfile.h"

#include <map>

using std::string;

#define SBC_TIMER_ID_CALL_TIMER         1
#define SBC_TIMER_ID_PREPAID_TIMEOUT    2


class SBCFactory: public AmSessionFactory,
    public AmDynInvoke,
    public AmDynInvokeFactory
{

  std::map<string, SBCCallProfile> call_profiles;
  
  string active_profile;
  AmMutex profiles_mut;

  void listProfiles(const AmArg& args, AmArg& ret);
  void reloadProfiles(const AmArg& args, AmArg& ret);
  void reloadProfile(const AmArg& args, AmArg& ret);
  void loadProfile(const AmArg& args, AmArg& ret);
  void getActiveProfile(const AmArg& args, AmArg& ret);
  void setActiveProfile(const AmArg& args, AmArg& ret);

 public:
  DECLARE_MODULE_INSTANCE(SBCFactory);

  SBCFactory(const string& _app_name);
  ~SBCFactory();

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
  static string user;
  static string domain;
  static string pwd;

  static AmConfigReader cfg;
  static AmSessionEventHandlerFactory* session_timer_fact;

  // DI
  // DI factory
  AmDynInvoke* getInstance() { return instance(); }
  // DI API
  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);

};

class SBCDialog : public AmB2BCallerSession
{
  enum {
    BB_Init = 0,
    BB_Dialing,
    BB_Connected,
    BB_Teardown
  } CallerState;

  int m_state;

  string ruri;
  string from;
  string to;
  string callid;

  unsigned int call_timer;

  // prepaid
  AmDynInvoke* prepaid_acc;
  time_t prepaid_starttime;
  struct timeval prepaid_acc_start;
  int prepaid_credit;

  SBCCallProfile call_profile;

  void stopCall();
  bool startCallTimer();
  void startPrepaidAccounting();
  void stopPrepaidAccounting();

  bool getPrepaidInterface();

 public:

  SBCDialog(const SBCCallProfile& call_profile);
  ~SBCDialog();
  
  void process(AmEvent* ev);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel();

 protected:
  int relayEvent(AmEvent* ev);

  void onSipReply(const AmSipReply& reply, int old_dlg_status,
		  const string& trans_method);
  void onSipRequest(const AmSipRequest& req);  

  bool onOtherReply(const AmSipReply& reply);
  void onOtherBye(const AmSipRequest& req);

  int filterBody(AmSdp& sdp, bool is_a2b);

  void createCalleeSession();
};

class SBCCalleeSession 
: public AmB2BCalleeSession, public CredentialHolder
{
  AmSessionEventHandler* auth;
  SBCCallProfile call_profile;

 protected:
  int relayEvent(AmEvent* ev);

  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipReply& reply, int old_dlg_status,
		  const string& trans_method);
  void onSendRequest(const string& method, const string& content_type,
		     const string& body, string& hdrs, int flags, unsigned int cseq);

  /* bool onOtherReply(const AmSipReply& reply); */

  int filterBody(AmSdp& sdp, bool is_a2b);

 public:
  SBCCalleeSession(const AmB2BCallerSession* caller,
		   const SBCCallProfile& call_profile); 
  ~SBCCalleeSession();

  inline UACAuthCred* getCredentials();
  
  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }
};
#endif                           
