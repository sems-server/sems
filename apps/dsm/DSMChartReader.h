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
#ifndef _DSMChartReader_h_
#define _DSMChartReader_h_

#include "DSMStateEngine.h"
#include "DSMCoreModule.h"
#include "DSMElemContainer.h"

#include <string>
using std::string;

class NamedAction : public DSMAction {
 public:
  NamedAction(const string& m_name) {
    name = m_name;
  }
  bool execute(AmSession* sess, DSMSession* sc_sess, 
	       DSMCondition::EventType event, 
	       map<string,string>* event_params) {
    return false;
  };
};

class AttribInitial : public DSMElement {
 public:
  AttribInitial() { }
};

class AttribName : public DSMElement {
 public:
  AttribName(const string& m_name) { name = m_name; }
};

class ActionList : public DSMElement {
 public:
  enum AL_type {
    AL_enter,
    AL_exit,
    AL_trans,
    AL_if,
    AL_else,
    AL_func,
    AL_for
  };

  AL_type al_type;
  
 ActionList(AL_type al_type) 
   : al_type(al_type) { }

  vector<DSMElement*> actions;
};

struct DSMConditionList : public DSMElement {
 DSMConditionList() : invert_next(false), is_exception(false), is_if(false) { }
  vector<DSMCondition*> conditions;
  bool invert_next;
  bool is_exception;
  bool is_if;
};

class DSMChartReader {

  bool is_wsp(const char c);
  bool is_snt(const char c);

  string getToken(string str, size_t& pos);
  DSMFunction* functionFromToken(const string& str);
  DSMAction* actionFromToken(const string& str);
  DSMCondition* conditionFromToken(const string& str, bool invert);
  bool forFromToken(DSMArrayFor& af, const string& token);

  bool importModule(const string& mod_cmd, const string& mod_path);
  vector<DSMModule*> mods;
  DSMCoreModule core_mod;

  vector<DSMFunction*> funcs;

 public:
  DSMChartReader();
  ~DSMChartReader();
  bool decode(DSMStateDiagram* e, const string& chart, 
	      const string& mod_path, DSMElemContainer* owner,
	      vector<DSMModule*>& out_mods);

  friend class DSMFactory;
  void cleanup();
};

#endif
