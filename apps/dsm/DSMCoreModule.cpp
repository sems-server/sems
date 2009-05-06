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

#include "DSMCoreModule.h"
#include "DSMSession.h"
#include "AmSession.h"
#include "AmSessionContainer.h"
#include "AmUtils.h"

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

  DEF_CMD("stop", SCStopAction);

  DEF_CMD("playPrompt", SCPlayPromptAction);
  DEF_CMD("playPromptLooped", SCPlayPromptLoopedAction);
  DEF_CMD("playFile", SCPlayFileAction);
  DEF_CMD("playFileFront", SCPlayFileFrontAction);
  DEF_CMD("recordFile", SCRecordFileAction);
  DEF_CMD("stopRecord", SCStopRecordAction);
  DEF_CMD("getRecordLength", SCGetRecordLengthAction);
  DEF_CMD("getRecordDataSize", SCGetRecordDataSizeAction);
  DEF_CMD("closePlaylist", SCClosePlaylistAction);
  DEF_CMD("addSeparator", SCAddSeparatorAction);
  DEF_CMD("connectMedia", SCConnectMediaAction);
  DEF_CMD("disconnectMedia", SCDisconnectMediaAction);
  DEF_CMD("mute", SCMuteAction);
  DEF_CMD("unmute", SCUnmuteAction);
  DEF_CMD("enableDTMFDetection", SCEnableDTMFDetection);
  DEF_CMD("disableDTMFDetection", SCDisableDTMFDetection);

  DEF_CMD("set", SCSetAction);
  DEF_CMD("append", SCAppendAction);
  DEF_CMD("substr", SCSubStrAction);
  DEF_CMD("inc", SCIncAction);
  DEF_CMD("log", SCLogAction);
  DEF_CMD("clear", SCClearAction);
  DEF_CMD("logVars", SCLogVarsAction);

  DEF_CMD("setTimer", SCSetTimerAction);

  DEF_CMD("setPrompts", SCSetPromptsAction);

  DEF_CMD("postEvent", SCPostEventAction);

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

  if (cmd == "keyTest") 
    return new TestDSMCondition(params, DSMCondition::Key);

  if (cmd == "timerTest") 
    return new TestDSMCondition(params, DSMCondition::Timer);

  if (cmd == "noAudioTest") 
    return new TestDSMCondition(params, DSMCondition::NoAudio);

  if (cmd == "separatorTest") 
    return new TestDSMCondition(params, DSMCondition::PlaylistSeparator);

  if (cmd == "hangup") 
    return new TestDSMCondition(params, DSMCondition::Hangup);  

  if (cmd == "eventTest") 
    return new TestDSMCondition(params, DSMCondition::DSMEvent);  

  if (cmd == "invite") 
    return new TestDSMCondition(params, DSMCondition::Invite);  

  if (cmd == "sessionStart") 
    return new TestDSMCondition(params, DSMCondition::SessionStart);  

  if (cmd == "B2B.otherReply") 
    return new TestDSMCondition(params, DSMCondition::B2BOtherReply);  

  if (cmd == "B2B.otherBye") 
    return new TestDSMCondition(params, DSMCondition::B2BOtherBye);  

  return NULL;
}

EXEC_ACTION_START(SCPlayPromptAction) {
  sc_sess->playPrompt(resolveVars(arg, sess, sc_sess, event_params));
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
    else 
      ev->params[var] = sc_sess->var[var];
  }

  DBG("posting event to session '%s'\n", sess_id.c_str());
  if (!AmSessionContainer::instance()->postEvent(sess_id, ev))
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
  else 
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);
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

EXEC_ACTION_START(SCClosePlaylistAction) {
  bool notify = 
    resolveVars(arg, sess, sc_sess, event_params) == "true";
  sc_sess->closePlaylist(notify);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCConnectMediaAction) {
  sc_sess->connectMedia();
} EXEC_ACTION_END;

EXEC_ACTION_START(SCDisconnectMediaAction) {
  sc_sess->disconnectMedia();
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

EXEC_ACTION_START(SCStopAction) {
  if (resolveVars(arg, sess, sc_sess, event_params) == "true") {
    DBG("sending bye\n");
    sess->dlg.bye();
  }
  sess->setStopped();
} EXEC_ACTION_END;

#define DEF_SCModActionExec(clsname)				\
								\
  bool clsname::execute(AmSession* sess,			\
			DSMCondition::EventType event,		\
			map<string,string>* event_params) {	\
    return true;						\
  }								\

DEF_SCModActionExec(SCRepostAction);
DSMAction::SEAction SCRepostAction::getSEAction(string& param) { 
  return Repost; 
}

DEF_SCModActionExec(SCJumpFSMAction);
DSMAction::SEAction SCJumpFSMAction::getSEAction(string& param) { 
  param = arg;
  return Jump; 
}

DEF_SCModActionExec(SCCallFSMAction);
DSMAction::SEAction SCCallFSMAction::getSEAction(string& param) { 
  param = arg;
  return Call; 
}

DEF_SCModActionExec(SCReturnFSMAction);
DSMAction::SEAction SCReturnFSMAction::getSEAction(string& param) { 
  return Return; 
}

#undef DEF_SCModActionExec

CONST_ACTION_2P(SCLogAction, ',', false);
EXEC_ACTION_START(SCLogAction) {

  unsigned int lvl;
  if (str2i(resolveVars(par1, sess, sc_sess, event_params), lvl)) {
    ERROR("unknown log level '%s'\n", par1.c_str());
    return false;
  }
  string l_line = resolveVars(par2, sess, sc_sess, event_params).c_str();
  _LOG((int)lvl, "FSM: %s '%s'\n", (par2 != l_line)?par2.c_str():"",
       l_line.c_str());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCLogVarsAction) {
  unsigned int lvl;
  if (str2i(resolveVars(arg, sess, sc_sess, event_params), lvl)) {
    ERROR("unknown log level '%s'\n", arg.c_str());
    return false;
  }

  _LOG((int)lvl, "FSM: variables set ---\n");
  for (map<string, string>::iterator it = 
	 sc_sess->var.begin(); it != sc_sess->var.end(); it++) {
    _LOG((int)lvl, "FSM:  $%s='%s'\n", it->first.c_str(), it->second.c_str());
  }
  _LOG((int)lvl, "FSM: variables end ---\n");
} EXEC_ACTION_END;

CONST_ACTION_2P(SCSetAction,'=', false);
EXEC_ACTION_START(SCSetAction) {
  string var_name = (par1.length() && par1[0] == '$')?
    par1.substr(1) : par1;

  sc_sess->var[var_name] = resolveVars(par2, sess, sc_sess, event_params);
  DBG("set variable '%s'='%s'\n", 
      var_name.c_str(), sc_sess->var[var_name].c_str());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCClearAction) {
  string var_name = (arg.length() && arg[0] == '$')?
    arg.substr(1) : arg;
  DBG("clear variable '%s'\n", var_name.c_str());
  sc_sess->var.erase(var_name);
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
  if (str2i(resolveVars(par2, sess, sc_sess, event_params), pos)) {
    ERROR("substr length '%s'\n",
	  resolveVars(par2, sess, sc_sess, event_params).c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return false;
  }
  try {
    sc_sess->var[var_name] = sc_sess->var[var_name].substr(pos);
  } catch(...) {
    ERROR("in substr\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
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
    return false;
  }

  unsigned int timeout;
  if (str2i(resolveVars(par2, sess, sc_sess, event_params), timeout)) {
    ERROR("timeout value '%s' not decipherable\n", 
	  resolveVars(par2, sess, sc_sess, event_params).c_str());
    return false;
  }

  DBG("setting timer %u with timeout %u\n", timerid, timeout);
  AmDynInvokeFactory* user_timer_fact = 
    AmPlugIn::instance()->getFactory4Di("user_timer");

  if(!user_timer_fact) {
    ERROR("load sess_timer module for timers.\n");
    return false;
  }
  AmDynInvoke* user_timer = user_timer_fact->getInstance();
  if(!user_timer) {
    ERROR("load sess_timer module for timers.\n");
    return false;
  }

  AmArg di_args,ret;
  di_args.push((int)timerid);
  di_args.push((int)timeout);      // in seconds
  di_args.push(sess->getLocalTag().c_str());
  user_timer->invoke("setTimer", di_args, ret);

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

bool TestDSMCondition::match(AmSession* sess, DSMCondition::EventType event,
			  map<string,string>* event_params) {
  if (ttype == None || (type != DSMCondition::Any && type != event))
    return false;

  if (ttype == Always)
    return true;

  DSMSession* sc_sess = dynamic_cast<DSMSession*>(sess);
  if (!sc_sess) {
    ERROR("wrong session type\n");
    return false;
  }
  
  string l;
  string r;
  if (lhs.length() > 5 && 
      (lhs.substr(0, 4) == "len(") && lhs[lhs.length()-1] == ')') {
    l = int2str(resolveVars(lhs.substr(4, lhs.length()-5), sess, sc_sess, event_params).length());
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
  case Eq: return l == r;
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
  params = explode(arg,",");
  if (params.size()<2) {
    ERROR("DI needs at least: mod_name, "
	  "function_name\n");
    return;
  }
}

EXEC_ACTION_START(SCDIAction) {

  if (params.size() < 2) {
    ERROR("DI needs at least: mod_name, "
	  "function_name (in '%s'\n", name.c_str());
    return false;    
  }

  vector<string>::iterator p_it=params.begin();
  string fact_name = trim(*p_it, " \"");
  AmDynInvokeFactory* fact = 
    AmPlugIn::instance()->getFactory4Di(fact_name);

  if(!fact) {
    ERROR("load module for factory '%s'.\n", fact_name.c_str());
    return false;
  }
  AmDynInvoke* di_inst = fact->getInstance();
  if(!di_inst) {
    ERROR("load module for factory '%s'\n", fact_name.c_str());
    return false;
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
	return false;
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
    return false;
  } catch (const AmArg::OutOfBoundsException& oob) {
    ERROR("out of bounds in  DI call '%s'\n", 
	  name.c_str());
    return false;
  } catch (const AmArg::TypeMismatchException& oob) {
    ERROR("type mismatch  in  DI call '%s'\n", 
	  name.c_str());
    return false;
  } catch (...) {
    ERROR("unexpected Exception  in  DI call '%s'\n", 
	  name.c_str());
    return false;
  }

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
	  sc_sess->var["DI_res"+int2str(i)] = 
	    sc_sess->di_res.get(i).asCStr();
	} break;
	case AmArg::Int: {
	  sc_sess->var["DI_res"+int2str(i)] = 
	    int2str(sc_sess->di_res.get(i).asInt());
	} break;
	default: {
	  ERROR("unsupported AmArg return type!");
	}
	}
      }
    } else {
      ERROR("unsupported AmArg return type!");
    }
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
