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

#include "DSMStateEngine.h"
#include "DSMModule.h"

#include "AmUtils.h"
#include "AmSession.h"
#include "log.h"

#include "AmSessionContainer.h"
#include "ampi/MonitoringAPI.h"

#include "DSM.h" // for DSMFactory::MonitoringFullCallgraph

DSMStateDiagram::DSMStateDiagram(const string& name) 
  : name(name) {
}

DSMStateDiagram::~DSMStateDiagram() {
}

void DSMStateDiagram::addState(const State& state, bool is_initial) {
  DBG("adding state '%s'\n", state.name.c_str());
  for (vector<DSMElement*>::const_iterator it=
	 state.pre_actions.begin(); it != state.pre_actions.end(); it++) {
    DBG("   pre-action '%s'\n", (*it)->name.c_str());
  }
  for (vector<DSMElement*>::const_iterator it=
	 state.post_actions.begin(); it != state.post_actions.end(); it++) {
    DBG("   post-action '%s'\n", (*it)->name.c_str());
  }

  states.push_back(state);
  if (is_initial) {
    if (!initial_state.empty()) {
      ERROR("trying to override initial state '%s' with '%s'\n",
	    initial_state.c_str(), state.name.c_str());
    } else {
      initial_state = state.name;
      DBG("set initial state '%s'\n", state.name.c_str());
    }
  }
}

bool DSMStateDiagram::addTransition(const DSMTransition& trans) {
  DBG("adding Transition '%s' %s -(...)-> %s\n",
      trans.name.c_str(), trans.from_state.c_str(), trans.to_state.c_str());
  for (vector<DSMCondition*>::const_iterator it=
	 trans.precond.begin(); it != trans.precond.end(); it++) {
    DBG("       DSMCondition  %s'%s'\n", 
	(*it)->invert?"not ":"", (*it)->name.c_str());
  }
  for (vector<DSMElement*>::const_iterator it=
	 trans.actions.begin(); it != trans.actions.end(); it++) {
    DBG("       Action     '%s'\n", (*it)->name.c_str());
  }

  vector<string> from_states;

  if (trans.from_state.find_first_of("(") != string::npos) {

    string states = trans.from_state;
    if (states.length() && states[0] == '(')
      states = states.substr(1);
    if (states.length() && states[states.length()-1] == ')')
      states = states.substr(0, states.length()-1);

    from_states = explode(states, ",");
    for (vector<string>::iterator it=from_states.begin(); 
	 it != from_states.end(); it++) {
      if (it->length() && (*it)[0] == ' ')
	*it = it->substr(1);
      if (it->length() && (*it)[it->length()-1] == ' ')
	*it = it->substr(0, it->length()-1);
    }

  } else  {
    from_states.push_back(trans.from_state);
  }

  for (vector<string>::iterator it=
	 from_states.begin(); it != from_states.end(); it++) {
    State* source_st = getState(*it);
    if (!source_st) {
      ERROR("state '%s' for transition '%s' not found\n",
	    it->c_str(), trans.name.c_str());
      return false;
    }
    
    source_st->transitions.push_back(trans);
  }

  return true;
}

State* DSMStateDiagram::getState(const string& s_name) {
  // find target state
  for (vector<State>::iterator target_st = 
	 states.begin(); target_st != states.end(); target_st++) {
    if (target_st->name == s_name) 
      return  &(*target_st);
  }

  return NULL;
}

State* DSMStateDiagram::getInitialState() {
  if (initial_state.empty()) {
    ERROR("diag '%s' doesn't have an initial state!\n",
	  name.c_str());
    return NULL;
  }
  return getState(initial_state);
}


bool DSMStateDiagram::checkConsistency(string& report) {
  bool res = true;
  DBG("checking consistency of '%s'\n", name.c_str());
  res &= checkInitialState(report);
  res &= checkDestinationStates(report);
  res &= checkHangupHandled(report);
  return res;
}

bool DSMStateDiagram::checkInitialState(string& report) {
  DBG("checking for initial state...\n");
  if (NULL == getInitialState()) {
    report+=name+": " "No initial state defined!\n";
    return false;
  }
  return true;
}
bool DSMStateDiagram::checkDestinationStates(string& report) {
  DBG("checking for existence of destination states...\n");
  bool res = true;
  for (vector<State>::iterator it=
	 states.begin(); it != states.end(); it++) {
    for (vector<DSMTransition>::iterator t_it=
	   it->transitions.begin(); t_it != it->transitions.end(); t_it++) {
      if (NULL == getState(t_it->to_state)) {
	report += name+": State '"+it->name+"' Transition '"+t_it->name+
	  "' : Destination state '"+ t_it->to_state +"' is not defined\n";
	res = false;
      }
    }
  }
  return res;
}

bool DSMStateDiagram::checkHangupHandled(string& report) {
  DBG("checking for hangup handled in all states...\n");
  bool res = true;
  for (vector<State>::iterator it=
	 states.begin(); it != states.end(); it++) {
    bool have_hangup_trans = false;
    for (vector<DSMTransition>::iterator t_it=
	   it->transitions.begin(); t_it != it->transitions.end(); t_it++) {
      for (vector<DSMCondition*>::iterator c_it=
	     t_it->precond.begin(); c_it!=t_it->precond.end(); c_it++) {
	if ((*c_it)->type == DSMCondition::Hangup) {
	  // todo: what if other conditions unmet?
	  have_hangup_trans = true;
	  break;
	}
      }
      if (have_hangup_trans)
	break;

    }
    if (!have_hangup_trans) {
      report += name+": State '"+it->name+"': hangup is not handled\n";
      res = false;
    }
  }

  return res;
}


DSMStateEngine::DSMStateEngine() 
  : current(NULL) {
}

DSMStateEngine::~DSMStateEngine() {
}

bool DSMStateEngine::onInvite(const AmSipRequest& req, DSMSession* sess) {
  bool res = true;
  for (vector<DSMModule*>::iterator it =
	 mods.begin(); it != mods.end(); it++)
    res &= (*it)->onInvite(req, sess);

  return res;
}
void DSMStateEngine::onBeforeDestroy(DSMSession* sc_sess, AmSession* sess) {
  for (vector<DSMModule*>::iterator it =
	 mods.begin(); it != mods.end(); it++)
    (*it)->onBeforeDestroy(sc_sess, sess);
}

void DSMStateEngine::processSdpOffer(AmSdp& offer) {
  for (vector<DSMModule*>::iterator it =
	 mods.begin(); it != mods.end(); it++)
    (*it)->processSdpOffer(offer);
}

void DSMStateEngine::processSdpAnswer(const AmSdp& offer, AmSdp& answer) {
  for (vector<DSMModule*>::iterator it =
	 mods.begin(); it != mods.end(); it++)
    (*it)->processSdpAnswer(offer, answer);
}

bool DSMStateEngine::runactions(vector<DSMElement*>::iterator from, 
				vector<DSMElement*>::iterator to, 
				AmSession* sess,  DSMSession* sc_sess, DSMCondition::EventType event,
				map<string,string>* event_params,  bool& is_consumed) {
  DBG("running %zd DSM action elements\n", to - from);
  for (vector<DSMElement*>::iterator it=from; it != to; it++) {

    DSMAction* dsm_act = dynamic_cast<DSMAction*>(*it);
    if (dsm_act) {
      DBG("executing '%s'\n", (dsm_act)->name.c_str()); 
      if ((dsm_act)->execute(sess, sc_sess, event, event_params)) {
        string se_modifier;
        switch ((dsm_act)->getSEAction(se_modifier,
				       sess, sc_sess, event, event_params)) {
	case DSMAction::Repost: 
	  is_consumed = false; 
	  break;
	case DSMAction::Jump: 
	  DBG("jumping to %s\n", se_modifier.c_str());
	  if (jumpDiag(se_modifier, sess, sc_sess, event, event_params)) {
	    // is_consumed = false; 
	    return true;  
	  }
	  break;
	case DSMAction::Call:
	  DBG("calling %s\n", se_modifier.c_str());
	  if (callDiag(se_modifier, sess, sc_sess, event, event_params))  {
	    // is_consumed = false; 
	    return true;   
	  } 
	  break;
	case DSMAction::Return: 
	  if (returnDiag(sess, sc_sess)) {
	    //is_consumed = false;
	    return true; 
	  }
	  break;
	default: break;
	}
      }
      continue;
    }

    DSMConditionTree* cond_tree = dynamic_cast<DSMConditionTree*>(*it);
    if (cond_tree) {
      DBG("checking conditions\n");
      vector<DSMCondition*>::iterator con=cond_tree->conditions.begin();
      while (con!=cond_tree->conditions.end()) {
	if (!(*con)->_match(sess, sc_sess, event, event_params))
	  break;
	con++;
      }
      if (con == cond_tree->conditions.end()) {
	DBG("condition tree matched.\n");
        if (runactions(cond_tree->run_if_true.begin(), cond_tree->run_if_true.end(),
		       sess, sc_sess, event, event_params, is_consumed))
	  return true;
      } else {
        if(runactions(cond_tree->run_if_false.begin(), cond_tree->run_if_false.end(),
		      sess, sc_sess, event, event_params, is_consumed))
	  return true;
      }
      continue;
    }

    DSMArrayFor* array_for = dynamic_cast<DSMArrayFor*>(*it);
    if (array_for) {
      if (array_for->for_type == DSMArrayFor::Range) {
	DBG("running for (%s in range(%s, %s) {\n",
	    array_for->k.c_str(), array_for->v.c_str(), array_for->array_struct.c_str());
      } else {
	DBG("running for (%s%s in %s) {\n",
	    array_for->k.c_str(), array_for->v.empty() ? "" : (","+array_for->v).c_str(),
	    array_for->array_struct.c_str());
      }

      string array_name = array_for->array_struct;
      string k_name = array_for->k;
      string v_name = array_for->v;

      if (!k_name.length()) {
	ERROR("empty counter name in for\n");
	continue;
      }

      if (array_for->for_type != DSMArrayFor::Range && !array_name.length()) {
	ERROR("empty array name in for\n");
	continue;
      }

      if (array_name[0] == '$')	array_name.erase(0, 1);
      if (k_name[0] == '$') k_name.erase(0, 1);

      if (array_for->for_type == DSMArrayFor::Struct && v_name[0] == '$')
	v_name.erase(0, 1);

      vector<pair<string, string> > cnt_values;
      int range[2] = {0,0};
      // get the counter values
      if (array_for->for_type == DSMArrayFor::Struct) {
	VarMapT::iterator lb = sc_sess->var.lower_bound(array_name);
	while (lb != sc_sess->var.end()) {
	  if ((lb->first.length() < array_name.length()) ||
	      strncmp(lb->first.c_str(), array_name.c_str(), array_name.length()))
	    break;

	  string varname = lb->first.substr(array_name.length()+1);
	  string valname = lb->second;
	  cnt_values.push_back(make_pair(varname, valname));
	  DBG("      '%s,%s'\n", varname.c_str(), valname.c_str());
	  lb++;
	}
      } else if (array_for->for_type == DSMArrayFor::Array) {
	unsigned int a_index = 0;
	VarMapT::iterator v = sc_sess->
	  var.lower_bound(array_name+"["+int2str(a_index)+"]");

	while (v != sc_sess->var.end()) {
	  string this_index = array_name+"["+int2str(a_index)+"]";
	  if (v->first.substr(0, this_index.length()) != this_index) {
	    a_index++;
	    this_index = array_name+"["+int2str(a_index)+"]";
	    if (v->first.substr(0, this_index.length()) != this_index) {
	      break;
	    }
	  }

	  cnt_values.push_back(make_pair(v->second, ""));
	  DBG("      '%s'\n", v->second.c_str());
	  v++;
	}
      } else if (array_for->for_type == DSMArrayFor::Range) {
	string s_range = resolveVars(array_for->v, sess, sc_sess, event_params);
	if (!str2int(s_range, range[0])) {
	  WARN("Error converting lower bound range(%s,%s)\n",
	       array_for->v.c_str(), array_for->array_struct.c_str());
	  range[0]=0;
	} else {
	  s_range = resolveVars(array_for->array_struct, sess, sc_sess, event_params);

	  if (!str2int(s_range, range[1])) {
	    WARN("Error converting upper bound range(%s,%s)\n",
		 array_for->v.c_str(), array_for->array_struct.c_str());
	    range[0]=0; range[1]=0;
	  }
	}
      }

      // save counter k
      VarMapT::iterator c_it = sc_sess->var.find(k_name);
      bool k_exists = c_it != sc_sess->var.end();
      string k_save = k_exists ? c_it->second : string("");

      // save counter v for Struct
      bool v_exists = false; string v_save;
      if (array_for->for_type == DSMArrayFor::Struct) {
	c_it = sc_sess->var.find(v_name);
	v_exists = c_it != sc_sess->var.end();
	if (v_exists)
	  v_save = c_it->second;
      }

      // run the loop
      if (array_for->for_type == DSMArrayFor::Range) {
	int cnt = range[0];
	while (cnt != range[1]) {
	  sc_sess->var[k_name] = int2str(cnt);
	  DBG("setting $%s=%s\n", k_name.c_str(), sc_sess->var[k_name].c_str());

	  runactions(array_for->actions.begin(), array_for->actions.end(),
		     sess, sc_sess, event, event_params, is_consumed);

	  if (range[1] > range[0])
	    cnt++;
	  else
	    cnt--;
	}

      } else {
	DBG("running for loop with %zd items\n", cnt_values.size());
	for (vector<pair<string, string> > ::iterator f_it=
	       cnt_values.begin(); f_it != cnt_values.end(); f_it++) {
	  if (array_for->for_type == DSMArrayFor::Struct) {
	    DBG("setting $%s=%s, $%s=%s\n", k_name.c_str(), f_it->first.c_str(),
		v_name.c_str(), f_it->second.c_str());
	    sc_sess->var[k_name] = f_it->first;
	    sc_sess->var[v_name] = f_it->second;
	  } else {
	    DBG("setting $%s=%s\n", k_name.c_str(), f_it->first.c_str());
	    sc_sess->var[k_name] = f_it->first;
	  }
	  runactions(array_for->actions.begin(), array_for->actions.end(),
		     sess, sc_sess, event, event_params, is_consumed);
	}
      }

      // restore the counter[s]
      if (k_exists)
	sc_sess->var[k_name] = k_save;
      else
	sc_sess->var.erase(k_name);

      if (array_for->for_type == DSMArrayFor::Struct) {
	if (v_exists)
	  sc_sess->var[v_name] = v_save;
	else
	  sc_sess->var.erase(v_name);
      }

      continue;
    }

    ERROR("DSMElement typenot understood\n");
  }
  return false;
} 

void DSMStateEngine::addDiagram(DSMStateDiagram* diag) {
  diags.push_back(diag);
}

void DSMStateEngine::addModules(vector<DSMModule*> modules) {
  for (vector<DSMModule*>::iterator it=
	 modules.begin(); it != modules.end(); it++)
    mods.push_back(*it);
}

bool DSMStateEngine::init(AmSession* sess, DSMSession* sc_sess,
			  const string& startDiagram,
			  DSMCondition::EventType init_event) {

  try {
    if (!jumpDiag(startDiagram, sess, sc_sess, init_event, NULL)) {
      ERROR("initializing with start diag '%s'\n",
	    startDiagram.c_str());
      return false;
    }
  } catch (DSMException& e) {
    DBG("Exception type '%s' occured while initializing! Run init event as exception...\n", 
	e.params["type"].c_str());
    runEvent(sess, sc_sess, init_event, &e.params, true);
    return true;
    
  }

  DBG("run init event...\n");
  runEvent(sess, sc_sess, init_event, NULL);
  return true;
}

bool DSMCondition::_match(AmSession* sess, DSMSession* sc_sess,
			  DSMCondition::EventType event,
			  map<string,string>* event_params) {
  // or xor
  return invert? (!match(sess,sc_sess,event,event_params)) : match(sess, sc_sess, event, event_params);
}

bool DSMCondition::match(AmSession* sess, DSMSession* sc_sess,
			 DSMCondition::EventType event,
			 map<string,string>* event_params) {

  if ((type != Any) && (event != type))
    return false;

  if (!event_params)
    return true;

  for (map<string,string>::iterator it=params.begin();
       it!=params.end(); it++) {
    map<string,string>::iterator val = event_params->find(it->first);
    if (val == event_params->end() || val->second != it->second)
      return false;
  }

  DBG("condition matched: '%s'\n", name.c_str());
  return true;
}

void DSMStateEngine::runEvent(AmSession* sess, DSMSession* sc_sess,
			      DSMCondition::EventType event,
			      map<string,string>* event_params,
			      bool run_exception) {
  if (!current || !current_diag)
    return;
  
  DSMCondition::EventType active_event = event;
  map<string,string>* active_params = event_params;
  map<string,string> exception_params;
  bool is_exception = run_exception;

  bool is_consumed = true;
  do {
    try {
      is_consumed = true;

      for (vector<DSMTransition>::iterator tr = current->transitions.begin();
	   tr != current->transitions.end();tr++) {
	if (tr->is_exception != is_exception)
	  continue;
	
	DBG("checking transition '%s'\n", tr->name.c_str());
	
	vector<DSMCondition*>::iterator con=tr->precond.begin();
	while (con!=tr->precond.end()) {
	  if (!(*con)->_match(sess, sc_sess, active_event, active_params))
	    break;
	  con++;
	}
	if (con == tr->precond.end()) {
	  DBG("transition '%s' matched.\n", tr->name.c_str());
	  
	  //  matched all preconditions
	  // find target state
	  State* target_st = current_diag->getState(tr->to_state);
	  if (!target_st) {
	    ERROR("script writer error: transition '%s' from "
		  "state '%s' to unknown state '%s'\n",
		  tr->name.c_str(),
		  current->name.c_str(),
		  tr->to_state.c_str());
	  }
	  
	  // run post-actions
	  if (current->post_actions.size()) {
	    DBG("running %zd post_actions of state '%s'\n",
		current->post_actions.size(), current->name.c_str());
	    if (runactions(current->post_actions.begin(), 
			   current->post_actions.end(), 
			   sess, sc_sess, active_event, active_params, is_consumed)) {
	      break;
	    }
	  }
	  
	  // run transition actions
	  if (tr->actions.size()) {
	    DBG("running %zd actions of transition '%s'\n",
		tr->actions.size(), tr->name.c_str());
	    if (runactions(tr->actions.begin(), 
			   tr->actions.end(), 
			   sess, sc_sess, active_event, active_params, is_consumed)) {
	      break;
	    }
	  }
	  
	  // go into new state
	  if (!target_st) {
	    break;
	  }
	  DBG("changing to new state '%s'\n", target_st->name.c_str());
	  
#ifdef USE_MONITORING
	  MONITORING_LOG(sess->getLocalTag().c_str(), "dsm_state", target_st->name.c_str());
	  
	  if (DSMFactory::MonitoringFullTransitions) {
	    MONITORING_LOG_ADD(sess->getLocalTag().c_str(), 
			       "dsm_stategraph", 
			       ("> "+ tr->name + " >").c_str());
	  }
	  
	  if (DSMFactory::MonitoringFullCallgraph) {
	    MONITORING_LOG_ADD(sess->getLocalTag().c_str(), 
			       "dsm_stategraph", 
			       (current_diag->getName() +"/"+ target_st->name).c_str());
	  }
#endif
	  
	  current = target_st;
	  
	  // execute pre-actions
	  if (current->pre_actions.size()) {
	    DBG("running %zd pre_actions of state '%s'\n",
		current->pre_actions.size(), current->name.c_str());
	    if (runactions(current->pre_actions.begin(), 
			   current->pre_actions.end(), 
			   sess, sc_sess, active_event, active_params, is_consumed)) {
	      break;
	    }
	  }
	  
	  break;
	}
      }
    } catch (DSMException& e) {
      DBG("DSMException occured, type = %s\n", e.params["type"].c_str());
      is_consumed = false;

      is_exception = true; // continue to process as exception event
      exception_params = e.params;
      active_params = &exception_params;
      active_event = DSMCondition::DSMException;
    }

  } while (!is_consumed);
}

bool DSMStateEngine::callDiag(const string& diag_name, AmSession* sess, DSMSession* sc_sess,
			   DSMCondition::EventType event,
			   map<string,string>* event_params) {
  if (!current || !current_diag) {
    ERROR("no current diag to push\n");
    return false;
  }
  stack.push_back(std::make_pair(current_diag, current));
  return jumpDiag(diag_name, sess, sc_sess, event, event_params);
}

bool DSMStateEngine::jumpDiag(const string& diag_name, AmSession* sess, DSMSession* sc_sess,
			   DSMCondition::EventType event,
			   map<string,string>* event_params) {
  for (vector<DSMStateDiagram*>::iterator it=
	 diags.begin(); it != diags.end(); it++) {
    if ((*it)->getName() == diag_name) {
      current_diag = *it;
      current = current_diag->getInitialState();
      if (!current) {
	ERROR("diag '%s' does not have initial state.\n",  
	      diag_name.c_str());
	return false;
      }

      MONITORING_LOG2(sess->getLocalTag().c_str(), 
		      "dsm_diag", diag_name.c_str(),
		      "dsm_state", current->name.c_str());

#ifdef USE_MONITORING
      if (DSMFactory::MonitoringFullCallgraph) {
	MONITORING_LOG_ADD(sess->getLocalTag().c_str(), 
			    "dsm_stategraph", 
			   (diag_name +"/"+ current->name).c_str());
      }
#endif

      // execute pre-actions
      DBG("running %zd pre_actions of init state '%s'\n",
	  current->pre_actions.size(), current->name.c_str());
      
      bool is_finished;
      is_finished = true;
      runactions(current->pre_actions.begin(), 
		 current->pre_actions.end(), 
		 sess, sc_sess, event, event_params, is_finished); 

      return true;
    }
  }
  ERROR("diag '%s' not found.\n",  diag_name.c_str());
  return false;
}

bool DSMStateEngine::returnDiag(AmSession* sess, DSMSession* sc_sess) {
  if (stack.empty()) {
    ERROR("returning from empty stack\n");
    return false;
  }
  current_diag = stack.back().first;
  current = stack.back().second;
  stack.pop_back();

  MONITORING_LOG2(sess->getLocalTag().c_str(), 
		  "dsm_diag", current_diag->getName().c_str(),
		  "dsm_state", current->name.c_str());

#ifdef USE_MONITORING
      if (DSMFactory::MonitoringFullCallgraph) {
	MONITORING_LOG_ADD(sess->getLocalTag().c_str(), 
			    "dsm_stategraph", 
			   (current_diag->getName() +"/"+ current->name).c_str());
      }
#endif

  DBG("returned to diag '%s' state '%s'\n",
      current_diag->getName().c_str(), 
      current->name.c_str());

  return true;
}

State::State() {
}

State::~State() {
}

DSMTransition::DSMTransition()
  : is_exception(false)
{
}

DSMTransition::~DSMTransition(){
}




void varPrintArg(const AmArg& a, map<string, string>& dst, const string& name) {
  switch (a.getType()) {
  case AmArg::Undef: dst[name] =  "null"; return;
  case AmArg::Int: dst[name] =  a.asInt()<0 ? 
      "-"+int2str(abs(a.asInt())):int2str(abs(a.asInt())); return;
  case AmArg::Bool:
     dst[name] = a.asBool()?"true":"false"; return;     
  case AmArg::Double:
    dst[name] = double2str(a.asDouble()); return;
  case AmArg::CStr:
    dst[name] = a.asCStr(); return;
  case AmArg::Array:
    for (size_t i = 0; i < a.size(); i ++)
      varPrintArg(a.get(i), dst, name+"["+int2str((unsigned int)i)+"]");
    return;
  case AmArg::Struct:
    for (AmArg::ValueStruct::const_iterator it = a.asStruct()->begin();
	 it != a.asStruct()->end(); it ++) {
      varPrintArg(it->second, dst, name+"."+it->first);
    }
    return;
  default: dst[name] = "<UNKONWN TYPE>"; return;
  }
}

