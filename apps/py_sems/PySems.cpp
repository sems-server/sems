/*
 * $Id: PySems.cpp,v 1.26.2.1 2005/09/02 13:47:46 rco Exp $
 * 
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PySemsAudio.h"
#include "PySems.h"

#include "AmConfigReader.h"
#include "AmConfig.h"
#include "log.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "AmPlugIn.h"

#include "PySemsDialog.h"
#include "PySemsB2BDialog.h"
#include "PySemsUtils.h"

#include "sip/sipAPIpy_sems_lib.h"
#include "sip/sippy_sems_libPySemsDialog.h"
#include "sip/sippy_sems_libPySemsB2BDialog.h"
#include "sip/sippy_sems_libPySemsB2ABDialog.h"

#include <unistd.h>
#include <pthread.h>
#include <regex.h>
#include <dirent.h>

#include <set>
using std::set;


#define PYFILE_REGEX "(.+)\\.(py|pyc|pyo)$"


EXPORT_SESSION_FACTORY(PySemsFactory,MOD_NAME);

PyMODINIT_FUNC initpy_sems_lib();

/** 
 * \brief gets python global interpreter lock (GIL)
 * 
 * structure to acquire the python global interpreter lock (GIL) 
 * while the structure is allocated.
 */
struct PythonGIL
{
  PyGILState_STATE gst;

  PythonGIL() { gst = PyGILState_Ensure(); }
  ~PythonGIL(){ PyGILState_Release(gst);   }
};


// This must be the first declaration of every 
// function using Python C-API.
// But this is not necessary in function which
// will get called from Python
#define PYLOCK PythonGIL _py_gil

extern "C" {

  static PyObject* py_sems_log(PyObject*, PyObject* args)
  {
    int level;
    char *msg;

    if(!PyArg_ParseTuple(args,"is",&level,&msg))
      return NULL;

    if((level)<=log_level) {

      if(log_stderr)
	log_print( level, msg );
      else {
	switch(level){
	case L_ERR:
	  syslog(LOG_ERR | L_FAC, "Error: %s", msg);
	  break;
	case L_WARN:
	  syslog(LOG_WARNING | L_FAC, "Warning: %s", msg);
	  break;
	case L_INFO:
	  syslog(LOG_INFO | L_FAC, "Info: %s", msg);
	  break;
	case L_DBG:
	  syslog(LOG_DEBUG | L_FAC, "Debug: %s", msg);
	  break;
	}
      }
    }
	
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* py_sems_getHeader(PyObject*, PyObject* args)
  {
    char* headers;
    char* header_name;
    if(!PyArg_ParseTuple(args,"ss",&headers,&header_name))
      return NULL;

    string res = getHeader(headers,header_name);
    return PyString_FromString(res.c_str());
  }


  static PyMethodDef py_sems_methods[] = {
    {"log", (PyCFunction)py_sems_log, METH_VARARGS,"Log a message using Sems' logging system"},
    {"getHeader", (PyCFunction)py_sems_getHeader, METH_VARARGS,"Python getHeader wrapper"},
    {NULL}  /* Sentinel */
  };
}

PySemsFactory::PySemsFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    user_timer_fact(NULL)
{
}

// void PySemsFactory::setScriptPath(const string& path)
// {
//     string python_path = script_path = path;

//     if(python_path.length()){
// 	add_env_path("PYTHONPATH", python_path);
//     }

//     add_env_path("PYTHONPATH",AmConfig::PlugInPath);
// }

void PySemsFactory::import_object(PyObject* m, char* name, PyTypeObject* type)
{
  if (PyType_Ready(type) < 0){
    ERROR("PyType_Ready failed !\n");
    return;
  }
  Py_INCREF(type);
  PyModule_AddObject(m, name, (PyObject *)type);
}

void PySemsFactory::import_py_sems_builtins()
{
  // py_sems module - start
  PyImport_AddModule("py_sems");
  py_sems_module = Py_InitModule("py_sems",py_sems_methods);

  // PySemsAudioFile
  import_object(py_sems_module,"PySemsAudioFile",&PySemsAudioFileType);

  PyModule_AddIntConstant(py_sems_module, "AUDIO_READ",AUDIO_READ);
  PyModule_AddIntConstant(py_sems_module, "AUDIO_WRITE",AUDIO_WRITE);
  // py_sems module - end

  // add log level for the log module
  PyModule_AddIntConstant(py_sems_module, "SEMS_LOG_LEVEL",log_level);

  import_module("py_sems_log");
  initpy_sems_lib();
}

void PySemsFactory::set_sys_path(const string& script_path)
{
  PyObject* py_mod = import_module("sys");
  if(!py_mod)	return;

  PyObject* sys_path_str = PyString_FromString("path");
  PyObject* sys_path = PyObject_GetAttr(py_mod,sys_path_str);
  Py_DECREF(sys_path_str);

  if(!sys_path){
    PyErr_Print();
    Py_DECREF(py_mod);
    return;
  }

  if(!PyList_Insert(sys_path,0,PyString_FromString(script_path.c_str()))){
    PyErr_Print();
  }
}

PyObject* PySemsFactory::import_module(const char* modname)
{
  PyObject* py_mod_name = PyString_FromString(modname);
  PyObject* py_mod = PyImport_Import(py_mod_name);
  Py_DECREF(py_mod_name);
    
  if(!py_mod){
    PyErr_Print();
    ERROR("PySemsFactory: could not find python module '%s'.\n",modname);
    ERROR("PySemsFactory: please check your installation.\n");
    return NULL;
  }
    
  return py_mod;
}

void PySemsFactory::init_python_interpreter(const string& script_path)
{
  if(!Py_IsInitialized()){

    add_env_path("PYTHONPATH",AmConfig::PlugInPath);
    Py_Initialize();
  }

  PyEval_InitThreads();
  set_sys_path(script_path);
  import_py_sems_builtins();
  PyEval_ReleaseLock();
}

AmSession* PySemsFactory::newDlg(const string& name)
{
  PYLOCK;

  map<string,PySemsScriptDesc>::iterator mod_it = mod_reg.find(name);
  if(mod_it == mod_reg.end()){
    ERROR("Unknown script name '%s'\n", name.c_str());
    throw AmSession::Exception(500,"Unknown Application");
  }

  PySemsScriptDesc& mod_desc = mod_it->second;

  AmDynInvoke* user_timer = user_timer_fact->getInstance();
  if(!user_timer){
    ERROR("could not get a user timer reference\n");
    throw AmSession::Exception(500,"could not get a user timer reference");
  }
	
  PyObject* dlg_inst = PyObject_Call(mod_desc.dlg_class,PyTuple_New(0),NULL);
  if(!dlg_inst){
	
    PyErr_Print();
    ERROR("PySemsFactory: while loading \"%s\": could not create instance\n",
	  name.c_str());
    throw AmSession::Exception(500,"Internal error in PY_SEMS plug-in.");
	
    return NULL;
  }

  int err=0;

  AmSession* sess = NULL;
  PySemsDialogBase* dlg_base  = NULL;

  switch(mod_desc.dt) {
  case PySemsScriptDesc::None: {
    ERROR("wrong script type: None.\n");
  }; break;
  case PySemsScriptDesc::Dialog: {
    PySemsDialog* dlg = (PySemsDialog*)
      sipForceConvertTo_PySemsDialog(dlg_inst,&err);
    sess = dlg;
    dlg_base = dlg;
  }; break;

  case PySemsScriptDesc::B2BDialog: {
    PySemsB2BDialog* b2b_dlg = (PySemsB2BDialog*)
      sipForceConvertTo_PySemsB2BDialog(dlg_inst,&err);
    sess = b2b_dlg;
    dlg_base = b2b_dlg;
  }; break;

  case PySemsScriptDesc::B2ABDialog: {
    PySemsB2ABDialog* b2ab_dlg = (PySemsB2ABDialog*)
      sipForceConvertTo_PySemsB2ABDialog(dlg_inst,&err);      
    sess = b2ab_dlg;
    dlg_base = b2ab_dlg;

  }; break;
  }

  if (err || !dlg_base) {
    // no luck
    PyErr_Print();
    ERROR("PySemsFactory: while loading \"%s\": could not retrieve a PySems*Dialog ptr.\n",
	  name.c_str());
    throw AmSession::Exception(500,"Internal error in PY_SEMS plug-in.");
    Py_DECREF(dlg_inst);
    return NULL;
  }


  // take the ownership over dlg
  sipTransferTo(dlg_inst,dlg_inst);
  Py_DECREF(dlg_inst);
  dlg_base->setPyPtrs(NULL,dlg_inst);
  return sess;
}

bool PySemsFactory::loadScript(const string& path)
{
  PYLOCK;
    
  PyObject *modName,*mod,*dict, *dlg_class, *config=NULL;
  PySemsScriptDesc::DialogType dt = PySemsScriptDesc::None;


  modName = PyString_FromString(path.c_str());
  mod     = PyImport_Import(modName);

  AmConfigReader cfg;
  string cfg_file = add2path(AmConfig::ModConfigPath,1,(path + ".conf").c_str());

  Py_DECREF(modName);

  if(!mod){
    PyErr_Print();
    WARN("PySemsFactory: Failed to load \"%s\"\n", path.c_str());

    dict = PyImport_GetModuleDict();
    Py_INCREF(dict);
    PyDict_DelItemString(dict,path.c_str());
    Py_DECREF(dict);

    return false;
  }

  dict = PyModule_GetDict(mod);
  dlg_class = PyDict_GetItemString(dict, "PySemsScript");

  if(!dlg_class){

    PyErr_Print();
    WARN("PySemsFactory: class PySemsDialog not found in \"%s\"\n", path.c_str());
    goto error1;
  }

  Py_INCREF(dlg_class);
    
  if(PyObject_IsSubclass(dlg_class,(PyObject *)sipClass_PySemsDialog)) {
    dt = PySemsScriptDesc::Dialog;
    DBG("Loaded a Dialog Script.\n");
  } else if (PyObject_IsSubclass(dlg_class,(PyObject *)sipClass_PySemsB2BDialog)) {
    DBG("Loaded a B2BDialog Script.\n");
    dt = PySemsScriptDesc::B2BDialog;
  } else if (PyObject_IsSubclass(dlg_class,(PyObject *)sipClass_PySemsB2ABDialog)) {
    DBG("Loaded a B2ABDialog Script.\n");
    dt = PySemsScriptDesc::B2ABDialog;
  } else {
    WARN("PySemsFactory: in \"%s\": PySemsScript is not a "
	 "subtype of PySemsDialog\n", path.c_str());

    goto error2;
  }

  if(cfg.loadFile(cfg_file)){
    ERROR("could not load config file at %s\n",cfg_file.c_str());
    goto error2;
  }

  config = PyDict_New();
  if(!config){
    ERROR("could not allocate new dict for config\n");
    goto error2;
  }

  for(map<string,string>::const_iterator it = cfg.begin();
      it != cfg.end(); it++){
	
    PyDict_SetItem(config, 
		   PyString_FromString(it->first.c_str()),
		   PyString_FromString(it->second.c_str()));
  }

  PyObject_SetAttrString(mod,"config",config);

  mod_reg.insert(std::make_pair(path,
			        PySemsScriptDesc(mod,dlg_class, dt)));

  return true;

 error2:
  Py_DECREF(dlg_class);
 error1:
  Py_DECREF(mod);

  return false;
}

/**
 * Loads python script path and default script file from configuration file
 */
int PySemsFactory::onLoad()
{
  user_timer_fact = AmPlugIn::instance()->getFactory4Di("user_timer");
  if(!user_timer_fact){
	
    ERROR("could not load user_timer from session_timer plug-in\n");
    return -1;
  }

  AmConfigReader cfg;

  if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1,MOD_NAME ".conf")))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  string script_path = cfg.getParameter("script_path");
  init_python_interpreter(script_path);

#ifdef PY_SEMS_WITH_TTS
  DBG("** PY_SEMS Text-To-Speech enabled\n");
#else
  DBG("** PY_SEMS Text-To-Speech disabled\n");
#endif

  DBG("** PY_SEMS script path: \'%s\'\n", script_path.c_str());

  regex_t reg;
  if(regcomp(&reg,PYFILE_REGEX,REG_EXTENDED)){
    ERROR("while compiling regular expression\n");
    return -1;
  }

  DIR* dir = opendir(script_path.c_str());
  if(!dir){
    regfree(&reg);
    ERROR("PySems: script pre-loader (%s): %s\n",
	  script_path.c_str(),strerror(errno));
    return -1;
  }

  DBG("directory '%s' opened\n",script_path.c_str());

  set<string> unique_entries;
  regmatch_t  pmatch[2];

  struct dirent* entry=0;
  while((entry = readdir(dir)) != NULL){

    if(!regexec(&reg,entry->d_name,2,pmatch,0)){

      string name(entry->d_name + pmatch[1].rm_so,
		  pmatch[1].rm_eo - pmatch[1].rm_so);

      unique_entries.insert(name);
    }
  }
  closedir(dir);
  regfree(&reg);

  AmPlugIn* plugin = AmPlugIn::instance();
  for(set<string>::iterator it = unique_entries.begin();
      it != unique_entries.end(); it++) {

    if(loadScript(*it)){
      bool res = plugin->registerFactory4App(*it,this);
      if(res)
	INFO("Application script registered: %s.\n",
	     it->c_str());
    }
  }

  return 0; // don't stop sems from starting up
}

/**
 * Load a script using user name from URI.
 * Note: there is no default script.
 */
AmSession* PySemsFactory::onInvite(const AmSipRequest& req)
{
  if(req.cmd != MOD_NAME)
    return newDlg(req.cmd);
  else
    return newDlg(req.user);
}

// PySemsDialogBase - base class for all possible PySemsDialog classes
PySemsDialogBase::PySemsDialogBase() 
  :  py_mod(NULL), 
     py_dlg(NULL)
{
}

PySemsDialogBase::~PySemsDialogBase() {
  PYLOCK;
  Py_XDECREF(py_dlg);
}

void PySemsDialogBase::setPyPtrs(PyObject *mod, PyObject *dlg)
{
  PYLOCK;
  Py_XDECREF(py_dlg);
  py_dlg = dlg;
}

bool PySemsDialogBase::callPyEventHandler(char* name, char* fmt, ...)
{
  bool ret=false;
  va_list va;

  PYLOCK;

  va_start(va, fmt);
  PyObject* o = PyObject_VaCallMethod(py_dlg,name,fmt,va);
  va_end(va);

  if(!o) {

    if(PyErr_ExceptionMatches(PyExc_AttributeError)){

      DBG("method %s is not implemented, trying default one\n",name);
      return true;
    }

    PyErr_Print();
  }
  else {
    if(o && PyBool_Check(o) && (o == Py_True)) {

      ret = true;
    }

    Py_DECREF(o);
  }
    
  return ret;
}
