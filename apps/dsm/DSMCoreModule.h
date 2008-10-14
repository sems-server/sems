/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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
#ifndef _DSM_CORE_MODULE_H
#define _DSM_CORE_MODULE_H
#include "DSMModule.h"
#include "DSMStateEngine.h"

#include <string>
using std::string;
#include <map>
using std::map;

class AmSession;
class DSMSession;

class DSMCoreModule 
: public DSMModule {

  void splitCmd(const string& from_str, 
		string& cmd, string& params);
 public:
  DSMCoreModule();
    
  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);
};

DEF_SCStrArgAction(SCPlayPromptAction);
DEF_SCStrArgAction(SCPlayPromptLoopedAction);
DEF_SCStrArgAction(SCRecordFileAction);
DEF_SCStrArgAction(SCStopRecordAction);
DEF_SCStrArgAction(SCClosePlaylistAction);
DEF_SCStrArgAction(SCStopAction);
DEF_SCStrArgAction(SCSetPromptsAction);

DEF_SCModSEStrArgAction(SCRepostAction);
DEF_SCModSEStrArgAction(SCJumpFSMAction);
DEF_SCModSEStrArgAction(SCCallFSMAction);
DEF_SCModSEStrArgAction(SCReturnFSMAction);


DEF_TwoParAction(SCSetAction);
DEF_TwoParAction(SCAppendAction);
DEF_TwoParAction(SCSetTimerAction);
DEF_TwoParAction(SCLogAction);
DEF_TwoParAction(SCPlayFileAction);

class SCDIAction					
: public DSMAction {
  vector<string> params;
  bool get_res;
 public:
  SCDIAction(const string& arg, bool get_res);
  bool execute(AmSession* sess,
	       DSMCondition::EventType event,
	       map<string,string>* event_params);
};									

// TODO: replace with real expression matching 
class TestDSMCondition 
: public DSMCondition {
  enum CondType {
    None,
    Always,
    Eq,
    Neq,
    Less,
    Gt
  };
  string lhs;
  string rhs;
  CondType ttype;

 public:
  TestDSMCondition(const string& expr, DSMCondition::EventType e);
  bool match(AmSession* sess, DSMCondition::EventType event,
	     map<string,string>* event_params);
};

string resolveVars(const string s, AmSession* sess,
		   DSMSession* sc_sess, map<string,string>* event_params);

string trim(string const& str,char const* sepSet);

#endif
