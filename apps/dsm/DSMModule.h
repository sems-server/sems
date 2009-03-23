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
#ifndef _DSM_MODULE_H
#define _DSM_MODULE_H
#include "DSMStateEngine.h"
#include "AmSipMsg.h"
#include "AmArg.h"

class DSMSession;

#include <string>
using std::string;

#include <typeinfo>

// script modules interface
// factory only: it produces actions and conditions from script statements.
class DSMModule {

 public:
  DSMModule();
  virtual ~DSMModule();
  
  virtual DSMAction* getAction(const string& from_str) = 0;
  virtual DSMCondition* getCondition(const string& from_str) = 0;

  virtual int preload() { return 0; }
  virtual bool onInvite(const AmSipRequest& req, DSMSession* sess) { return true; }
};

typedef void* (*SCFactoryCreate)();

#define SCSTR(x) #x
#define SCXSTR(x) SCSTR(x)

#define SC_FACTORY_EXPORT      sc_factory_create
#define SC_FACTORY_EXPORT_STR  SCXSTR(SC_FACTORY_EXPORT)

#if  __GNUC__ < 3
#define EXPORT_SC_FACTORY(fctname,class_name,args...)	\
  extern "C" void* fctname()				\
  {							\
    return new class_name(##args);			\
  }
#else
#define EXPORT_SC_FACTORY(fctname,class_name,...)	\
  extern "C" void* fctname()				\
  {							\
    return new class_name(__VA_ARGS__);			\
  }
#endif

#define SC_EXPORT(class_name)			\
  EXPORT_SC_FACTORY(SC_FACTORY_EXPORT,class_name)


string trim(string const& str,char const* sepSet);

class SCStrArgAction   
: public DSMAction {
 protected:
  string arg;
 public:
  SCStrArgAction(const string& m_arg); 
};

#define DEF_ACTION_1P(CL_Name)						\
  class CL_Name								\
  : public SCStrArgAction {						\
  public:								\
  CL_Name(const string& arg) : SCStrArgAction(arg) { }			\
    bool execute(AmSession* sess,					\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
  };									\
  

#define DEF_SCModSEStrArgAction(CL_Name)				\
  class CL_Name								\
  : public SCStrArgAction {						\
  public:								\
  CL_Name(const string& arg) : SCStrArgAction(arg) { }			\
    bool execute(AmSession* sess,					\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
    SEAction getSEAction(std::string&);					\
  };									\

#define DEF_ACTION_2P(CL_Name)						\
  class CL_Name								\
  : public DSMAction {							\
    string par1;							\
    string par2;							\
  public:								\
    CL_Name(const string& arg);						\
    bool execute(AmSession* sess,					\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
  };									\

/* bool xsplit(const string& arg, char sep, bool optional, string& par1, string& par2); */

#define CONST_ACTION_2P(CL_name, sep, optional)				\
  CL_name::CL_name(const string& arg) {					\
    size_t p = 0;							\
    char last_c = ' ';							\
    bool quot=false;							\
    char quot_c = ' ';							\
    bool sep_found = false;						\
    while (p<arg.size()) {						\
      if (quot) {							\
	if (last_c != '\\' && arg[p]==quot_c)				\
	  quot=false;							\
      } else {								\
	if (last_c != '\\'  && (arg[p]=='\'' || arg[p]=='\"')) {	\
	  quot = true;							\
	  quot_c = arg[p];						\
	} else {							\
	  if (arg[p] == sep) {						\
	    sep_found = true;						\
	    break;							\
	  }								\
	}								\
      }									\
      p++;								\
      last_c = arg[p];							\
    }									\
									\
    if ((!optional) && (!sep_found)) {					\
      ERROR("expected two parameters separated with '%c' in expression '%s' for %s\n", \
	    sep,arg.c_str(),typeid(this).name());			\
      return;								\
    }									\
									\
    par1 = trim(arg.substr(0,p), " \t");				\
    if (sep_found) 							\
      par2 = trim(arg.substr(p+1), " \t");				\
									\
    if (par1.length() && par1[0]=='\'') {				\
      par1 = trim(par1, "\'");						\
      size_t rpos = 0;							\
      while ((rpos=par1.find("\\\'")) != string::npos)			\
	par1.erase(rpos, 1);						\
    } else if (par1.length() && par1[0]=='\"') {			\
      par1 = trim(par1, "\"");						\
      size_t rpos = 0;							\
      while ((rpos=par1.find("\\\"")) != string::npos)			\
	par1.erase(rpos, 1);						\
    }									\
									\
    if (par2.length() && par2[0]=='\'') {				\
      par2 = trim(par2, "\'");						\
      size_t rpos = 0;							\
      while ((rpos=par2.find("\\\'")) != string::npos)			\
	par2.erase(rpos, 1);						\
    } else if (par2.length() && par2[0]=='\"') {			\
      par2 = trim(par2, "\"");						\
      size_t rpos = 0;							\
      while ((rpos=par2.find("\\\"")) != string::npos)			\
	par2.erase(rpos, 1);						\
    }									\
									\
    if ((!optional) && ((par1.empty())||(par2.empty()))) {		\
      ERROR("expected two parameters separated with '%c' in expression '%s' for %s\n", \
	    sep,arg.c_str(),typeid(this).name());			\
      return;								\
    }									\
  }									\
  

#define GET_SCSESSION()					       \
  DSMSession* sc_sess = dynamic_cast<DSMSession*>(sess);       \
  if (!sc_sess) {					       \
    ERROR("wrong session type\n");			       \
    return false;					       \
  }


#define EXEC_ACTION_START(act_name)					\
  bool act_name::execute(AmSession* sess,				\
			 DSMCondition::EventType event,			\
			 map<string,string>* event_params) {		\
  GET_SCSESSION();							

#define EXEC_ACTION_END				\
  return false;					\
  }

string resolveVars(const string s, AmSession* sess,
		   DSMSession* sc_sess, map<string,string>* event_params);

#define DEF_CMD(cmd_name, class_name) \
				      \
  if (cmd == cmd_name) {	      \
    class_name * a =		      \
      new class_name(params);	      \
    a->name = from_str;		      \
    return a;			      \
  }

#define DEF_SCCondition(cond_name)		\
  class cond_name				\
  : public DSMCondition {			\
    string arg;					\
    bool inv;					\
    						\
  public:					\
    						\
  cond_name(const string& arg, bool inv)			\
    : arg(arg), inv(inv) { }					\
    bool match(AmSession* sess, DSMCondition::EventType event,	\
	       map<string,string>* event_params);		\
  };								\
  

#define MATCH_CONDITION_START(cond_clsname)				\
  bool cond_clsname::match(AmSession* sess, DSMCondition::EventType event, \
			   map<string,string>* event_params) {		\
  GET_SCSESSION();

#define MATCH_CONDITION_END }			


#endif
