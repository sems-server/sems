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
#ifndef _PINAUTHCONFERENCE_H_
#define _PINAUTHCONFERENCE_H_

#include "AmApi.h"
#include "AmSession.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"
#include "ampi/UACAuthAPI.h"

#include <map>
#include <string>
using std::map;
using std::multimap;
using std::string;

#include <time.h>

enum CallGenEvent {
  CGCreate = 0,
  CGConnect,
  CGDisconnect,
  CGDestroy
};

struct CallInfo {
  CallGenEvent status;
  time_t connect_ts;
  time_t disconnect_ts;
};

class CallGenFactory 
  : public AmSessionFactory,
    public AmDynInvokeFactory,
    public AmDynInvoke,
    public AmThread
{
  AmPromptCollection prompts;

  // for DI 
  static CallGenFactory* _instance;

  int load();
  bool configured;

  multimap<time_t, AmArg> actions;
  AmMutex actions_mut;

  map<string, CallInfo> active_calls;
  map<string, CallInfo> past_calls;
  AmMutex calls_list_mut;

  void createCall(const AmArg& args);
  void checkTarget();

  bool target_enabled;
  AmArg* target_args;
  // number of already scheduled calls for the target
  int scheduled; 

public:
  static string DigitsDir;
  static AmFileCache play_file;
  static string from_host;
  CallGenFactory(const string& _app_name);
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      AmArg& args);

  int onLoad();

  // DI API
  CallGenFactory* getInstance(){
    return _instance;
  }

  static CallGenFactory* instance(){
    return _instance;
  }

  void invoke(const string& method, const AmArg& args, AmArg& ret);

  // DI functions
  void createCalls(const AmArg& args, AmArg& ret);
  void scheduleCalls(const AmArg& args, AmArg& ret);
  void setTarget(const AmArg& args, AmArg& ret);
  void callGenStats(const AmArg& args, AmArg& ret);

  // thread
  void run();
  void on_stop();

  // report
  void reportCall(string callid, 
		  CallGenEvent ev, 
		  time_t connect_ts,
		  time_t disconnect_ts);
};

class CallGenDialog 
  : public AmSession
{
public:

private:
  AmPlaylist  play_list;
  AmCachedAudioFile play_file;
  AmPromptCollection& prompts;

  time_t connect_ts;
  time_t disconnect_ts;

  int play_rand_digits;
  int call_time_base; 
  int call_time_rand;

  bool timer_started;

  void report(CallGenEvent what);
  void setCallTimer();

public:
  CallGenDialog(AmPromptCollection& prompts, 
		int play_rand_digits, int call_time_base, int call_time_rand);
  ~CallGenDialog();

  void onStart();
  void onEarlySessionStart();
  void onSessionStart();
  void onBye(const AmSipRequest& req);
  void process(AmEvent* event);
  void onSipReply(const AmSipRequest& req, const AmSipReply& reply, AmBasicSipDialog::Status old_dlg_status);

};

#endif
// Local Variables:
// mode:C++
// End:

