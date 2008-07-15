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
#include "DSMChartReader.h"
#include "log.h"

#include <dlfcn.h> // dlopen & friends

#include <vector>
using std::vector;

DSMChartReader::DSMChartReader() {
}

DSMChartReader::~DSMChartReader() {
}

bool DSMChartReader::is_wsp(const char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool DSMChartReader::is_snt(const char c) {
  return c== ';' || c == '{' || c == '}';
}

string DSMChartReader::getToken(string str, size_t& pos) {
  while (pos<str.length() && is_wsp(str[pos]))
    pos++;

  size_t pos1 = pos;

  if (is_snt(str[pos])) {
    string res = " "; 
    res[0] = str[pos];
    pos++;
    return res;
  }
  char last_chr = ' ';
  while (pos1<str.length() && !is_wsp(str[pos1]) && !is_snt(str[pos1])) {
    if (str[pos1] == '"') {
      pos1++;
      while (pos1<str.length() && !((str[pos1] == '"') && (last_chr != '\\'))) {
	last_chr = str[pos1];
	pos1++;
      }
    }
    if (str[pos1] == '(') {
      int lvl = 0;
      pos1++;
      while (pos1<str.length() && (lvl || (str[pos1] != ')'))) {

	if (str[pos1] == '(')
	  lvl++;
	else if (str[pos1] == ')')
	  lvl--;
	    
	if (str[pos1] == '"') {
	  pos1++;
	  while (pos1<str.length() && !((str[pos1] == '"') && (last_chr != '\\'))) {
	    last_chr = str[pos1];
	    pos1++;
	  }
	}
	last_chr = str[pos1];
	pos1++;
      }
    }

    pos1++;
  }

  string res;
  if (str[pos] == '"')
    res = str.substr(pos+1, pos1-pos-2);
  else 
    res = str.substr(pos, pos1-pos);

  pos = pos1;
  return res;
}

DSMAction* DSMChartReader::actionFromToken(const string& str) {
  for (vector<DSMModule*>::iterator it=
	 mods.begin(); it!= mods.end(); it++) {
    DSMAction* a = (*it)->getAction(str);
    if (a) return a;
  }

  return core_mod.getAction(str);
}

DSMCondition* DSMChartReader::conditionFromToken(const string& str) {
  for (vector<DSMModule*>::iterator it=
	 mods.begin(); it!= mods.end(); it++) {
    DSMCondition* c=(*it)->getCondition(str);
    if (c) return c;
  }
  return core_mod.getCondition(str);
}

void splitCmd(const string& from_str, 
	      string& cmd, string& params) {
  size_t b_pos = from_str.find('(');
  if (b_pos != string::npos) {
    cmd = from_str.substr(0, b_pos);
    params = from_str.substr(b_pos + 1, from_str.rfind(')') - b_pos -1);
  } else 
    cmd = from_str;  
}

bool DSMChartReader::importModule(const string& mod_cmd, const string& mod_path) {
  string cmd;
  string params;
  
  splitCmd(mod_cmd, cmd, params);
  if (!params.length()) {
    ERROR("import needs module name\n");
    return false;
  }

  string fname = mod_path;
  if (fname.length() &&fname[fname.length()-1]!= '/')
    fname+='/';
  fname += params + ".so";

  void* h_dl = dlopen(fname.c_str(),RTLD_NOW | RTLD_GLOBAL);
  if(!h_dl){
    ERROR("import module: %s: %s\n",fname.c_str(),dlerror());
    return false;
  }

  SCFactoryCreate fc = NULL;
  if ((fc = (SCFactoryCreate)dlsym(h_dl,SC_FACTORY_EXPORT_STR)) == NULL) {
    ERROR("invalid SC module '%s'\n", fname.c_str());
    return false;
  }
   
  DSMModule* mod = (DSMModule*)fc();
  if (!mod) {
    ERROR("module '%s' did not return functions.\n", 
	  fname.c_str());
    return false;
  }
  mods.push_back(mod);
  DBG("loaded module '%s' from '%s'\n", 
      params.c_str(), fname.c_str());
  return true;
}

bool DSMChartReader::decode(DSMStateDiagram* e, const string& chart, 
			 const string& mod_path, DSMElemContainer* owner) {
  vector<DSMElement*> stack;
  size_t pos = 0;
  while (pos < chart.length()) {
    string token = getToken(chart, pos);
    if (!token.length())
      continue;
    
    if (token.length()>6 && token.substr(0, 6) == "import") {
      if (!importModule(token, mod_path)) {
	ERROR("error loading module in '%s'\n", 
	      token.c_str());
	return false;
      }
      continue;
    }
    
    if (token == "initial") {
      stack.push_back(new AttribInitial());
      continue;
    }

    if (token == "state") {
      stack.push_back(new State());
      continue;
    }

    if (token == "transition") {
      stack.push_back(new DSMTransition());
      continue;
    }
    
    if (stack.empty()) {
      if (token == ";")
	continue;
      ERROR("I do not understand '%s'\n", token.c_str());
      return false;
    }

    DSMElement* stack_top = &(*stack.back());
    
    State* state = dynamic_cast<State*>(stack_top);
    if (state) {
      if (!state->name.length()) {
	// 	DBG("naming state '%s'\n", token.c_str());
	state->name = token;
	continue;
      }
      if (token == "enter") {
	stack.push_back(new ActionList(ActionList::AL_enter));
	continue;
      }
      if (token == "exit") {
	stack.push_back(new ActionList(ActionList::AL_exit));
	continue;
      }
      if (token == ";") {
	bool is_initial = false;
	stack.pop_back();
	if (!stack.empty()) {
	  AttribInitial* ai = dynamic_cast<AttribInitial*>(&(*stack.back()));
	  if (ai) {
	    is_initial = true;
	    stack.pop_back();
	    delete ai;
	  }
	}
	e->addState(*state, is_initial);
	delete state;
      }
      continue;
    }

    ActionList* al = dynamic_cast<ActionList*>(stack_top);
    if (al) {
      if (token == ";") {
	continue;
      }
      if (token == "{") {
	continue;
      } 
      if ((token == "}") || (token == "->")) {
	stack.pop_back();
	if (stack.empty()) {
	  ERROR("no item for action list\n");
	  delete al;
	  return false;
	}
	
	if (al->al_type == ActionList::AL_enter || 
	    al->al_type == ActionList::AL_exit) {
	  State* s = dynamic_cast<State*>(&(*stack.back()));
	  if (!s) {
	    ERROR("no State for action list\n");
	    delete al;
	    return false;
	  }
	  if (al->al_type == ActionList::AL_enter)
	    s->pre_actions = al->actions;
	  else if (al->al_type == ActionList::AL_exit)
	    s->post_actions = al->actions;
	} else if (al->al_type == ActionList::AL_trans) {
	  DSMTransition* t = dynamic_cast<DSMTransition*>(&(*stack.back()));
	  if (!t) {
	    ERROR("no DSMTransition for action list\n");
	    delete al;
	    return false;
	  }
	  t->actions = al->actions;
	} else {
	  ERROR("internal: unknown transition list type\n");
	}
	delete al;
	continue;
      }

      // token is action
      //       DBG("adding action '%s'\n", token.c_str());
      DSMAction* a = actionFromToken(token);
      if (!a)
	return false;
      owner->transferElem(a);
      al->actions.push_back(a);
      continue;
    }
    
    DSMConditionList* cl = dynamic_cast<DSMConditionList*>(stack_top);
    if (cl) {
      if (token == ";")
	continue;

      if ((token == "{") || (token == "}")) {
	// readability
	continue;
      } 

      if ((token == "/") || (token == "->"))  {
	// end of condition list
	stack.pop_back();
	if (stack.empty()) {
	  ERROR("no transition to apply conditions to\n");
	  delete cl;
	  return false;
	}
	DSMTransition* tr = dynamic_cast<DSMTransition*>(&(*stack.back()));
	if (!tr) {
	  ERROR("no transition to apply conditions to\n");
	  delete cl;
	  return false;
	}

	tr->precond = cl->conditions;
	delete cl;

	// start AL_trans action list
	if (token == "/") {
	  stack.push_back(new ActionList(ActionList::AL_trans));
	}
	continue;
      }
      //       DBG("new condition: '%s'\n", token.c_str());
      DSMCondition* c = conditionFromToken(token);
      if (!c) 
	return false;
      
      owner->transferElem(c);
      cl->conditions.push_back(c);
      continue;
    }

    DSMTransition* tr = dynamic_cast<DSMTransition*>(stack_top);
    if (tr) {
      if (!tr->name.length()) {
	tr->name = token;
	continue;
      }

      if (!tr->from_state.length()) {
	tr->from_state = token;
	continue;
      }

      if (token == "-->") {
	continue;
      }

      if (token == "->") {
	continue;
      }

      if (token == "-") {
	stack.push_back(new DSMConditionList());
	continue;
      }

      if ((token == "-/") || (token == "/")) {
	stack.push_back(new ActionList(ActionList::AL_trans));
	continue;
      }

      if (token == ";") {
	if (!e->addTransition(*tr)) {
	  delete tr;
	  return false;
	}
	delete tr;
	  
	stack.pop_back();
	continue;
      }

      if (!tr->to_state.length()) {
	tr->to_state = token;
	continue;
      }
      continue;
    }
  }
  return true;
}

