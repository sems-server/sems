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

// script modules interface
// factory only: it produces actions and conditions from script statements.
class DSMModule {

 public:
  DSMModule();
  virtual ~DSMModule();
  
  virtual DSMAction* getAction(const string& from_str) = 0;
  virtual DSMCondition* getCondition(const string& from_str) = 0;

  virtual int preload() { return 0; }
  virtual void onInvite(const AmSipRequest& req, DSMSession* sess) { }
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



class SCStrArgAction   
: public DSMAction {
 protected:
  string arg;
 public:
  SCStrArgAction(const string& arg)
    : arg(arg) { }
};

#define DEF_SCStrArgAction(CL_Name)					\
  class CL_Name								\
  : public SCStrArgAction {						\
  public:								\
  CL_Name(const string& arg) : SCStrArgAction(arg) { }			\
    bool execute(AmSession* sess,					\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
  };									\
  

#define DEF_SCModSEStrArgAction(CL_Name)					\
  class CL_Name								\
  : public SCStrArgAction {						\
  public:								\
  CL_Name(const string& arg) : SCStrArgAction(arg) { }			\
    bool execute(AmSession* sess,					\
		 DSMCondition::EventType event,				\
		 map<string,string>* event_params);			\
    SEAction getSEAction(std::string&);					\
  };									\

#define DEF_TwoParAction(CL_Name)					\
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

#define CONST_TwoParAction(CL_name, sep, optional)		\
  CL_name::CL_name(const string& arg) {				\
    vector<string> args = explode(arg,sep);			\
    if (!optional && args.size()!=2) {				\
      ERROR("expression '%s' not valid\n", arg.c_str());	\
      return;							\
    }								\
    par1 = args.size()?trim(args[0], " \t"):"";			\
    par2 = args.size()>1?trim(args[1], " \t"):"";		\
  }

#endif
