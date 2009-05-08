/*
 * $Id$
 *
 * Copyright (C) 2009 IPTEGO GmbH
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

#include "ModPy.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "PyDSMSession.h"
#include "PyDSM.h"

struct PythonGIL
{
  PyGILState_STATE gst;

  PythonGIL() { gst = PyGILState_Ensure(); }
  ~PythonGIL(){ PyGILState_Release(gst);   }
};
#define PYLOCK PythonGIL _py_gil

SC_EXPORT(SCPyModule);

PyObject* SCPyModule::dsm_module = NULL;
PyObject* SCPyModule::session_module = NULL;

SCPyModule::SCPyModule() {

}

SCPyModule::~SCPyModule() {
}

int SCPyModule::preload() {
  if(!Py_IsInitialized()){
    Py_Initialize();
    DBG("Python version %s\n", Py_GetVersion());
  }

  PyEval_InitThreads();

  PyImport_AddModule("dsm");
  dsm_module = Py_InitModule("dsm",mod_py_methods);
  PyModule_AddIntConstant(dsm_module, "Any", DSMCondition::Any);
  PyModule_AddIntConstant(dsm_module, "Invite", DSMCondition::Invite);
  PyModule_AddIntConstant(dsm_module, "SessionStart", DSMCondition::SessionStart);
  PyModule_AddIntConstant(dsm_module, "Key", DSMCondition::Key);
  PyModule_AddIntConstant(dsm_module, "Timer", DSMCondition::Timer);
  PyModule_AddIntConstant(dsm_module, "NoAudio", DSMCondition::NoAudio);
  PyModule_AddIntConstant(dsm_module, "Hangup", DSMCondition::Hangup);
  PyModule_AddIntConstant(dsm_module, "Hold", DSMCondition::Hold);  PyModule_AddIntConstant(dsm_module, "UnHold", DSMCondition::UnHold);
  PyModule_AddIntConstant(dsm_module, "XmlrpcResponse", DSMCondition::XmlrpcResponse);
  PyModule_AddIntConstant(dsm_module, "DSMEvent", DSMCondition::DSMEvent);
  PyModule_AddIntConstant(dsm_module, "PlaylistSeparator", DSMCondition::PlaylistSeparator);
  PyModule_AddIntConstant(dsm_module, "B2BOtherReply", DSMCondition::B2BOtherReply);
  PyModule_AddIntConstant(dsm_module, "B2BOtherBye", DSMCondition::B2BOtherBye);

  PyImport_AddModule("session");
  session_module = Py_InitModule("session",session_methods);

  PyEval_ReleaseLock();
  return 0;
//   _thr_state = PyEval_SaveThread();
}

DSMAction* SCPyModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  if (NULL==dsm_module) {
    ERROR("mod_py must be preloaded! add preload=mod_py to dsm.conf\n");
    return NULL;
  }

  try {
    DEF_CMD("py", SCPyPyAction);
  } catch (const string& err) {
    ERROR("creating py() action\n");
    return NULL;
  }

  return NULL;
}

DSMCondition* SCPyModule::getCondition(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  if (NULL==dsm_module) {
    ERROR("mod_py must be preloaded! add preload=mod_py to dsm.conf\n");
    return NULL;
  }

  if (cmd == "py") {
    try {
      return new PyPyCondition(params);
    } catch (const string& err) {
      ERROR("creating py() condition\n");
      return NULL;
    }
  }

  return NULL;
}

bool py_execute(PyCodeObject* py_func, DSMSession* sc_sess, 
		DSMCondition::EventType event, map<string,string>* event_params,
		bool expect_int_result) {
  // acquire the GIL
  PYLOCK;

  bool py_res = false;
  
  PyObject* m = PyImport_AddModule("__main__");
  if (m == NULL) {
    ERROR("getting main module\n");
    return false;
  }
  PyObject*d = PyModule_GetDict(m);

  PyObject* locals = PyDict_New();
  PyDict_SetItem(locals, PyString_FromString("dsm"), SCPyModule::dsm_module);
  PyDict_SetItem(locals, PyString_FromString("session"), SCPyModule::session_module);

  PyObject* params = PyDict_New();
  if (NULL != event_params) {
    for (map<string,string>::iterator it=event_params->begin(); 
	 it != event_params->end(); it++) {
      PyDict_SetItemString(params, it->first.c_str(), 
			   PyString_FromString(it->second.c_str()));
    }
  }
  PyDict_SetItemString(locals, "params", params);
  PyDict_SetItemString(locals, "type", PyInt_FromLong(event));

  PyObject* py_sc_sess = PyCObject_FromVoidPtr(sc_sess,NULL);
  PyObject* ts_dict = PyThreadState_GetDict();
  PyDict_SetItemString(ts_dict, "_dsm_sess_", py_sc_sess);

  // call the function
  PyObject* res = PyEval_EvalCode((PyCodeObject*)py_func, d, locals);

  if(PyErr_Occurred())
    PyErr_Print();
  
  ts_dict = PyThreadState_GetDict(); // should be the same as before
  py_sc_sess = PyDict_GetItemString(ts_dict, "_dsm_sess_"); // should be the same as before
  Py_XDECREF(py_sc_sess);  
  PyDict_DelItemString(ts_dict, "_dsm_sess_");
  
  Py_DECREF(params);
  Py_DECREF(locals);
  if (NULL == res) {
    ERROR("evaluating python code\n");
  } else if (PyBool_Check(res)) {
    py_res = PyInt_AsLong(res);
  } else {
    if (expect_int_result) {
      ERROR("unknown result from python code\n");
    }
    Py_DECREF(res);
  }

  return py_res;
}

SCPyPyAction::SCPyPyAction(const string& arg) {
  py_func = Py_CompileString(arg.c_str(), "<mod_py>", Py_file_input);
  if (NULL == py_func) {
    ERROR("compiling python code '%s'\n", 
	  arg.c_str());
    if(PyErr_Occurred())
      PyErr_Print();

    throw string("compiling python code '" + arg +"'");
  }
}

EXEC_ACTION_START(SCPyPyAction) {
  py_execute((PyCodeObject*)py_func, sc_sess, 
	     event, event_params, false);

} EXEC_ACTION_END;


PyPyCondition::PyPyCondition(const string& arg) {
  py_func = Py_CompileString(arg.c_str(), "<mod_py>", Py_eval_input);
  if (NULL == py_func) {
    ERROR("compiling python code '%s'\n", 
	  arg.c_str());
    if(PyErr_Occurred())
      PyErr_Print();

    throw string("compiling python code '" + arg +"'");
  }
}

MATCH_CONDITION_START(PyPyCondition) {
  return py_execute((PyCodeObject*)py_func, sc_sess, 
	     event, event_params, false);
} MATCH_CONDITION_END;
