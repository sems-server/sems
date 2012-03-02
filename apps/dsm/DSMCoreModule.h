/*
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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

 public:
  DSMCoreModule();
    
  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);
};

DEF_ACTION_1P(SCPlayPromptAction);
DEF_ACTION_1P(SCPlayPromptFrontAction);
DEF_ACTION_1P(SCPlayPromptLoopedAction);
DEF_ACTION_1P(SCRecordFileAction);
DEF_ACTION_1P(SCStopRecordAction);
DEF_ACTION_1P(SCGetRecordDataSizeAction);
DEF_ACTION_1P(SCGetRecordLengthAction);
DEF_ACTION_1P(SCFlushPlaylistAction);
DEF_ACTION_1P(SCSetInOutPlaylistAction);
DEF_ACTION_1P(SCStopAction);
DEF_ACTION_1P(SCConnectMediaAction);
DEF_ACTION_1P(SCDisconnectMediaAction);
DEF_ACTION_1P(SCEnableReceivingAction);
DEF_ACTION_1P(SCDisableReceivingAction);
DEF_ACTION_1P(SCEnableForceDTMFReceiving);
DEF_ACTION_1P(SCDisableForceDTMFReceiving);
DEF_ACTION_1P(SCMuteAction);
DEF_ACTION_1P(SCUnmuteAction);
DEF_ACTION_1P(SCEnableDTMFDetection);
DEF_ACTION_1P(SCDisableDTMFDetection);
DEF_ACTION_2P(SCSendDTMFAction);
DEF_ACTION_2P(SCSendDTMFSequenceAction);


DEF_ACTION_1P(SCSetPromptsAction);
DEF_ACTION_2P(SCAddSeparatorAction);

DEF_SCModSEStrArgAction(SCRepostAction);
DEF_SCModSEStrArgAction(SCJumpFSMAction);
DEF_SCModSEStrArgAction(SCCallFSMAction);
DEF_SCModSEStrArgAction(SCReturnFSMAction);

DEF_ACTION_2P(SCThrowAction);
DEF_ACTION_1P(SCThrowOnErrorAction);

DEF_ACTION_2P(SCSetAction);
DEF_ACTION_2P(SCSetSAction);
DEF_ACTION_2P(SCEvalAction);
DEF_ACTION_2P(SCAppendAction);
DEF_ACTION_2P(SCSubStrAction);
DEF_ACTION_1P(SCIncAction);
DEF_ACTION_1P(SCClearAction);
DEF_ACTION_1P(SCClearArrayAction);
DEF_ACTION_2P(SCSizeAction);
DEF_ACTION_2P(SCSetTimerAction);
DEF_ACTION_1P(SCRemoveTimerAction);
DEF_ACTION_1P(SCRemoveTimersAction);
DEF_ACTION_2P(SCLogAction);
DEF_ACTION_1P(SCLogVarsAction);
DEF_ACTION_1P(SCLogParamsAction);
DEF_ACTION_1P(SCLogSelectsAction);
DEF_ACTION_1P(SCLogAllAction);
DEF_ACTION_2P(SCGetVarAction);
DEF_ACTION_2P(SCGetParamAction);
DEF_ACTION_2P(SCSetVarAction);
DEF_ACTION_2P(SCPlayFileAction);
DEF_ACTION_2P(SCPlayFileFrontAction);
DEF_ACTION_1P(SCPlaySilenceAction);
DEF_ACTION_1P(SCPlaySilenceFrontAction);
DEF_ACTION_2P(SCPostEventAction);

DEF_ACTION_2P(SCB2BConnectCalleeAction);
DEF_ACTION_1P(SCB2BTerminateOtherLegAction);
DEF_ACTION_2P(SCB2BReinviteAction);

DEF_ACTION_1P(SCB2BAddHeaderAction);
DEF_ACTION_1P(SCB2BClearHeadersAction);
DEF_ACTION_2P(SCB2BSetHeadersAction);

DEF_ACTION_1P(SCRegisterEventQueueAction);
DEF_ACTION_1P(SCUnregisterEventQueueAction);

DEF_ACTION_2P(SCCreateSystemDSMAction);

class SCDIAction					
: public DSMAction {
  vector<string> params;
  bool get_res;
 public:
  SCDIAction(const string& arg, bool get_res);
  bool execute(AmSession* sess, DSMSession* sc_sess,
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
  bool match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
	     map<string,string>* event_params);
};

#endif
