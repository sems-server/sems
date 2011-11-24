/*
 * Copyright (C) 2007 iptego GmbH
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
#ifndef _CALLBACK_H_
#define _CALLBACK_H_

#include "AmApi.h"
#include "AmB2ABSession.h"
#include "AmAudio.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"
#include "AmUACAuth.h"

#include <map>
#include <string>
using std::string;

#include <sys/types.h>
#include <regex.h>

// configuration parameter names
#define WELCOME_PROMPT "welcome_prompt"
#define DIGITS_DIR     "digits_dir"
#define ACCEPT_CALLER_RE "accept_caller_re"

class CallBackFactory 
  : public AmSessionFactory,
    public AmThread
{
  AmPromptCollection prompts;

  bool configured;

  regex_t accept_caller_re;

  std::multimap<time_t, string> scheduled_calls;
  AmMutex scheduled_calls_mut;
  // seconds to wait before calling back
  int cb_wait;

  void createCall(const string& number);


public:
  static string gw_user;
  static string gw_domain;
  static string auth_user;
  static string auth_pwd;
  
  static string DigitsDir;
  static PlayoutType m_PlayoutType;

  CallBackFactory(const string& _app_name);
  AmSession* onInvite(const AmSipRequest&, const string& app_name,
		      const map<string,string>& app_params);
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      AmArg& session_params);
  int onLoad();

  void run();
  void on_stop();
};

enum CBState {
  CBNone = 0,
  CBEnteringNumber,
  CBTellingNumber,
  CBConnecting,
  CBConnected
};

class CallBackDialog 
  : public AmB2ABCallerSession,
    public CredentialHolder
{

private:
  AmPlaylist  play_list;

  AmPromptCollection& prompts;

  string  call_number;
  UACAuthCred* cred;

  CBState state;
public:
  CallBackDialog(AmPromptCollection& prompts,		 
		 UACAuthCred* cred);
  ~CallBackDialog();

  void process(AmEvent* ev);
  void onInvite(const AmSipRequest& req); 
  void onSessionStart();
  void onDtmf(int event, int duration);

  UACAuthCred* getCredentials() { return cred; }
  AmB2ABCalleeSession* createCalleeSession();

};

class CallBackCalleeDialog 
  : public AmB2ABCalleeSession,
    public CredentialHolder
{
  UACAuthCred* cred;
public:
  CallBackCalleeDialog(const string& other_tag,
		       AmSessionAudioConnector* connector,
		       UACAuthCred* cred);
  ~CallBackCalleeDialog();
  UACAuthCred* getCredentials() { return cred; }
};

#endif
// Local Variables:
// mode:C++
// End:

