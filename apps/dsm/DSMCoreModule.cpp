/*
 * Copyright (C) 2008 iptego GmbH
 * Copyright (C) 2010 Stefan Sayer
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

#include "DSMCoreModule.h"
#include "DSMSession.h"
#include "AmSession.h"
#include "AmSessionContainer.h"
#include "AmUtils.h"
#include "AmEventDispatcher.h"
#include "DSM.h"

#include "jsonArg.h"

DSMCoreModule::DSMCoreModule() {
}

DSMAction* DSMCoreModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  DEF_CMD("repost", SCRepostAction);
  DEF_CMD("jumpFSM", SCJumpFSMAction);
  DEF_CMD("callFSM", SCCallFSMAction);
  DEF_CMD("returnFSM", SCReturnFSMAction);

  DEF_CMD("throw", SCThrowAction);
  DEF_CMD("throwOnError", SCThrowOnErrorAction);

  DEF_CMD("stop", SCStopAction);

  DEF_CMD("playPrompt", SCPlayPromptAction);
  DEF_CMD("playPromptFront", SCPlayPromptFrontAction);
  DEF_CMD("playPromptLooped", SCPlayPromptLoopedAction);
  DEF_CMD("playFile", SCPlayFileAction);
  DEF_CMD("playFileFront", SCPlayFileFrontAction);
  DEF_CMD("playSilence", SCPlaySilenceAction);
  DEF_CMD("playSilenceFront", SCPlaySilenceFrontAction);
  DEF_CMD("recordFile", SCRecordFileAction);
  DEF_CMD("stopRecord", SCStopRecordAction);
  DEF_CMD("getRecordLength", SCGetRecordLengthAction);
  DEF_CMD("getRecordDataSize", SCGetRecordDataSizeAction);
  DEF_CMD("flushPlaylist", SCFlushPlaylistAction);
  DEF_CMD("setInOutPlaylist", SCSetInOutPlaylistAction);
  DEF_CMD("addSeparator", SCAddSeparatorAction);
  DEF_CMD("connectMedia", SCConnectMediaAction);
  DEF_CMD("disconnectMedia", SCDisconnectMediaAction);
  DEF_CMD("enableReceiving", SCEnableReceivingAction);
  DEF_CMD("disableReceiving", SCDisableReceivingAction);
  DEF_CMD("enableForceDTMFReceiving", SCEnableForceDTMFReceiving);
  DEF_CMD("disableForceDTMFReceiving", SCDisableForceDTMFReceiving);
  DEF_CMD("mute", SCMuteAction);
  DEF_CMD("unmute", SCUnmuteAction);
  DEF_CMD("enableDTMFDetection", SCEnableDTMFDetection);
  DEF_CMD("disableDTMFDetection", SCDisableDTMFDetection);
  DEF_CMD("sendDTMF", SCSendDTMFAction);
  DEF_CMD("sendDTMFSequence", SCSendDTMFSequenceAction);

  DEF_CMD("set", SCSetAction);
  DEF_CMD("sets", SCSetSAction);
  DEF_CMD("eval", SCEvalAction);
  DEF_CMD("setVar", SCSetVarAction);
  DEF_CMD("var", SCGetVarAction);
  DEF_CMD("param", SCGetParamAction);
  DEF_CMD("append", SCAppendAction);
  DEF_CMD("substr", SCSubStrAction);
  DEF_CMD("inc", SCIncAction);
  DEF_CMD("log", SCLogAction);
  DEF_CMD("clear", SCClearAction);
  DEF_CMD("clearArray", SCClearArrayAction);
  DEF_CMD("size", SCSizeAction);
  DEF_CMD("logVars", SCLogVarsAction);
  DEF_CMD("logParams", SCLogParamsAction);
  DEF_CMD("logSelects", SCLogSelectsAction);
  DEF_CMD("logAll", SCLogAllAction);

  DEF_CMD("setTimer", SCSetTimerAction);
  DEF_CMD("removeTimer", SCRemoveTimerAction);
  DEF_CMD("removeTimers", SCRemoveTimersAction);

  DEF_CMD("setPrompts", SCSetPromptsAction);

  DEF_CMD("postEvent", SCPostEventAction);

  DEF_CMD("registerEventQueue", SCRegisterEventQueueAction);
  DEF_CMD("unregisterEventQueue", SCUnregisterEventQueueAction);
  DEF_CMD("createSystemDSM", SCCreateSystemDSMAction);

  if (cmd == "DI") {
    SCDIAction * a = new SCDIAction(params, false);
    a->name = from_str;
    return a;
  }  

  if (cmd == "DIgetResult") {
    SCDIAction * a = new SCDIAction(params, true);
    a->name = from_str;
    return a;
  }  

  DEF_CMD("B2B.connectCallee", SCB2BConnectCalleeAction);
  DEF_CMD("B2B.terminateOtherLeg", SCB2BTerminateOtherLegAction);
  DEF_CMD("B2B.sendReinvite", SCB2BReinviteAction);
  DEF_CMD("B2B.addHeader", SCB2BAddHeaderAction);
  DEF_CMD("B2B.clearHeaders", SCB2BClearHeadersAction);
  DEF_CMD("B2B.setHeaders", SCB2BSetHeadersAction);

  return NULL;
}

DSMCondition* DSMCoreModule::getCondition(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  if (cmd == "keyPress") {
    DSMCondition* c = new DSMCondition();
    c->name = "key pressed: " + params;
    c->type = DSMCondition::Key;
    c->params["key"] = params;
    return c;
  }

  if (cmd == "test")
    return new TestDSMCondition(params, DSMCondition::Any);

  if ((cmd == "keyTest") || (cmd == "key"))
    return new TestDSMCondition(params, DSMCondition::Key);

  if ((cmd == "timerTest") || (cmd == "timer"))
    return new TestDSMCondition(params, DSMCondition::Timer);

  if ((cmd == "noAudioTest") || (cmd == "noAudio"))
    return new TestDSMCondition(params, DSMCondition::NoAudio);

  if ((cmd == "separatorTest") || (cmd == "separator"))
    return new TestDSMCondition(params, DSMCondition::PlaylistSeparator);

  if (cmd == "hangup") 
    return new TestDSMCondition(params, DSMCondition::Hangup);  

  if ((cmd == "eventTest") || (cmd == "event"))
    return new TestDSMCondition(params, DSMCondition::DSMEvent);  

  if (cmd == "invite") 
    return new TestDSMCondition(params, DSMCondition::Invite);  

  if (cmd == "earlySession")
    return new TestDSMCondition(params, DSMCondition::EarlySession);

  if (cmd == "sessionStart") 
    return new TestDSMCondition(params, DSMCondition::SessionStart);  

  if (cmd == "ringing") 
    return new TestDSMCondition(params, DSMCondition::Ringing);

  if (cmd == "early") 
    return new TestDSMCondition(params, DSMCondition::EarlySession);

  if (cmd == "failed") 
    return new TestDSMCondition(params, DSMCondition::FailedCall);  

  if (cmd == "B2B.otherReply") 
    return new TestDSMCondition(params, DSMCondition::B2BOtherReply);  

  if (cmd == "B2B.otherBye") 
    return new TestDSMCondition(params, DSMCondition::B2BOtherBye);  

  if (cmd == "sipRequest") 
    return new TestDSMCondition(params, DSMCondition::SipRequest);  

  if (cmd == "sipReply") 
    return new TestDSMCondition(params, DSMCondition::SipReply);  

  if (cmd == "remoteDisappeared") 
    return new TestDSMCondition(params, DSMCondition::RemoteDisappeared);  

  if (cmd == "sessionTimeout")
    return new TestDSMCondition(params, DSMCondition::SessionTimeout);

  if (cmd == "rtpTimeout")
    return new TestDSMCondition(params, DSMCondition::RtpTimeout);

  if (cmd == "jsonRpcRequest") 
    return new TestDSMCondition(params, DSMCondition::JsonRpcRequest);  

  if (cmd == "jsonRpcResponse") 
    return new TestDSMCondition(params, DSMCondition::JsonRpcResponse);  

  if (cmd == "startup")
    return new TestDSMCondition(params, DSMCondition::Startup);

  if (cmd == "reload")
    return new TestDSMCondition(params, DSMCondition::Reload);

  if (cmd == "system")
    return new TestDSMCondition(params, DSMCondition::System);

  if (cmd == "rtpTimeout")
    return new TestDSMCondition(params, DSMCondition::RTPTimeout);

  return NULL;
}

EXEC_ACTION_START(SCPlayPromptAction) {
  sc_sess->playPrompt(resolveVars(arg, sess, sc_sess, event_params));
} EXEC_ACTION_END;

EXEC_ACTION_START(SCPlayPromptFrontAction) {
  sc_sess->playPrompt(resolveVars(arg, sess, sc_sess, event_params), false, true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCSetPromptsAction) {
  sc_sess->setPromptSet(resolveVars(arg, sess, sc_sess, event_params));
} EXEC_ACTION_END;

CONST_ACTION_2P(SCAddSeparatorAction, ',', true);
EXEC_ACTION_START(SCAddSeparatorAction){
  bool front = resolveVars(par2, sess, sc_sess, event_params) == "true";
  sc_sess->addSeparator(resolveVars(par1, sess, sc_sess, event_params), front);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCPlayPromptLoopedAction){
  sc_sess->playPrompt(resolveVars(arg, sess, sc_sess, event_params), true);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCPostEventAction, ',', true);
EXEC_ACTION_START(SCPostEventAction){
  string sess_id = resolveVars(par1, sess, sc_sess, event_params);
  string var = resolveVars(par2, sess, sc_sess, event_params);
  DSMEvent* ev = new DSMEvent();
  if (!var.empty()) {
    if (var == "var")
      ev->params = sc_sess->var;
    else {
      vector<string> vars = explode(var, ";");
      for (vector<string>::iterator it =
	     vars.begin(); it != vars.end(); it++) {
	string varname = *it;

	if (varname.length() && varname[varname.length()-1]=='.') {
	  DBG("adding postEvent param %s (struct)\n", varname.c_str());
	
	  map<string, string>::iterator lb = sc_sess->var.lower_bound(varname);
	  while (lb != sc_sess->var.end()) {
	    if ((lb->first.length() < varname.length()) ||
		strncmp(lb->first.c_str(), varname.c_str(), varname.length()))
	      break;
	    ev->params[lb->first] = lb->second;
	    lb++;
	  }
	} else {
	  DBG("adding postEvent param %s=%s\n",
	      it->c_str(), sc_sess->var[*it].c_str());
	  ev->params[*it] = sc_sess->var[*it];
	}
      }
    }
  }

  DBG("posting event to session '%s'\n", sess_id.c_str());
  if (!AmSessionContainer::instance()->postEvent(sess_id, ev)) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("event could not be posted\n");
  } else {
    sc_sess->CLR_ERRNO;
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCPlayFileAction, ',', true);
EXEC_ACTION_START(SCPlayFileAction) {
  bool loop = 
    resolveVars(par2, sess, sc_sess, event_params) == "true";
  DBG("par1 = '%s', par2 = %s\n", par1.c_str(), par2.c_str());
  sc_sess->playFile(resolveVars(par1, sess, sc_sess, event_params), 
		    loop);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCPlayFileFrontAction, ',', true);
EXEC_ACTION_START(SCPlayFileFrontAction) {
  bool loop = 
    resolveVars(par2, sess, sc_sess, event_params) == "true";
  DBG("par1 = '%s', par2 = %s\n", par1.c_str(), par2.c_str());
  sc_sess->playFile(resolveVars(par1, sess, sc_sess, event_params), 
		    loop, true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCPlaySilenceAction) {
  int length;
  string length_str = resolveVars(arg, sess, sc_sess, event_params);
  if (!str2int(length_str, length)) {
    throw DSMException("core", "cause", "cannot parse number");
  }
  sc_sess->playSilence(length);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCPlaySilenceFrontAction) {
  int length;
  string length_str = resolveVars(arg, sess, sc_sess, event_params);
  if (!str2int(length_str, length)) {
    throw DSMException("core", "cause", "cannot parse number");
  }
  sc_sess->playSilence(length, true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCRecordFileAction) {
  sc_sess->recordFile(resolveVars(arg, sess, sc_sess, event_params));
} EXEC_ACTION_END;

EXEC_ACTION_START(SCStopRecordAction) {
  sc_sess->stopRecord();
} EXEC_ACTION_END;

EXEC_ACTION_START(SCGetRecordLengthAction) {
  string varname = resolveVars(arg, sess, sc_sess, event_params);
  if (varname.empty())
    varname = "record_length";
  sc_sess->var[varname]=int2str(sc_sess->getRecordLength());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCGetRecordDataSizeAction) {
  string varname = resolveVars(arg, sess, sc_sess, event_params);
  if (varname.empty())
    varname = "record_data_size";
  sc_sess->var[varname]=int2str(sc_sess->getRecordDataSize());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCFlushPlaylistAction) {
  sc_sess->flushPlaylist();
} EXEC_ACTION_END;


EXEC_ACTION_START(SCSetInOutPlaylistAction) {
  sc_sess->setInOutPlaylist();
} EXEC_ACTION_END;

EXEC_ACTION_START(SCConnectMediaAction) {
  sc_sess->connectMedia();
} EXEC_ACTION_END;

EXEC_ACTION_START(SCDisconnectMediaAction) {
  sc_sess->disconnectMedia();
} EXEC_ACTION_END;

EXEC_ACTION_START(SCEnableReceivingAction) {
  DBG("enabling RTP receving in session\nb");
  sess->setReceiving(true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCDisableReceivingAction) {
  DBG("disabling RTP receving in session\nb");
  sess->setReceiving(false);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCEnableForceDTMFReceiving) {
  DBG("enabling forced DTMF RTP receving in session\nb");
  sess->setForceDtmfReceiving(true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCDisableForceDTMFReceiving) {
  DBG("disabling forced DTMF RTP receving in session\nb");
  sess->setForceDtmfReceiving(false);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMuteAction) {
  sc_sess->mute();
} EXEC_ACTION_END;

EXEC_ACTION_START(SCUnmuteAction) {
  sc_sess->unmute();
} EXEC_ACTION_END;


EXEC_ACTION_START(SCEnableDTMFDetection) {
  sess->setDtmfDetectionEnabled(true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCDisableDTMFDetection) {
  sess->setDtmfDetectionEnabled(false);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCThrowAction, ',', true);
EXEC_ACTION_START(SCThrowAction) {
  map<string, string> e_args;
  e_args["type"] = resolveVars(par1, sess, sc_sess, event_params); 
  DBG("throwing DSMException type '%s'\n", e_args["type"].c_str());

  string e_params = resolveVars(par2, sess, sc_sess, event_params);
  
  // inefficient param-split
  vector<string> params = explode(e_params, ";");
  for (vector<string>::iterator it=
	 params.begin(); it != params.end(); it++) {
    vector<string> n = explode(*it, "=");
    if (n.size()==2) {
      e_args[n[0]]=n[1];
    }
  }
  
  throw DSMException(e_args);

} EXEC_ACTION_END;

EXEC_ACTION_START(SCThrowOnErrorAction) {
  if (sc_sess->var["errno"].empty())
    EXEC_ACTION_STOP;

  map<string, string> e_args;
  e_args["type"] = sc_sess->var["errno"];

  DBG("throwing DSMException type '%s'\n", e_args["type"].c_str());
  e_args["text"] = sc_sess->var["strerror"];
  
  throw DSMException(e_args);

} EXEC_ACTION_END;


EXEC_ACTION_START(SCStopAction) {
  if (resolveVars(arg, sess, sc_sess, event_params) == "true") {
    DBG("sending bye\n");
    sess->dlg.bye();
  }
  sess->setStopped();
} EXEC_ACTION_END;

#define DEF_SCModActionExec(clsname)				\
								\
  bool clsname::execute(AmSession* sess, DSMSession* sc_sess,	\
			DSMCondition::EventType event,		\
			map<string,string>* event_params) {	\
    return true;						\
  }								\

DEF_SCModActionExec(SCRepostAction);
DSMAction::SEAction SCRepostAction::getSEAction(string& param,
						AmSession* sess, DSMSession* sc_sess,
						DSMCondition::EventType event,
						map<string,string>* event_params) {
  return Repost; 
}

DEF_SCModActionExec(SCJumpFSMAction);
DSMAction::SEAction SCJumpFSMAction::getSEAction(string& param,
						 AmSession* sess, DSMSession* sc_sess,
						 DSMCondition::EventType event,
						 map<string,string>* event_params) {
  param = resolveVars(arg, sess, sc_sess, event_params);
  return Jump; 
}

DEF_SCModActionExec(SCCallFSMAction);
DSMAction::SEAction SCCallFSMAction::getSEAction(string& param,
						 AmSession* sess, DSMSession* sc_sess,
						 DSMCondition::EventType event,
						 map<string,string>* event_params) {
  param = resolveVars(arg, sess, sc_sess, event_params);
  return Call; 
}

DEF_SCModActionExec(SCReturnFSMAction);
DSMAction::SEAction SCReturnFSMAction::getSEAction(string& param,
						   AmSession* sess, DSMSession* sc_sess,
						   DSMCondition::EventType event,
						   map<string,string>* event_params) {
  return Return; 
}

#undef DEF_SCModActionExec

CONST_ACTION_2P(SCLogAction, ',', false);
EXEC_ACTION_START(SCLogAction) {
  unsigned int lvl;
  if (str2i(resolveVars(par1, sess, sc_sess, event_params), lvl)) {
    ERROR("unknown log level '%s'\n", par1.c_str());
    EXEC_ACTION_STOP;
  }
  string l_line = resolveVars(par2, sess, sc_sess, event_params).c_str();
  _LOG((int)lvl, "FSM: %s '%s'\n", (par2 != l_line)?par2.c_str():"",
       l_line.c_str());
} EXEC_ACTION_END;

void log_vars(const string& l_arg, AmSession* sess,
	      DSMSession* sc_sess, map<string,string>* event_params) {
  unsigned int lvl;
  if (str2i(resolveVars(l_arg, sess, sc_sess, event_params), lvl)) {
    ERROR("unknown log level '%s'\n", l_arg.c_str());
    return;
  }

  _LOG((int)lvl, "FSM: variables set ---\n");
  for (map<string, string>::iterator it = 
	 sc_sess->var.begin(); it != sc_sess->var.end(); it++) {
    _LOG((int)lvl, "FSM:  $%s='%s'\n", it->first.c_str(), it->second.c_str());
  }
  _LOG((int)lvl, "FSM: variables end ---\n");
}

EXEC_ACTION_START(SCLogVarsAction) {
  log_vars(arg, sess, sc_sess, event_params);
} EXEC_ACTION_END;

void log_params(const string& l_arg, AmSession* sess,
		DSMSession* sc_sess, map<string,string>* event_params) {
  unsigned int lvl;
  if (str2i(resolveVars(l_arg, sess, sc_sess, event_params), lvl)) {
    ERROR("unknown log level '%s'\n", l_arg.c_str());
    return;
  }

  if (NULL == event_params) {
    _LOG((int)lvl, "FSM: no event params ---\n");
    return;
  }

  _LOG((int)lvl, "FSM: params set ---\n");
  for (map<string, string>::iterator it = 
	 event_params->begin(); it != event_params->end(); it++) {
    _LOG((int)lvl, "FSM:  #%s='%s'\n", it->first.c_str(), it->second.c_str());
  }
  _LOG((int)lvl, "FSM: params end ---\n");
}

EXEC_ACTION_START(SCLogParamsAction) {
  log_params(arg, sess, sc_sess, event_params);
} EXEC_ACTION_END;


void log_selects(const string& l_arg, AmSession* sess,
		 DSMSession* sc_sess, map<string,string>* event_params) {
  unsigned int lvl;
  if (str2i(resolveVars(l_arg, sess, sc_sess, event_params), lvl)) {
    ERROR("unknown log level '%s'\n", l_arg.c_str());
    return;
  }

  _LOG((int)lvl, "FSM: selects set ---\n");

#define SELECT_LOG(select_name)					\
  _LOG((int)lvl, "FSM:  @%s='%s'\n", select_name,			\
       resolveVars("@" select_name, sess, sc_sess, event_params).c_str());	

  SELECT_LOG("local_tag");
  SELECT_LOG("user");
  SELECT_LOG("domain");
  SELECT_LOG("remote_tag");
  SELECT_LOG("callid");
  SELECT_LOG("local_uri");
  SELECT_LOG("remote_uri");
#undef SELECT_LOG
  _LOG((int)lvl, "FSM: selects end ---\n");
}

EXEC_ACTION_START(SCLogSelectsAction) {
  log_selects(arg, sess, sc_sess, event_params);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCLogAllAction) {
  log_vars(arg, sess, sc_sess, event_params);
  log_params(arg, sess, sc_sess, event_params);
  log_selects(arg, sess, sc_sess, event_params);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCSetAction,'=', false);
EXEC_ACTION_START(SCSetAction) {
  if (par1.length() && par1[0] == '#') {
    // set param
    if (NULL != event_params) {
      string res = resolveVars(par2, sess, sc_sess, event_params);
      (*event_params)[par1.substr(1)] = res;
      DBG("set #%s='%s'\n", par1.substr(1).c_str(), res.c_str());
    } else {
      DBG("not setting %s (no param set)\n", par1.c_str());
    }
  } else {
    // set variable
    string var_name = (par1.length() && par1[0] == '$')?
      par1.substr(1) : par1;

    sc_sess->var[var_name] = resolveVars(par2, sess, sc_sess, event_params);
    
    DBG("set $%s='%s'\n", 
	var_name.c_str(), sc_sess->var[var_name].c_str());
  }
} EXEC_ACTION_END;

string replaceParams(const string& q, AmSession* sess, DSMSession* sc_sess, 
		     map<string,string>* event_params) {
  string res = q;
  size_t repl_pos = 0;
  while (repl_pos<res.length()) {
    size_t rstart = res.find_first_of("$#@", repl_pos);
    repl_pos = rstart+1;
    if (rstart == string::npos) 
      break;
    if (rstart && res[rstart-1] == '\\') // escaped
      continue;
    size_t rend;
    if (res.length() > rstart+1 && 
	(res[rstart+1] == '(' ||
	 res[rstart+1] == '"' ||
	 res[rstart+1] == '\''
	 ))
      rend = res.find_first_of(" ,()[]$#@\t;:'\"", rstart+2);
    else 
      rend = res.find_first_of(" ,()[]$#@\t;:'\"", rstart+1);
    if (rend==string::npos)
      rend = res.length();
    string keyname = res.substr(rstart+1, rend-rstart-1);

    if (keyname.length()>2) {
      if ((keyname[0] == '(' && res[rend] == ')') ||
	  (keyname[0] == res[rend] &&
	   (keyname[0] == '"' ||keyname[0] == '\''))) {
	keyname = keyname.substr(1);
	if (rend != res.length())
	  rend++;
      }
    }
    // todo: simply use resolveVars (?)
    switch(res[rstart]) {
    case '$': {
      
      if (sc_sess->var.find(keyname) == sc_sess->var.end())
	res.erase(rstart, rend-rstart); 
      else 
	res.replace(rstart, rend-rstart, sc_sess->var[keyname]); 
    } break;
    case '#':
      if (NULL!=event_params) {
	if (event_params->find(keyname) != event_params->end())
	  res.replace(rstart, rend-rstart, (*event_params)[keyname]);
	else
	  res.erase(rstart, rend-rstart);	
      } break;
    case '@': {
      // todo: optimize 
      res.replace(rstart, rend-rstart, 
		  resolveVars("@"+keyname, sess, sc_sess, event_params));
    } break;
    default: break;
    }
  }
  return res;
}
CONST_ACTION_2P(SCSetSAction,'=', false);
EXEC_ACTION_START(SCSetSAction) {
  if (par1.length() && par1[0] == '#') {
    // set param
    if (NULL != event_params) {
      string res = replaceParams(par2, sess, sc_sess, event_params);
      (*event_params)[par1.substr(1)] = res;
      DBG("set #%s='%s'\n", par1.substr(1).c_str(), res.c_str());
    } else {
      DBG("not set %s (no param set)\n", par1.c_str());
    }
  } else {
    // set variable
    string var_name = (par1.length() && par1[0] == '$')?
      par1.substr(1) : par1;

    sc_sess->var[var_name] = replaceParams(par2, sess, sc_sess, event_params);
    
    DBG("set $%s='%s'\n", 
	var_name.c_str(), sc_sess->var[var_name].c_str());
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCEvalAction,'=', false);
EXEC_ACTION_START(SCEvalAction) {
  string var_name = (par1.length() && par1[0] == '$')?
    par1.substr(1) : par1;

  sc_sess->var[var_name] = resolveVars(par2, sess, sc_sess, event_params, true);
  DBG("eval $%s='%s'\n", 
      var_name.c_str(), sc_sess->var[var_name].c_str());
} EXEC_ACTION_END;

CONST_ACTION_2P(SCSetVarAction,'=', false);
EXEC_ACTION_START(SCSetVarAction) {
  string var_name = resolveVars(par1, sess, sc_sess, event_params);
  sc_sess->var[var_name] = resolveVars(par2, sess, sc_sess, event_params);
  DBG("set $%s='%s'\n", 
      var_name.c_str(), sc_sess->var[var_name].c_str());
} EXEC_ACTION_END;

CONST_ACTION_2P(SCGetParamAction,'=', false);
EXEC_ACTION_START(SCGetParamAction){

  string dst_var_name = (par1.length() && par1[0] == '$')?
    par1.substr(1) : par1;
  string param_name = resolveVars(par2, sess, sc_sess, event_params);
  
  DBG("param_name = %s, dst = %s\n", param_name.c_str(), dst_var_name.c_str());

  if (NULL==event_params) {
    sc_sess->var[dst_var_name] = "";
    EXEC_ACTION_STOP;
  }

  map<string, string>::iterator it = event_params->find(param_name);
  if (it != event_params->end()) {
    sc_sess->var[dst_var_name] = it->second;
  } else {
    sc_sess->var[dst_var_name] = "";
  }
  
  DBG("set $%s='%s'\n", 
      dst_var_name.c_str(), sc_sess->var[dst_var_name].c_str());
} EXEC_ACTION_END;

CONST_ACTION_2P(SCGetVarAction,'=', false);
EXEC_ACTION_START(SCGetVarAction){
  string dst_var_name = (par1.length() && par1[0] == '$')?
    par1.substr(1) : par1;
  string var_name = resolveVars(par2, sess, sc_sess, event_params);
  
  DBG("var_name = %s, dst = %s\n", var_name.c_str(), dst_var_name.c_str());
  sc_sess->var[dst_var_name] = sc_sess->var[var_name];
  DBG("set $%s='%s'\n", 
      dst_var_name.c_str(), sc_sess->var[dst_var_name].c_str());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCClearAction) {
  string var_name = (arg.length() && arg[0] == '$')?
    arg.substr(1) : arg;
  DBG("clear variable '%s'\n", var_name.c_str());
  sc_sess->var.erase(var_name);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCClearArrayAction) {
  string varprefix = (arg.length() && arg[0] == '$')?
    arg.substr(1) : arg;
  DBG("clear variable array '%s.*'\n", varprefix.c_str());

  varprefix+=".";

  map<string, string>::iterator lb = sc_sess->var.lower_bound(varprefix);
  while (lb != sc_sess->var.end()) {
    if ((lb->first.length() < varprefix.length()) ||
	strncmp(lb->first.c_str(), varprefix.c_str(),varprefix.length()))
      break;
    map<string, string>::iterator lb_d = lb;
    lb++;
    sc_sess->var.erase(lb_d);    
  }

} EXEC_ACTION_END;

CONST_ACTION_2P(SCSizeAction, ',', false);
EXEC_ACTION_START(SCSizeAction) {
  string array_name = par1;
  if (array_name.length() && array_name[0]=='$')
    array_name.erase(0,1);

  string dst_name = par2;
  if (dst_name.length()&&dst_name[0]=='$')
    dst_name.erase(0,1);


  unsigned int a_size = 0;
  while (true) {
    string ai_name = array_name+"["+int2str(a_size)+"]";
    VarMapT::iterator lb = sc_sess->var.lower_bound(ai_name);
    if (lb == sc_sess->var.end() ||
	lb->first.substr(0,ai_name.length()) != ai_name)
      break;
    a_size++;
  }
  string res = int2str(a_size);
  sc_sess->var[dst_name] = res;
  DBG("set $%s=%s\n", dst_name.c_str(), res.c_str());
} EXEC_ACTION_END;


CONST_ACTION_2P(SCAppendAction,',', false);
EXEC_ACTION_START(SCAppendAction) {
  string var_name = (par1.length() && par1[0] == '$')?
    par1.substr(1) : par1;

  sc_sess->var[var_name] += resolveVars(par2, sess, sc_sess, event_params);

  DBG("$%s now '%s'\n", 
      var_name.c_str(), sc_sess->var[var_name].c_str());
} EXEC_ACTION_END;

CONST_ACTION_2P(SCSubStrAction,',', false);
EXEC_ACTION_START(SCSubStrAction) {
  string var_name = (par1.length() && par1[0] == '$')?
    par1.substr(1) : par1;
  unsigned int pos = 0;
  unsigned int pos2 = 0;
  size_t c_pos = par2.find(",");
  if (c_pos == string::npos) {
    if (str2i(resolveVars(par2, sess, sc_sess, event_params), pos)) {
      ERROR("substr length '%s' unparseable\n",
	    resolveVars(par2, sess, sc_sess, event_params).c_str());
      return false;
    }
  } else {
    if (str2i(resolveVars(par2.substr(0, c_pos), sess, sc_sess, event_params), pos)) {
      ERROR("substr length '%s' unparseable\n",
	    resolveVars(par2.substr(0, c_pos), sess, sc_sess, event_params).c_str());
      return false;
    }

    if (str2i(resolveVars(par2.substr(c_pos+1), sess, sc_sess, event_params), pos2)) {
      ERROR("substr length '%s' unparseable\n",
	    resolveVars(par2.substr(0, c_pos-1), sess, sc_sess, event_params).c_str());
      return false;
    }
  }

  try {
    if (pos2 == 0)
      sc_sess->var[var_name] = sc_sess->var[var_name].substr(pos);
    else 
      sc_sess->var[var_name] = sc_sess->var[var_name].substr(pos, pos2);
  } catch(...) {
    ERROR("in substr\n");
    return false;
  }  

  DBG("$%s now '%s'\n", 
      var_name.c_str(), sc_sess->var[var_name].c_str());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCIncAction) {
  string var_name = (arg.length() && arg[0] == '$')?
    arg.substr(1) : arg;
  unsigned int val = 0;
  str2i(sc_sess->var[var_name], val);
  sc_sess->var[var_name] = int2str(val+1);

  DBG("inc: $%s now '%s'\n", 
      var_name.c_str(), sc_sess->var[var_name].c_str());

} EXEC_ACTION_END;

CONST_ACTION_2P(SCSetTimerAction,',', false);
EXEC_ACTION_START(SCSetTimerAction) {

  unsigned int timerid;
  if (str2i(resolveVars(par1, sess, sc_sess, event_params), timerid)) {
    ERROR("timer id '%s' not decipherable\n", 
	  resolveVars(par1, sess, sc_sess, event_params).c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("timer id '"+
			  resolveVars(par1, sess, sc_sess, event_params)+
			  "' not decipherable\n");
    EXEC_ACTION_STOP;
  }

  unsigned int timeout;
  if (str2i(resolveVars(par2, sess, sc_sess, event_params), timeout)) {
    ERROR("timeout value '%s' not decipherable\n", 
	  resolveVars(par2, sess, sc_sess, event_params).c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("timeout value '"+
			  resolveVars(par2, sess, sc_sess, event_params)+
			  "' not decipherable\n");
    EXEC_ACTION_STOP;
  }

  if (!sess->setTimer(timerid, timeout)) {
    ERROR("load session_timer module for timers.\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_CONFIG);
    sc_sess->SET_STRERROR("load sess_timer module for timers.\n");
    EXEC_ACTION_STOP;
  }

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;


EXEC_ACTION_START(SCRemoveTimerAction) {

  unsigned int timerid;
  string timerid_s = resolveVars(arg, sess, sc_sess, event_params);
  if (str2i(timerid_s, timerid)) {
    ERROR("timer id '%s' not decipherable\n", timerid_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("timer id '"+timerid_s+"' not decipherable\n");
    return false;
  }

  if (!sess->removeTimer(timerid)) {
    ERROR("load session_timer module for timers.\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_CONFIG);
    sc_sess->SET_STRERROR("load session_timer module for timers.\n");
    EXEC_ACTION_STOP;
  }

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

EXEC_ACTION_START(SCRemoveTimersAction) {

  DBG("removing timers for session %s\n", sess->getLocalTag().c_str());
  if (!sess->removeTimers()) {
    ERROR("load session_timer module for timers.\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_CONFIG);
    sc_sess->SET_STRERROR("load sess_timer module for timers.\n");
    EXEC_ACTION_STOP;
  }

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;



// TODO: replace with real expression matching 
TestDSMCondition::TestDSMCondition(const string& expr, DSMCondition::EventType evt) {

  type = evt;

  if (expr.empty()) {
    ttype = Always;
    return;
  }
 
  ttype = None;
  
  size_t p = expr.find("==");
  size_t p2;
  if (p != string::npos) {
    ttype = Eq; p2 = p+2;
  } else {
    p = expr.find("!=");
    if (p != string::npos)  {
      ttype = Neq; p2 = p+2;
    } else {
      p = expr.find("<");
      if (p != string::npos) {
	ttype = Less; p2 = p+1;
      } else {
	p = expr.find(">");
	if (p != string::npos)  {
	  ttype = Gt; p2 = p+1;
	} else {
	  ERROR("expression '%s' not understood\n", 
		expr.c_str());
	  return;
	}
      }
    }
  }

  lhs = trim(expr.substr(0, p), " ");
  rhs = trim(expr.substr(p2,expr.length()-p2+1), " ");

  name = expr;
}

bool TestDSMCondition::match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
			  map<string,string>* event_params) {
  if (ttype == None || (type != DSMCondition::Any && type != event))
    return false;

  if (ttype == Always)
    return true;

  if (!sc_sess) {
    ERROR("wrong session type\n");
    return false;
  }
  
  string l;
  string r;
  if (lhs.length() > 5 && 
      (lhs.substr(0, 4) == "len(") && lhs[lhs.length()-1] == ')') {
    l = int2str((unsigned int)resolveVars(lhs.substr(4, lhs.length()-5), sess, sc_sess, event_params).length());
  } else {    
    l   = resolveVars(lhs, sess, sc_sess, event_params);
  }
  if (rhs.length() > 5 && 
      rhs.substr(0, 4) == "len(" && rhs[rhs.length()-1] == ')') {
    r = resolveVars(rhs.substr(4, rhs.length()-5), sess, sc_sess, event_params).length();
  } else {    
    r   = resolveVars(rhs, sess, sc_sess, event_params);
  }

//   string r = resolveVars(rhs, sess, sc_sess, event_params);

  DBG("test '%s' vs '%s'\n", l.c_str(), r.c_str());

  switch (ttype) {
  case Eq: {
    size_t starpos = r.find("*");
    if (starpos==string::npos)
      return l == r;
    else {
      if (l.size()<starpos)
	return false;
      return r.substr(0, starpos) == l.substr(0, starpos);
    }
  }
  case Neq: return l != r;
  case Less: {
    char* endptr = NULL;
    long l_i = strtol(l.c_str(), &endptr, 10);
    if (endptr && *endptr  == '\0') {
      long r_i = strtol(r.c_str(), &endptr, 10);
      if (endptr && *endptr  == '\0') 
	return l_i < r_i;
      }
    return l < r;
  }
  case Gt: {
    char* endptr = NULL;
    long l_i = strtol(l.c_str(), &endptr, 10);
    if (endptr && *endptr  == '\0') {
      long r_i = strtol(r.c_str(), &endptr, 10);
      if (endptr && *endptr  == '\0') 
	return l_i > r_i;
      }
    return l > r;
  } 
  default: return false;
  }  
}


SCDIAction::SCDIAction(const string& arg, bool get_res) 
  : get_res(get_res) {
  // TODO: don't ignore "" (don't use explode here)
  params = explode(arg,",");
  if (params.size()<2) {
    ERROR("DI needs at least: mod_name, "
	  "function_name\n");
    return;
  }
}

void string2argarray(const string& key, const string& val, AmArg& res) {
  if (key.empty())
    return;

  if (!(isArgStruct(res) || isArgUndef(res))) {
    WARN("array element [%s] is shadowed by value '%s'\n", 
	 key.c_str(), AmArg::print(res).c_str());
    return;
  }

  size_t delim = key.find(".");
  if (delim == string::npos) {
    res[key]=val;
    return;
  }
  string2argarray(key.substr(delim+1), val, res[key.substr(0,delim)]);
}

EXEC_ACTION_START(SCDIAction) {

  if (params.size() < 2) {
    ERROR("DI needs at least: mod_name, "
	  "function_name (in '%s')\n", name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("DI needs at least: mod_name, "
			  "function_name (in '"+name+"%s')\n");
    EXEC_ACTION_STOP;
  }

  vector<string>::iterator p_it=params.begin();
  string fact_name = trim(*p_it, " \"");
  AmDynInvokeFactory* fact = 
    AmPlugIn::instance()->getFactory4Di(fact_name);

  if(!fact) {
    ERROR("load module for factory '%s'.\n", fact_name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_CONFIG);
    sc_sess->SET_STRERROR("load module for factory '"+fact_name+"'.\n");
    EXEC_ACTION_STOP;
  }
  AmDynInvoke* di_inst = fact->getInstance();
  if(!di_inst) {
    ERROR("load module for factory '%s'\n", fact_name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_CONFIG);
    sc_sess->SET_STRERROR("load module for factory '"+fact_name+"'.\n");
    EXEC_ACTION_STOP;
  }
  p_it++; 

  string func_name = trim(*p_it, " \""); 
  p_it++;

  AmArg di_args;
  
  while (p_it != params.end()) {
    string p = trim(*p_it, " \t");
    if (p.length() && p[0] == '"') {
      di_args.push(trim(p,"\"").c_str());
    } else if (p.length() > 5 && 
	       p.substr(0, 5) =="(int)") {
      p = resolveVars(p.substr(5), sess, sc_sess, event_params);
      char* endptr = NULL;
      long p_i = strtol(p.c_str(), &endptr, 10);
      if (endptr && *endptr  == '\0') {
	di_args.push((int)p_i);
      } else {
	ERROR("converting value '%s' to int\n", 
	      p.c_str());
	sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
	sc_sess->SET_STRERROR("converting value '"+p+"' to int\n");
	EXEC_ACTION_STOP;
      }
    } else if (p.length() > 8 &&  
	       p.substr(0, 8) =="(struct)") {
      p.erase(0, 8);
      AmArg var_struct;
      string varprefix = p+".";
      bool has_vars = false;
      map<string, string>::iterator lb = sc_sess->var.lower_bound(varprefix);
      while (lb != sc_sess->var.end()) {
	if ((lb->first.length() < varprefix.length()) ||
	    strncmp(lb->first.c_str(), varprefix.c_str(),varprefix.length()))
	  break;
	
	string varname = lb->first.substr(varprefix.length());
	if (varname.find(".") == string::npos)
	  var_struct[varname] = lb->second;
	else
	  string2argarray(varname, lb->second, var_struct);
	
	lb++;
	has_vars = true;
      }
      di_args.push(var_struct);
    } else if (p.length() > 7 &&  
	       p.substr(0, 7) =="(array)") {
      p.erase(0, 7);
      di_args.push(AmArg());
      AmArg& var_array = di_args.get(di_args.size()-1);
      
      unsigned int i=0;
      while (true) {
	map<string, string>::iterator it = 
	  sc_sess->var.find(p+"["+int2str(i)+"]");
	if (it == sc_sess->var.end())
	  break;
	var_array.push(it->second);
	i++;
      }
    } else if (p.length() > 6 &&  
	       p.substr(0, 6) == "(json)") {
      p.erase(0, 6);
      if (p.length() && p[0] == '"') {
	// remove quotes if parameter in form (json)"{"json":"object"}"
	p = trim(p,"\"");
      } else {
	p = resolveVars(p, sess, sc_sess, event_params);
      }

      di_args.push(AmArg());
      AmArg& var_json = di_args.get(di_args.size()-1);
      if (!json2arg(p, var_json)) {
	WARN("Error parsing JSON object '%s'\n", p.c_str());
	// todo: throw exception? 
      }      
    } else {
      di_args.push(resolveVars(p, sess, sc_sess, event_params).c_str());
    }
    p_it++;
  }

  sc_sess->di_res.clear();
  DBG("executing DI function '%s'\n", func_name.c_str());
  try {
    di_inst->invoke(func_name, di_args, sc_sess->di_res);
  } catch (const AmDynInvoke::NotImplemented& ni) {
    ERROR("not implemented DI function '%s'\n", 
	  ni.what.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("not implemented DI function '"+ni.what+"'\n");
    EXEC_ACTION_STOP;
  } catch (const AmArg::OutOfBoundsException& oob) {
    ERROR("out of bounds in  DI call '%s'\n", 
	  name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("out of bounds in  DI call '"+name+"'\n");
    EXEC_ACTION_STOP;
  } catch (const AmArg::TypeMismatchException& oob) {
    ERROR("type mismatch  in  DI call '%s'\n", 
	  name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("type mismatch in  DI call '"+name+"'\n");
    EXEC_ACTION_STOP;
  } catch (...) {
    ERROR("unexpected Exception  in  DI call '%s'\n", 
	  name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("unexpected Exception in  DI call '"+name+"'\n");
    EXEC_ACTION_STOP;
  }

  bool flag_error = false;
  if (get_res) {
    // rudimentary variables conversion...
    if (isArgCStr(sc_sess->di_res)) 
      sc_sess->var["DI_res"] = sc_sess->di_res.asCStr();
    else if (isArgInt(sc_sess->di_res)) 
      sc_sess->var["DI_res"] = int2str(sc_sess->di_res.asInt());
    else if (isArgArray(sc_sess->di_res)) {
      // copy results to $DI_res0..$DI_resn
      for (size_t i=0;i<sc_sess->di_res.size();i++) {
	switch (sc_sess->di_res.get(i).getType()) {
	case AmArg::CStr: {
	  sc_sess->var["DI_res"+int2str((unsigned int)i)] =
	    sc_sess->di_res.get(i).asCStr();
	} break;
	case AmArg::Int: {
	  sc_sess->var["DI_res"+int2str((unsigned int)i)] =
	    int2str(sc_sess->di_res.get(i).asInt());
	} break;
	default: {
	  ERROR("unsupported AmArg return type!");
	  flag_error = true;
	  sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
	  sc_sess->SET_STRERROR("unsupported AmArg return type");
	}
	}
      }
    } else {
      ERROR("unsupported AmArg return type!");
      flag_error = true;
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("unsupported AmArg return type");
    }
  }
  if (!flag_error) {
    sc_sess->CLR_ERRNO;
  }

} EXEC_ACTION_END;
  

CONST_ACTION_2P(SCB2BConnectCalleeAction,',', false);
EXEC_ACTION_START(SCB2BConnectCalleeAction) {  
  string remote_party = resolveVars(par1, sess, sc_sess, event_params);
  string remote_uri = resolveVars(par2, sess, sc_sess, event_params);
  sc_sess->B2BconnectCallee(remote_party, remote_uri);
} EXEC_ACTION_END;
 
EXEC_ACTION_START(SCB2BTerminateOtherLegAction) {
  sc_sess->B2BterminateOtherLeg();
} EXEC_ACTION_END;

CONST_ACTION_2P(SCB2BReinviteAction,',', true);
EXEC_ACTION_START(SCB2BReinviteAction) {
  bool updateSDP = par1=="true";
  sess->sendReinvite(updateSDP, par2);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCB2BAddHeaderAction) {
  string val = resolveVars(arg, sess, sc_sess, event_params);
  DBG("adding B2B header '%s'\n", val.c_str());
  sc_sess->B2BaddHeader(val);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCB2BSetHeadersAction,',', true);
EXEC_ACTION_START(SCB2BSetHeadersAction) {
  string val = resolveVars(par1, sess, sc_sess, event_params);
  string repl = resolveVars(par2, sess, sc_sess, event_params);
  bool replace_crlf = false;
  if (repl == "true")
    replace_crlf = true;
  DBG("setting B2B headers to '%s' (%sreplacing CRLF)\n", 
      val.c_str(), replace_crlf?"":"not ");
  sc_sess->B2BsetHeaders(val, replace_crlf);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCB2BClearHeadersAction) {
  DBG("clearing B2B headers\n");
  sc_sess->B2BclearHeaders();
} EXEC_ACTION_END;

CONST_ACTION_2P(SCSendDTMFAction,',', true);
EXEC_ACTION_START(SCSendDTMFAction) {
  string event = resolveVars(par1, sess, sc_sess, event_params);
  string duration = resolveVars(par2, sess, sc_sess, event_params);  
  
  unsigned int event_i;
  if (str2i(event, event_i)) {
    ERROR("event '%s' not a valid DTMF event\n", event.c_str());
    throw DSMException("core", "cause", "invalid DTMF:"+ event);
  }

  unsigned int duration_i;
  if (duration.empty()) {
    duration_i = 500; // default
  } else {
    if (str2i(duration, duration_i)) {
      ERROR("event duration '%s' not a valid DTMF duration\n", duration.c_str());
      throw DSMException("core", "cause", "invalid DTMF duration:"+ duration);
    }
  }

  sess->sendDtmf(event_i, duration_i);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCSendDTMFSequenceAction,',', true);
EXEC_ACTION_START(SCSendDTMFSequenceAction) {
  string events = resolveVars(par1, sess, sc_sess, event_params);
  string duration = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int duration_i;
  if (duration.empty()) {
    duration_i = 500; // default
  } else {
    if (str2i(duration, duration_i)) {
      ERROR("event duration '%s' not a valid DTMF duration\n", duration.c_str());
      throw DSMException("core", "cause", "invalid DTMF duration:"+ duration);
    }
  }

  for (size_t i=0;i<events.length();i++) {
    if ((events[i]<'0' || events[i]>'9')
	&& (events[i] != '#') && (events[i] != '*')
	&& (events[i] <'A' || events[i] >'F')) {
	DBG("skipping non-DTMF event char '%c'\n", events[i]);
	continue;
    }
    int event = events[i] - '0';
    if (events[i] == '*')
      event = 10;
    else if (events[i] == '#')
      event = 11;
    else if (events[i] >= 'A' && events[i] <= 'F' )
      event = 12 + (events[i] - 'A');
    DBG("sending event %d duration %u\n", event, duration_i);
    sess->sendDtmf(event, duration_i);
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCRegisterEventQueueAction) {
  string q_name = resolveVars(arg, sess, sc_sess, event_params);
  DBG("Registering event queue '%s'\n", q_name.c_str());
  if (q_name.empty()) {
    WARN("Registering empty event queue name!\n");
  }
  AmEventDispatcher::instance()->addEventQueue(q_name, sess);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCUnregisterEventQueueAction) {
  string q_name = resolveVars(arg, sess, sc_sess, event_params);
  DBG("Unregistering event queue '%s'\n", q_name.c_str());
  if (q_name.empty()) {
    WARN("Unregistering empty event queue name!\n");
  }
  AmEventDispatcher::instance()->delEventQueue(q_name);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCCreateSystemDSMAction,',', false);
EXEC_ACTION_START(SCCreateSystemDSMAction) {
  string conf_name = resolveVars(par1, sess, sc_sess, event_params);
  string script_name = resolveVars(par2, sess, sc_sess, event_params);

  if (conf_name.empty() || script_name.empty()) {
    throw DSMException("core", "cause", "parameters missing - "
		       "need both conf_name and script_name for createSystemDSM");
  }

  DBG("creating system DSM conf_name %s, script_name %s\n", 
      conf_name.c_str(), script_name.c_str());
  string status;
  if (!DSMFactory::instance()->createSystemDSM(conf_name, script_name, false, status)) {
    ERROR("creating system DSM: %s\n", status.c_str());
    throw DSMException("core", "cause", status);
  }
  
} EXEC_ACTION_END;

