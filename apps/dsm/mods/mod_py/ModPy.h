/*
 * Copyright (C) 2009 IPTEGO GmbH
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
#ifndef _MOD_PY_H
#define _MOD_PY_H
#include <Python.h>

#include "DSMModule.h"
#include "DSMSession.h"

#define MOD_CLS_NAME SCPyModule

class SCPyModule
: public DSMModule {

 public:

  SCPyModule();
  ~SCPyModule();
  
  int preload();

  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);
  static PyObject* dsm_module;
  static PyObject* session_module;
  static PyInterpreterState* interp;
  static PyThreadState* tstate;
};

/** smart AmObject that "owns" a python dictionary reference */
struct SCPyDictArg
  : public AmObject, 
  public DSMDisposable   
{
  SCPyDictArg();
  SCPyDictArg(PyObject* pPyObject);
  ~SCPyDictArg();
  PyObject* pPyObject;
};

class SCPyPyAction			
  : public DSMAction {			
    PyObject* py_func;
 public:				
    SCPyPyAction(const string& arg);
    bool execute(AmSession* sess, DSMSession* sc_sess,
		 DSMCondition::EventType event,	
		 map<string,string>* event_params);
};

class PyPyCondition
: public DSMCondition {
  
  PyObject* py_func;
 public:
  
  PyPyCondition(const string& arg);
  bool match(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
	     map<string,string>* event_params);
};

#endif
