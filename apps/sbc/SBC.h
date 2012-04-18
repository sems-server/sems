/*
 * Copyright (C) 2010-2011 Stefan Sayer
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
#include "RegexMapper.h"

#include <map>

using std::string;

#define SBC_TIMER_ID_CALL_TIMERS_START   10
#define SBC_TIMER_ID_CALL_TIMERS_END     99

class SBCFactory: public AmSessionFactory,
    public AmDynInvoke,
    public AmDynInvokeFactory
{

  std::map<string, SBCCallProfile> call_profiles;
  
  vector<string> active_profile;
  AmMutex profiles_mut;

  void listProfiles(const AmArg& args, AmArg& ret);
  void reloadProfiles(const AmArg& args, AmArg& ret);
  void reloadProfile(const AmArg& args, AmArg& ret);
  void loadProfile(const AmArg& args, AmArg& ret);
  void getActiveProfile(const AmArg& args, AmArg& ret);
  void setActiveProfile(const AmArg& args, AmArg& ret);
  void getRegexMapNames(const AmArg& args, AmArg& ret);
  void setRegexMap(const AmArg& args, AmArg& ret);
  void loadCallcontrolModules(const AmArg& args, AmArg& ret);
  void postControlCmd(const AmArg& args, AmArg& ret);

  string getActiveProfileMatch(string& profile_rule, const AmSipRequest& req,
			       const string& app_param, AmUriParser& ruri_parser,
			       AmUriParser& from_parser, AmUriParser& to_parser);

 public:
  DECLARE_MODULE_INSTANCE(SBCFactory);

  SBCFactory(const string& _app_name);
  ~SBCFactory();

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);

  static AmConfigReader cfg;
  static AmSessionEventHandlerFactory* session_timer_fact;

  static RegexMapper regex_mappings;

  // DI
  // DI factory
  AmDynInvoke* getInstance() { return instance(); }
  // DI API
  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);

};

class SBCDialog : public AmB2BCallerSession, public CredentialHolder
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

  map<int, double> call_timers;

  int outbound_interface;

  int rtprelay_interface;

  // call control
  vector<AmDynInvoke*> cc_modules;
  // current timer ID - cc module setting timer will use this
  int cc_timer_id;

  struct timeval call_start_ts;
  struct timeval call_connect_ts;
  struct timeval call_end_ts;

  // auth
  AmSessionEventHandler* auth;

  SBCCallProfile call_profile;

  void fixupCCInterface(const string& val, CCInterface& cc_if);

  /** handler called when the second leg is connected */
  void onCallConnected(const AmSipReply& reply);

  /** handler called when call is stopped */
  void onCallStopped();

  /** handler called when SST timeout occured */
  void onSessionTimeout();

  /** handler called when no ACK received */
  void onNoAck(unsigned int cseq);

  /** handler called when we receive 408/481 */
  void onRemoteDisappeared(const AmSipReply& reply);

  /** stop call (both legs, CC) */
  void stopCall();

  /* set call timer (if enabled) */
  bool startCallTimers();
  /* clear call timer */
  void stopCallTimers();

  /** initialize call control module interfaces @return sucess or not*/
  bool getCCInterfaces();
  /** call is started */
  bool CCStart(const AmSipRequest& req);
  /** connection of second leg */
  void CCConnect(const AmSipReply& reply);
  /** end call */
  void CCEnd();
  void CCEnd(const CCInterfaceListIteratorT& end_interface);

 public:

  SBCDialog(const SBCCallProfile& call_profile);
  ~SBCDialog();
  
  void process(AmEvent* ev);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel(const AmSipRequest& cancel);

  void onSystemEvent(AmSystemEvent* ev);

  UACAuthCred* getCredentials();

  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }

  /** save call timer; only effective before call is connected */
  void saveCallTimer(int timer, double timeout);
  /** clear saved call timer, only effective before call is connected */
  void clearCallTimer(int timer);
  /** clear all saved call timer, only effective before call is connected */
  void clearCallTimers();

 protected:
  int relayEvent(AmEvent* ev);

  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
  void onSendRequest(AmSipRequest& req, int flags);

  bool onOtherReply(const AmSipReply& reply);
  void onOtherBye(const AmSipRequest& req);

  virtual void filterBody(AmSipRequest &req, AmSdp &sdp);
  virtual void filterBody(AmSipReply &reply, AmSdp &sdp);

  void onControlCmd(string& cmd, AmArg& params);

  void createCalleeSession();
};

class SBCCalleeSession 
: public AmB2BCalleeSession, public CredentialHolder
{
  AmSessionEventHandler* auth;
  SBCCallProfile call_profile;

  void appendTranscoderCodecs(AmSdp &sdp);
  
 protected:
  int relayEvent(AmEvent* ev);

  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
  void onSendRequest(AmSipRequest& req, int flags);

  /* bool onOtherReply(const AmSipReply& reply); */

  virtual void filterBody(AmSipRequest &req, AmSdp &sdp);
  virtual void filterBody(AmSipReply &reply, AmSdp &sdp);

  void onControlCmd(string& cmd, AmArg& params);

 public:
  SBCCalleeSession(const AmB2BCallerSession* caller,
		   const SBCCallProfile& call_profile); 
  ~SBCCalleeSession();

  void process(AmEvent* ev);

  inline UACAuthCred* getCredentials();
  
  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }
};

extern void assertEndCRLF(string& s);

#endif                           
