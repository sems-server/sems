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
#ifndef _DSM_MODULE_H
#define _DSM_MODULE_H
#include "DSMStateEngine.h"
#include "AmSipMsg.h"
#include "AmArg.h"
#include "AmSession.h"

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
  virtual void onBeforeDestroy(DSMSession* sc_sess, AmSession* sess) { }
  virtual void processSdpOffer(AmSdp& offer) { }
  virtual void processSdpAnswer(const AmSdp& offer, AmSdp& answer) { }
};

typedef map<string,string> EventParamT;

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
    bool execute(AmSession* sess, DSMSession* sc_sess,			\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
  };									\
  

#define DEF_SCModSEStrArgAction(CL_Name)				\
  class CL_Name								\
  : public SCStrArgAction {						\
    bool is_evaluated;							\
  public:								\
  CL_Name(const string& arg) : SCStrArgAction(arg),			\
      is_evaluated(false) { }						\
    bool execute(AmSession* sess, DSMSession* sc_sess,			\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
    SEAction getSEAction(std::string&,					\
			 AmSession* sess, DSMSession* sc_sess,		\
			 DSMCondition::EventType event,			\
			 map<string,string>* event_params);		\
  };									\

#define DEF_ACTION_2P(CL_Name)						\
  class CL_Name								\
  : public DSMAction {							\
    string par1;							\
    string par2;							\
  public:								\
    CL_Name(const string& arg);						\
    bool execute(AmSession* sess, DSMSession* sc_sess,			\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
  };									\

/* bool xsplit(const string& arg, char sep, bool optional, string& par1, string& par2); */

#define SPLIT_ARGS(sep, optional)					\
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
    }


#define CONST_ACTION_2P(CL_name, _sep, _optional)			\
  CL_name::CL_name(const string& arg) {					\
    SPLIT_ARGS(_sep, _optional);					\
  }


#define EXEC_ACTION_START(act_name)					\
  bool act_name::execute(AmSession* sess, DSMSession* sc_sess,		\
			 DSMCondition::EventType event,			\
			 map<string,string>* event_params) {


#define EXEC_ACTION_END				\
  return false;					\
  }

#define EXEC_ACTION_STOP			\
  return false;

string resolveVars(const string s, AmSession* sess,
		   DSMSession* sc_sess, map<string,string>* event_params,
		   bool eval_ops = false);

void splitCmd(const string& from_str, 
		string& cmd, string& params);


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
  cond_name(const string& arg, bool inv)				\
    : arg(arg), inv(inv) { }						\
    bool match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event, \
	       map<string,string>* event_params);			\
  };

#define DEF_CONDITION_2P(cond_name)					\
  class cond_name							\
  : public DSMCondition {						\
    string par1;							\
    string par2;							\
    bool inv;								\
  public:								\
    cond_name(const string& arg, bool inv);				\
    bool match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event, \
	       map<string,string>* event_params);			\
  };

#define CONST_CONDITION_2P(cond_name, _sep, _optional)			\
  cond_name::cond_name(const string& arg, bool inv)			\
  : inv(inv) {								\
    SPLIT_ARGS(_sep, _optional);					\
  }

#define MATCH_CONDITION_START(cond_clsname)				\
  bool cond_clsname::match(AmSession* sess, DSMSession* sc_sess,	\
			   DSMCondition::EventType event,		\
			   map<string,string>* event_params) {

#define MATCH_CONDITION_END }

#define DECLARE_MODULE(mod_cls_name)			\
  class mod_cls_name					\
  : public DSMModule {					\
							\
  public:						\
    mod_cls_name() { }					\
 ~mod_cls_name() { }					\
							\
 DSMAction* getAction(const string& from_str);		\
 DSMCondition* getCondition(const string& from_str);	\
};

#define DECLARE_MODULE_BEGIN(mod_cls_name)		\
  class mod_cls_name					\
  : public DSMModule {					\
							\
  public:						\
    mod_cls_name() { }					\
 ~mod_cls_name() { }					\
							\
 DSMAction* getAction(const string& from_str);		\
 DSMCondition* getCondition(const string& from_str);	



#define DECLARE_MODULE_END \
  }

#define MOD_ACTIONEXPORT_BEGIN(mod_cls_name)				\
  DSMAction* mod_cls_name::getAction(const string& from_str) {		\
  string cmd;								\
  string params;							\
  splitCmd(from_str, cmd, params);					

#define MOD_ACTIONEXPORT_END			\
  return NULL;					\
  }						

#define MOD_CONDITIONEXPORT_NONE(mod_cls_name)				\
  DSMCondition* mod_cls_name::getCondition(const string& from_str) {	\
    return NULL;							\
  }


#define MOD_CONDITIONEXPORT_BEGIN(mod_cls_name)				\
  DSMCondition* mod_cls_name::getCondition(const string& from_str) {	\
  string cmd;								\
  string params;							\
  splitCmd(from_str, cmd, params);

#define MOD_CONDITIONEXPORT_END \
  return NULL;			\
  }				\


#endif
