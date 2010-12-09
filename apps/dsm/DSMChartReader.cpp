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
#include "DSMChartReader.h"
#include "log.h"

#include <dlfcn.h> // dlopen & friends

#include <vector>
using std::vector;

#include <string>
using std::string;

DSMChartReader::DSMChartReader() {
}

DSMChartReader::~DSMChartReader() {
}

bool DSMChartReader::is_wsp(const char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool DSMChartReader::is_snt(const char c) {
  return c== ';' || c == '{' || c == '}' || c == '[' || c == ']';
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
    } else  if (str[pos1] == '\'') {
      pos1++;
      while (pos1<str.length() && !((str[pos1] == '\'') && (last_chr != '\\'))) {
	last_chr = str[pos1];
	pos1++;
      }
    } else if (str[pos1] == '(') {
      int lvl = 0;
      pos1++;
      while (pos1<str.length() && (lvl || (str[pos1] != ')'))) {

	if (str[pos1] == '(')
	  lvl++;
	else if (str[pos1] == ')')
	  lvl--;
	    
	else if (str[pos1] == '"') {
	  pos1++;
	  while (pos1<str.length() && !((str[pos1] == '"') && (last_chr != '\\'))) {
	    last_chr = str[pos1];
	    pos1++;
	  }
	}
	else if (str[pos1] == '\'') {
	  pos1++;
	  while (pos1<str.length() && !((str[pos1] == '\'') && (last_chr != '\\'))) {
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
  if ((str[pos] == '"') || (str[pos] == '\''))
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

  DSMAction* a = core_mod.getAction(str);
  if (a) return a;

  ERROR("could not find action for '%s' (missing import?)\n", str.c_str());
  return NULL;
}

DSMFunction* DSMChartReader::functionFromToken(const string& str) {
  string cmd;
  size_t b_pos = str.find('(');
  if (b_pos != string::npos) {
    cmd = str.substr(0, b_pos);
  } else {
    return NULL;
  }

  for (vector<DSMFunction*>::iterator it=funcs.begin(); it!= funcs.end(); it++) {
    if((*it)->name == cmd) {
        DBG("found function '%s' in fuction list\n", cmd.c_str());
        return *it;
    }
  }
  return NULL;
}

DSMCondition* DSMChartReader::conditionFromToken(const string& str, bool invert) {
  for (vector<DSMModule*>::iterator it=
	 mods.begin(); it!= mods.end(); it++) {
    DSMCondition* c=(*it)->getCondition(str);
    if (c) {
      c->invert = invert;
      return c;
    }
  }

  DSMCondition* c = core_mod.getCondition(str);
  if (c) 
    c->invert = invert;

  if (c)  return c;
  ERROR("could not find condition for '%s' (missing import?)\n", str.c_str());
  return NULL;
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
    ERROR("invalid SC module '%s' (SC_EXPORT missing?)\n", fname.c_str());
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
			    const string& mod_path, DSMElemContainer* owner,
			    vector<DSMModule*>& out_mods) {
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
    
    if (token == "function") {
      stack.push_back(new DSMFunction());
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
      ERROR("Without context I do not understand '%s'\n", token.c_str());
      return false;
    }

    DSMElement* stack_top = &(*stack.back());
    
    DSMFunction* f = dynamic_cast<DSMFunction*>(stack_top);
    
    if (f) {
      if (f->name.length()==0) {
        size_t b_pos = token.find('(');
        if (b_pos != string::npos) {
          f->name = token.substr(0, b_pos);
          continue;
        } else {
          ERROR("Parse error -- function declarations must have a name followed by parentheses, e.g., 'function foo()'\n");
          return false;
        }
      }
          
      if (token == "{") {
      	stack.push_back(new ActionList(ActionList::AL_func));
      	continue;
      }
      if (token == ";") {
        owner->transferElem(f);
        funcs.push_back(f);
      	DBG("Adding DSMFunction '%s' to funcs\n", f->name.c_str());
       	continue;
      }
      
      DBG("Unknown token: %s\n", token.c_str());
      return false;
   }
    
    DSMConditionTree* ct = dynamic_cast<DSMConditionTree*>(stack_top);
   	if (ct) {
      if (token == "[") {
  	    stack.push_back(new DSMConditionList());
  	    continue;
  	  }
      if (token == "{") {
      	stack.push_back(new ActionList(ActionList::AL_if));
      	continue;
      }
   	  if (token == ";") {
   	  	stack.pop_back();
    	  ActionList* al = dynamic_cast<ActionList*>(&(*stack.back()));
    	  if (al) {
      		owner->transferElem(ct);
      		al->actions.push_back(ct);
      		DBG("Added DSMConditionTree to ActionList\n");
    	  } else {
    	    ERROR("no ActionList for DSMConditionTree\n");
    	    delete al;
    	    return false;
    	  }
       	continue;
     	}
   	}
    

    
    State* state = dynamic_cast<State*>(stack_top);
    if (state) {
      if (!state->name.length()) {
	// 	DBG("naming state '%s'\n", token.c_str());
	state->name = token;
	continue;
      }
      if (token == "enter") {
      DBG("adding 'enter' actions for state '%s'\n", state->name.c_str());
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
  	    
  	    if (al->al_type == ActionList::AL_func) {
  	        DSMFunction* f = dynamic_cast<DSMFunction*>(&(*stack.back()));
        	if (!f) {
          	  ERROR("no DSMFunction for action list\n");
          	  delete al;
          	  return false;
        	}
        	f->actions = al->actions;
  	    } else if (al->al_type == ActionList::AL_if ||
            al->al_type == ActionList::AL_else) {
        	DSMConditionTree* ct = dynamic_cast<DSMConditionTree*>(&(*stack.back()));
        	if (!ct) {
          	  ERROR("no DSMConditionTree for action list\n");
          	  delete al;
          	  return false;
        	}
        	
        	if (al->al_type == ActionList::AL_if) {
          	ct->run_if_true = al->actions;
        	} else if (al->al_type == ActionList::AL_else) {
          	ct->run_if_false = al->actions;
          }
  	    } else if (al->al_type == ActionList::AL_enter || 
            al->al_type == ActionList::AL_exit) {
        	State* s = dynamic_cast<State*>(&(*stack.back()));
        	if (!s) {
          	ERROR("no State for action list\n");
          	delete al;
          	return false;
        	}
        	
        	if (al->al_type == ActionList::AL_enter) {
          	s->pre_actions = al->actions;
        	} else if (al->al_type == ActionList::AL_exit) {
          	s->post_actions = al->actions;
          }
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
  	
        if (token == "if") {
          	//token is condition tree
          	stack.push_back(new DSMConditionTree());
          	continue;
        } else {
            DSMFunction* f = functionFromToken(token);
            if (f) {
              DBG("adding actions from function '%s'\n", f->name.c_str());
              DBG("al.size is %zd before", al->actions.size());
              for (vector<DSMElement*>::iterator it=f->actions.begin(); it != f->actions.end(); it++) {
                DSMElement* a = *it;
              	owner->transferElem(a);
                al->actions.push_back(a);
              }
              DBG("al.size is %zd after", al->actions.size());
            } else {
              	DBG("adding action '%s'\n", token.c_str());
              	DSMAction* a = actionFromToken(token);
              	if (!a)
                	return false;
              	owner->transferElem(a);
              	al->actions.push_back(a);
            }
          	continue;
        }
  	}
    
     
    DSMConditionList* cl = dynamic_cast<DSMConditionList*>(stack_top);
    if (cl) {
      if (token == ";")
        continue;

      if ((token == "{") || (token == "}")) {
      	// readability
      	continue;
      } 

      if ((token == "/") || (token == "->") || (token == "]"))  {
      	// end of condition list
      	stack.pop_back();
      	if (stack.empty()) {
      	  ERROR("nothing to apply conditions to\n");
      	  delete cl;
      	  return false;
      	}
      	
      	DSMElement* el = &(*stack.back());
      	
      	DSMTransition* tr = dynamic_cast<DSMTransition*>(el);
      	DSMConditionTree* ct = dynamic_cast<DSMConditionTree*>(el);
      	if (tr) {
        	tr->precond = cl->conditions;
        	tr->is_exception = cl->is_exception;
        } else if (ct) {
          ct->conditions = cl->conditions;
          ct->is_exception = cl->is_exception;
        } else {
      	  ERROR("no transition or condition list to apply conditions to\n");
      	  delete cl;
      	  return false;
      	}
      
      	delete cl;
      
      	// start AL_trans action list
      	if (token == "/") {
      	  stack.push_back(new ActionList(ActionList::AL_trans));
      	}
      	continue;
      }
      
      if (token == "not") {
      	cl->invert_next = !cl->invert_next;
      	continue;
      }

      if (token == "exception") {
      	cl->is_exception = true;
      	continue;
      }
      
      DBG("new condition: '%s'\n", token.c_str());
      DSMCondition* c = conditionFromToken(token, cl->invert_next);
      cl->invert_next = false;
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

  for (vector<DSMModule*>::iterator it=
	 mods.begin(); it != mods.end(); it++)
    out_mods.push_back(*it);

  return true;
}


void DSMChartReader::cleanup() {
  for (vector<DSMModule*>::iterator it=mods.begin(); it != mods.end(); it++)
    delete *it;
  mods.clear();  
}
