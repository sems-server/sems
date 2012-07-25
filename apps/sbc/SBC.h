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

#include "CallLeg.h"

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

extern void assertEndCRLF(string& s);

#endif                           
