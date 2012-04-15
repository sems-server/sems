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
#ifndef _STATE_ENGINE_H
#define _STATE_ENGINE_H

#include "DSMElemContainer.h"
#include "AmSipMsg.h"
#include "AmArg.h"
#include "AmSdp.h"

class AmSession;
class DSMSession;

#include <map>
using std::map;
#include <vector>
using std::vector;
#include <string>
using std::string;

using std::pair;

#include "log.h"

class DSMElement {
 public: 
  DSMElement() { }
  virtual ~DSMElement() { }
  string name; // documentary only

};

class DSMCondition
  : public DSMElement {
 public:
  enum EventType {
    Any,

    Start,
    Invite,
    SessionStart,
    Ringing,
    EarlySession,
    FailedCall,
    SipRequest,
    SipReply,

    Hangup,
    Hold,
    UnHold,

    B2BOtherReply,
    B2BOtherBye,

    SessionTimeout,
    RtpTimeout,
    RemoteDisappeared,

    Key,
    Timer,

    NoAudio,
    PlaylistSeparator,

    DSMEvent,    
    DSMException,

    XmlrpcResponse,

    JsonRpcResponse,
    JsonRpcRequest,

    Startup,
    Reload,
    System,

    SIPSubscription,

    RTPTimeout
  };

  bool invert; 
  
  DSMCondition() : invert(false) { }
  virtual ~DSMCondition() { }

  EventType type;
  map<string, string> params;

  bool _match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
	      map<string,string>* event_params);

  virtual bool match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
		     map<string,string>* event_params);
};

class DSMAction 
: public DSMElement {
 public:
  /** modifies State Engine operation */
  enum SEAction {
    None,   // no modification
    Repost, // repost current event
    Jump,   // jump FSM
    Call,   // call FSM
    Return  // return from FSM call 
  };

  DSMAction() { /* DBG("const action\n"); */ }
  virtual ~DSMAction() { /* DBG("dest action\n"); */ }

  /** @return whether state engine is to be modified (via getSEAction) */
  virtual bool execute(AmSession* sess, DSMSession* sc_sess, 
		       DSMCondition::EventType event,
		       map<string,string>* event_params) = 0;

  /** @return state engine modification */
  virtual SEAction getSEAction(string& param,
			       AmSession* sess, DSMSession* sc_sess,
			       DSMCondition::EventType event,
			       map<string,string>* event_params) { return None; }
};

class DSMTransition;

class State
: public DSMElement {
 public:
  State();
  ~State();
  vector<DSMElement*> pre_actions;
  vector<DSMElement*> post_actions;
  
  vector<DSMTransition> transitions;
};

class DSMTransition
: public DSMElement {
 public:
  DSMTransition();
  ~DSMTransition();

  vector<DSMCondition*> precond;
  vector<DSMElement*> actions;
  string from_state;
  string to_state;

  bool is_exception;
};

class DSMConditionTree
: public DSMElement {
 public:
  vector<DSMCondition*> conditions;
  vector<DSMElement*> run_if_true;
  vector<DSMElement*> run_if_false;
  bool is_exception;
};

class DSMFunction
: public DSMElement {
 public:
  string name;
  vector<DSMElement*> actions;
};

class DSMArrayFor
: public DSMElement {
 public:
  DSMArrayFor() { }
  ~DSMArrayFor() { }

  enum DSMForType {
    Range,
    Array,
    Struct
  } for_type;

  string k; // for(k in array)
  string v; // for(k,v in struct), or range lower bound
  string array_struct; // array or struct name, or range upper bound
  vector<DSMElement*> actions;
};

class DSMModule;

class DSMStateDiagram  {
  vector<State> states;
  string name;
  string initial_state;

  bool checkInitialState(string& report);
  bool checkDestinationStates(string& report);
  bool checkHangupHandled(string& report);

 public:
  DSMStateDiagram(const string& name);
  ~DSMStateDiagram();

  State* getInitialState();
  State* getState(const string& s_name);

  void addState(const State& state, bool is_initial = false);
  bool addTransition(const DSMTransition& trans);
  const string& getName() { return name; }
  bool checkConsistency(string& report);
};

class DSMException {
 public:
  DSMException(const string& e_type) 
    { params["type"] = e_type; }

  DSMException(const string& e_type, 
	       const string& key1, const string& val1) 
    { params["type"] = e_type; 
      params[key1] = val1; }

  DSMException(const string& e_type, 
	       const string& key1, const string& val1,
	       const string& key2, const string& val2) 
    { params["type"] = e_type; 
      params[key1] = val1; 
      params[key2] = val2; }

  DSMException(map<string, string>& params)
    : params(params) { }

  ~DSMException() { }

  map<string, string> params;    
};

class DSMStateEngine {
  State* current;
  DSMStateDiagram* current_diag;
  vector<DSMStateDiagram*> diags;

  vector<pair<DSMStateDiagram*, State*> > stack;

  bool callDiag(const string& diag_name, AmSession* sess, DSMSession* sc_sess, 
		DSMCondition::EventType event,
		map<string,string>* event_params);
  bool jumpDiag(const string& diag_name, AmSession* sess, DSMSession* sc_sess,
		DSMCondition::EventType event,
		map<string,string>* event_params);
  bool returnDiag(AmSession* sess, DSMSession* sc_sess);
  bool runactions(vector<DSMElement*>::iterator from, 
		  vector<DSMElement*>::iterator to, 
		  AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
		  map<string,string>* event_params,  bool& is_consumed);

  vector<DSMModule*> mods;

 public: 
  DSMStateEngine();
  ~DSMStateEngine();

  void addDiagram(DSMStateDiagram* diag); 
  void addModules(vector<DSMModule*> modules);

  bool init(AmSession* sess, DSMSession* sc_sess,
	    const string& startDiagram,
	    DSMCondition::EventType init_event);

  void runEvent(AmSession* sess, DSMSession* sc_sess,
		DSMCondition::EventType event,
		map<string,string>* event_params,
		bool run_exception = false);

  /** @return whether call should be accepted */
  bool onInvite(const AmSipRequest& req, DSMSession* sess);
  void onBeforeDestroy(DSMSession* sc_sess, AmSession* sess);

  void processSdpOffer(AmSdp& offer);
  void processSdpAnswer(const AmSdp& offer, AmSdp& answer);
};

extern void varPrintArg(const AmArg& a, map<string, string>& dst, const string& name);

#endif
