/*
 * $Id$
 *
 * Copyright (C) 2009 TelTech Systems
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
#ifndef _MOD_PY_H
#define _MOD_PY_H
#include "DSMModule.h"
#include "DSMSession.h"

#include <Python.h>


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
};

class SCPyPyAction			
  : public DSMAction {			
    PyObject* py_func;
 public:				
    SCPyPyAction(const string& arg);
    bool execute(AmSession* sess,	
		 DSMCondition::EventType event,	
		 map<string,string>* event_params);
  };							

class PyPyCondition
: public DSMCondition {

  PyObject* py_func;    		
 public:	
    		
    PyPyCondition(const string& arg);
    bool match(AmSession* sess, DSMCondition::EventType event,	
	       map<string,string>* event_params);
  };								

#endif
