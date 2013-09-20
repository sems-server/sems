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

#include "ModPy.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "PyDSMSession.h"
#include "PyDSM.h"
#include "AmArg.h"
   #include <stdio.h>

#include "grammar.h"
#include "pythread.h"

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
PyInterpreterState* SCPyModule::interp = NULL;
PyThreadState* SCPyModule::tstate = NULL;

SCPyModule::SCPyModule() {

}

int SCPyModule::preload() {
  if(!Py_IsInitialized()){
    add_env_path("PYTHONPATH",AmConfig::PlugInPath);
    Py_Initialize();
    DBG("Python version %s\n", Py_GetVersion());
  }

  PyEval_InitThreads();

  interp = PyThreadState_Get()->interp; 
  tstate = PyThreadState_Get();

  PyImport_AddModule("dsm");
  dsm_module = Py_InitModule("dsm",mod_py_methods);
  PyModule_AddIntConstant(dsm_module, "Any", DSMCondition::Any);
  PyModule_AddIntConstant(dsm_module, "Invite", DSMCondition::Invite);
  PyModule_AddIntConstant(dsm_module, "SessionStart", DSMCondition::SessionStart);
  PyModule_AddIntConstant(dsm_module, "Key", DSMCondition::Key);
  PyModule_AddIntConstant(dsm_module, "Timer", DSMCondition::Timer);
  PyModule_AddIntConstant(dsm_module, "NoAudio", DSMCondition::NoAudio);
  PyModule_AddIntConstant(dsm_module, "Hangup", DSMCondition::Hangup);
  PyModule_AddIntConstant(dsm_module, "Hold", DSMCondition::Hold);  
  PyModule_AddIntConstant(dsm_module, "UnHold", DSMCondition::UnHold);
  PyModule_AddIntConstant(dsm_module, "XmlrpcResponse", DSMCondition::XmlrpcResponse);
  PyModule_AddIntConstant(dsm_module, "DSMEvent", DSMCondition::DSMEvent);
  PyModule_AddIntConstant(dsm_module, "PlaylistSeparator", DSMCondition::PlaylistSeparator);
  PyModule_AddIntConstant(dsm_module, "B2BOtherReply", DSMCondition::B2BOtherReply);
  PyModule_AddIntConstant(dsm_module, "B2BOtherBye", DSMCondition::B2BOtherBye);

  PyImport_AddModule("session");
  session_module = Py_InitModule("session",session_methods);

  PyEval_ReleaseLock();
  return 0;
}

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  if (NULL==dsm_module) {
    ERROR("mod_py must be preloaded! add preload_mods=mod_py to dsm.conf\n");
    return NULL;
  }

  try {
    DEF_CMD("py", SCPyPyAction);
  } catch (const string& err) {
    ERROR("creating py() action\n");
    return NULL;
  }

} MOD_ACTIONEXPORT_END;


MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {

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

} MOD_CONDITIONEXPORT_END;

SCPyDictArg::SCPyDictArg() 
 : pPyObject(NULL) {
}

SCPyDictArg::SCPyDictArg(PyObject* pPyObject) 
 : pPyObject(pPyObject) {
}

SCPyDictArg::~SCPyDictArg() {
  PYLOCK;

  if (NULL != pPyObject) {
    PyDict_Clear(pPyObject);
  }
  Py_XDECREF(pPyObject); 
}

PyObject* getPyLocals(DSMSession* sc_sess) {
  map<string, AmArg>::iterator l_it;
  SCPyDictArg* py_arg = NULL;
  AmObject* py_locals_obj;

  if (((l_it=sc_sess->avar.find("py_locals")) != sc_sess->avar.end()) && 
      (l_it->second.getType() == AmArg::AObject) && 
      ((py_locals_obj = l_it->second.asObject()) != NULL) &&
      ((py_arg = dynamic_cast<SCPyDictArg*>(py_locals_obj)) != NULL) &&
      (py_arg->pPyObject != NULL)
      ) {
    return py_arg->pPyObject;
  }

  PyObject* locals = PyDict_New();
  PyDict_SetItemString(locals, "dsm", SCPyModule::dsm_module);
  PyDict_SetItemString(locals, "session", SCPyModule::session_module);
  
  py_arg = new SCPyDictArg(locals);
  sc_sess->transferOwnership(py_arg);
  sc_sess->avar["py_locals"] = AmArg(py_arg);
  
  return locals;
}

bool py_execute(PyCodeObject* py_func, DSMSession* sc_sess, 
		DSMCondition::EventType event, map<string,string>* event_params,
		bool expect_int_result) {
  // acquire the GIL
  PYLOCK;

  bool py_res = false;
  DBG("add main \n");
  PyObject* m = PyImport_AddModule("__main__");
  if (m == NULL) {
    ERROR("getting main module\n");
    return false;
  }
  DBG("get globals \n");
  PyObject* globals = PyModule_GetDict(m);
  PyObject* locals = getPyLocals(sc_sess);

  PyObject* params = PyDict_New();
  if (NULL != event_params) {
    for (map<string,string>::iterator it=event_params->begin(); 
	 it != event_params->end(); it++) {
      PyObject* v = PyString_FromString(it->second.c_str());
      PyDict_SetItemString(params, it->first.c_str(), v);
      Py_DECREF(v);
    }
  }
  PyDict_SetItemString(locals, "params", params);

  PyObject *t = PyInt_FromLong(event);
  PyDict_SetItemString(locals, "type", t);

  PyObject* py_sc_sess = PyCObject_FromVoidPtr(sc_sess,NULL);
  PyObject* ts_dict = PyThreadState_GetDict();
  PyDict_SetItemString(ts_dict, "_dsm_sess_", py_sc_sess);
  Py_DECREF(py_sc_sess);

  // call the function
  PyObject* res = PyEval_EvalCode((PyCodeObject*)py_func, globals, locals);

  if(PyErr_Occurred())
    PyErr_Print();

  PyDict_DelItemString(locals, "params");
  PyDict_Clear(params);
  Py_DECREF(params);

  PyDict_DelItemString(locals, "type");
  Py_DECREF(t);
  
  //   ts_dict = PyThreadState_GetDict(); // should be the same as before
  PyDict_DelItemString(ts_dict, "_dsm_sess_");
  
  if (NULL == res) {
    ERROR("evaluating python code\n");
  } else if (PyBool_Check(res)) {
    py_res = PyInt_AsLong(res);
    Py_DECREF(res);
  } else {
    if (expect_int_result) {
      ERROR("unknown result from python code\n");
    }
    Py_DECREF(res);
  }

  return py_res;
}

SCPyPyAction::SCPyPyAction(const string& arg) {
  PYLOCK;
  py_func = Py_CompileString(arg.c_str(), ("<mod_py action: '"+arg+"'>").c_str(), Py_file_input);
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
  PYLOCK;
  py_func = Py_CompileString(arg.c_str(), ("<mod_py condition: '"+arg+"'>").c_str(), Py_eval_input);
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


// define PYDSM_WITH_MEM_DEBUG

#ifndef PYDSM_WITH_MEM_DEBUG
SCPyModule::~SCPyModule() { }
#else

void printdict(PyObject* p, char* name) {
  return;

  DBG("dict %s %p -------------\n", name, p);
  PyObject *key, *value;
  Py_ssize_t pos = 0;
  
  while (PyDict_Next(p, &pos, &key, &value)) {
    DBG(" obj '%s' ref %d\n", PyString_AsString(key), key->ob_refcnt);
  }
  DBG("dict %p end -------------\n", p);
}

extern grammar _PyParser_Grammar; /* From graminit.c */

SCPyModule::~SCPyModule() {
  //PYLOCK;
  PyEval_AcquireThread(tstate);
  FILE* f = fopen("refs.txt", "w");
  
  _Py_PrintReferences(f);
  
  /* Disable signal handling */
  PyOS_FiniInterrupts();

  PyInterpreterState_Clear(interp);
  
  
  /* Delete current thread */
  PyThreadState_Swap(NULL);
  PyInterpreterState_Delete(interp);
  
  /* Sundry finalizers */
  PyMethod_Fini();
  PyFrame_Fini();
  PyCFunction_Fini();
  PyTuple_Fini();
  PyList_Fini();
  PySet_Fini();
  PyString_Fini();
  PyInt_Fini();
  PyFloat_Fini();
  
  PyGrammar_RemoveAccelerators(&_PyParser_Grammar);
  
  _Py_PrintReferenceAddresses(f);

  fclose(f);
}
#endif //PYDSM_WITH_MEM_DEBUG
