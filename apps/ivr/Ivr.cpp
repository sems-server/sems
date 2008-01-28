/*
 * $Id$
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

#include "IvrDialogBase.h"
#include "IvrSipDialog.h"
#include "IvrSipRequest.h"
#include "IvrSipReply.h"
#include "IvrAudio.h"
#include "IvrAudioMixIn.h"
#include "IvrUAC.h"
#include "Ivr.h"

#include "AmSessionContainer.h"

#include "AmConfigReader.h"
#include "AmConfig.h"
#include "log.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "AmPlugIn.h"

#include <unistd.h>
#include <pthread.h>
#include <regex.h>
#include <dirent.h>

#include <set>
using std::set;


#define PYFILE_REGEX "(.+)\\.(py|pyc|pyo)$"


EXPORT_SESSION_FACTORY(IvrFactory,MOD_NAME);


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

  static PyObject* ivr_log(PyObject*, PyObject* args)
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

  static PyObject* ivr_getHeader(PyObject*, PyObject* args)
  {
    char* headers;
    char* header_name;
    if(!PyArg_ParseTuple(args,"ss",&headers,&header_name))
      return NULL;

    string res = getHeader(headers,header_name);
    return PyString_FromString(res.c_str());
  }

  static PyObject* ivr_getSessionParam(PyObject*, PyObject* args)
  {
    char* headers;
    char* header_name;
    if(!PyArg_ParseTuple(args,"ss",&headers,&header_name))
      return NULL;

    string res;
    string iptel_app_param = getHeader(headers, PARAM_HDR);
    if (iptel_app_param.length()) {
      res = get_header_keyvalue(iptel_app_param,header_name);
    } else {
      INFO("Use of P-%s is deprecated. \n", header_name);
      INFO("Use '%s: %s=<addr>' instead.\n", PARAM_HDR, header_name);

      res = getHeader(headers,string("P-") + header_name);
    }

	 
    return PyString_FromString(res.c_str());
  }

  static PyObject* ivr_createThread(PyObject*, PyObject* args)
  {
    PyObject* py_thread_object = NULL;

    if(!PyArg_ParseTuple(args,"O",&py_thread_object))
      return NULL;

    IvrFactory* pIvrFactory = NULL;
    PyObject *module = PyImport_ImportModule(MOD_NAME);
    if (module != NULL) {
      PyObject *ivrFactory = PyObject_GetAttrString(module, "__c_ivrFactory");
      if (ivrFactory != NULL){
	if (PyCObject_Check(ivrFactory))
	  pIvrFactory = (IvrFactory*)PyCObject_AsVoidPtr(ivrFactory);
	Py_DECREF(ivrFactory);
      }
    }
    if (pIvrFactory) 
      pIvrFactory->addDeferredThread(py_thread_object);
    else 
      ERROR("Could not find __c_ivrFactory in Python state.\n");

    return Py_None;
  }

  static PyMethodDef ivr_methods[] = {
    {"log", (PyCFunction)ivr_log, METH_VARARGS,"Log a message using Sems' logging system"},
    {"getHeader", (PyCFunction)ivr_getHeader, METH_VARARGS,"Python getHeader wrapper"},
    {"getSessionParam", (PyCFunction)ivr_getSessionParam, METH_VARARGS,"Python getSessionParam wrapper"},
    {"createThread", (PyCFunction)ivr_createThread, METH_VARARGS, "Create another interpreter thread"},
    {NULL}  /* Sentinel */
  };
}

void PythonScriptThread::run() {
  PYLOCK;
  DBG("PythonScriptThread - calling python function.\n");
  PyObject_CallObject(py_thread_object, NULL);
  DBG("PythonScriptThread - thread finished..\n");
}

void PythonScriptThread::on_stop() {
  DBG("PythonScriptThread::on_stop.\n");
}

IvrFactory::IvrFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    user_timer_fact(NULL)
{
}

// void IvrFactory::setScriptPath(const string& path)
// {
//     string python_path = script_path = path;

    
//     if(python_path.length()){

// 	python_path = AmConfig::PlugInPath + ":" + python_path;
//     }
//     else
// 	python_path = AmConfig::PlugInPath;

//     char* old_path=0;
//     if((old_path = getenv("PYTHONPATH")) != 0)
// 	if(strlen(old_path))
// 	    python_path += ":" + string(old_path);

//     DBG("setting PYTHONPATH to: '%s'\n",python_path.c_str());
//     setenv("PYTHONPATH",python_path.c_str(),1);

// }

void IvrFactory::import_object(PyObject* m, char* name, PyTypeObject* type)
{
  if (PyType_Ready(type) < 0){
    ERROR("PyType_Ready failed !\n");
    return;
  }
  Py_INCREF(type);
  PyModule_AddObject(m, name, (PyObject *)type);
}

void IvrFactory::import_ivr_builtins()
{
  // ivr module - start
  PyImport_AddModule("ivr");
  ivr_module = Py_InitModule("ivr",ivr_methods);

  PyObject* pIvrFactory = PyCObject_FromVoidPtr((void*)this,NULL);
  if (pIvrFactory != NULL)
    PyModule_AddObject(ivr_module, "__c_ivrFactory", pIvrFactory);

  // IvrSipDialog (= AmSipDialog)
  import_object(ivr_module, "IvrSipDialog", &IvrSipDialogType);

  // IvrDialogBase
  import_object(ivr_module,"IvrDialogBase",&IvrDialogBaseType);

  // IvrSipRequest
  import_object(ivr_module,"IvrSipRequest",&IvrSipRequestType);

  // IvrSipReply
  import_object(ivr_module,"IvrSipReply",&IvrSipReplyType);

  // IvrAudioFile
  import_object(ivr_module,"IvrAudioFile",&IvrAudioFileType);

  // IvrAudioMixIn
  import_object(ivr_module,"IvrAudioMixIn",&IvrAudioMixInType);

  // IvrUAC
  import_object(ivr_module,"IvrUAC",&IvrUACType);

  PyModule_AddIntConstant(ivr_module, "AUDIO_READ",AUDIO_READ);
  PyModule_AddIntConstant(ivr_module, "AUDIO_WRITE",AUDIO_WRITE);

  // add log level for the log module
  PyModule_AddIntConstant(ivr_module, "SEMS_LOG_LEVEL",log_level);

  PyObject* log_mod_name = PyString_FromString("log");
  PyObject* log_mod = PyImport_Import(log_mod_name);
  Py_DECREF(log_mod_name);

  if(!log_mod){
    PyErr_Print();
    ERROR("IvrFactory: could not find the log python module.\n");
    ERROR("IvrFactory: please check your installation.\n");
    return;
  }
}

void IvrFactory::init_python_interpreter(const string& script_path)
{
  if(!Py_IsInitialized()){

    add_env_path("PYTHONPATH",AmConfig::PlugInPath);
    Py_Initialize();
  }

  PyEval_InitThreads();
  set_sys_path(script_path);
  import_ivr_builtins();
  PyEval_ReleaseLock();
}

void IvrFactory::set_sys_path(const string& script_path)
{

  PyObject* py_mod_name = PyString_FromString("sys");
  PyObject* py_mod = PyImport_Import(py_mod_name);
  Py_DECREF(py_mod_name);
    
  if(!py_mod){
    PyErr_Print();
    ERROR("IvrFactory: could not import 'sys' module.\n");
    ERROR("IvrFactory: please check your installation.\n");
    return;
  }

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

IvrDialog* IvrFactory::newDlg(const string& name)
{
  PYLOCK;

  map<string,IvrScriptDesc>::iterator mod_it = mod_reg.find(name);
  if(mod_it == mod_reg.end()){
    ERROR("Unknown script name '%s'\n", name.c_str());
    throw AmSession::Exception(500,"Unknown Application");
  }

  IvrScriptDesc& mod_desc = mod_it->second;

  AmDynInvoke* user_timer = user_timer_fact->getInstance();
  if(!user_timer){
    ERROR("could not get a user timer reference\n");
    throw AmSession::Exception(500,"could not get a user timer reference");
  }
	

  IvrDialog* dlg = new IvrDialog(user_timer);

  PyObject* c_dlg = PyCObject_FromVoidPtr(dlg,NULL);
  PyObject* dlg_inst = PyObject_CallMethod(mod_desc.dlg_class,"__new__","OO",
					   mod_desc.dlg_class,c_dlg);
  Py_DECREF(c_dlg);

  if(!dlg_inst){

    delete dlg;

    PyErr_Print();
    ERROR("IvrFactory: while loading \"%s\": could not create instance\n",
	  name.c_str());
    throw AmSession::Exception(500,"Internal error in IVR plug-in.\n");

    return NULL;
  }    

  dlg->setPyPtrs(mod_desc.mod,dlg_inst);
  Py_DECREF(dlg_inst);

  return dlg;
}

bool IvrFactory::loadScript(const string& path)
{
  PYLOCK;
    
  PyObject *modName=NULL,*mod=NULL,*dict=NULL,*dlg_class=NULL,*config=NULL;

  // load module configuration
  AmConfigReader cfg;
  string cfg_file = add2path(AmConfig::ModConfigPath,1,(path + ".conf").c_str());
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

  // set config ivr ivr_module while loading
  Py_INCREF(config);
  PyObject_SetAttrString(ivr_module,"config",config);

  // load module
  modName = PyString_FromString(path.c_str());

  mod     = PyImport_Import(modName);
  Py_DECREF(modName);

  // remove config ivr ivr_module while loading
  PyObject_DelAttrString(ivr_module, "config");
  Py_DECREF(config);

  if(!mod){
    PyErr_Print();
    WARN("IvrFactory: Failed to load \"%s\"\n", path.c_str());

    dict = PyImport_GetModuleDict();
    Py_INCREF(dict);
    PyDict_DelItemString(dict,path.c_str());
    Py_DECREF(dict);

    return false;
  }

  dict = PyModule_GetDict(mod);
  dlg_class = PyDict_GetItemString(dict, "IvrDialog");

  if(!dlg_class){

    PyErr_Print();
    WARN("IvrFactory: class IvrDialog not found in \"%s\"\n", path.c_str());
    goto error1;
  }

  Py_INCREF(dlg_class);

  if(!PyObject_IsSubclass(dlg_class,(PyObject*)&IvrDialogBaseType)){

    WARN("IvrFactory: in \"%s\": IvrDialog is not a subtype of IvrDialogBase\n",
	 path.c_str());
    goto error2;
  }

  PyObject_SetAttrString(mod,"config",config);

  mod_reg.insert(std::make_pair(path,
			        IvrScriptDesc(mod,dlg_class)));

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
int IvrFactory::onLoad()
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

  //setScriptPath(cfg.getParameter("script_path"));
  string script_path = cfg.getParameter("script_path");
  init_python_interpreter(script_path);

  DBG("** IVR compile time configuration:\n");
  DBG("**     built with PYTHON support.\n");

#ifdef IVR_WITH_TTS
  DBG("**     Text-To-Speech enabled\n");
#else
  DBG("**     Text-To-Speech disabled\n");
#endif

  DBG("** IVR run time configuration:\n");
  DBG("**     script path:         \'%s\'\n", script_path.c_str());

  regex_t reg;
  if(regcomp(&reg,PYFILE_REGEX,REG_EXTENDED)){
    ERROR("while compiling regular expression\n");
    return -1;
  }

  DIR* dir = opendir(script_path.c_str());
  if(!dir){
    regfree(&reg);
    ERROR("Ivr: script pre-loader (%s): %s\n",
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

  start_deferred_threads();

  return 0; // don't stop sems from starting up
}

void IvrFactory::addDeferredThread(PyObject* pyCallable) {
  deferred_threads.push(pyCallable);
}

void IvrFactory::start_deferred_threads() {
  if (!deferred_threads.empty()) {
    while (!deferred_threads.empty()) {
      PythonScriptThread* t = new PythonScriptThread(deferred_threads.front());
      deferred_threads.pop();
      t->start();
      AmThreadWatcher::instance()->add(t);
    }
  }
}


int IvrDialog::transfer(const string& target)
{
  return dlg.transfer(target);
}

int IvrDialog::drop()
{
  int res = dlg.drop();
  if (res) 
    setStopped();
	
  return res;
}

/**
 * Load a script using user name from URI.
 * Note: there is no default script.
 */
AmSession* IvrFactory::onInvite(const AmSipRequest& req)
{
  if(req.cmd != MOD_NAME)
    return newDlg(req.cmd);
  else
    return newDlg(req.user);
}

IvrDialog::IvrDialog(AmDynInvoke* user_timer)
  : py_mod(NULL), 
    py_dlg(NULL),
    playlist(this),
    user_timer(user_timer)
{
  sip_relay_only = false;
}

IvrDialog::~IvrDialog()
{
  DBG("----------- IvrDialog::~IvrDialog() ------------- \n");

  playlist.close(false);
  
  PYLOCK;
  Py_XDECREF(py_mod);
  Py_XDECREF(py_dlg);
}

void IvrDialog::setPyPtrs(PyObject *mod, PyObject *dlg)
{
  assert(py_mod = mod);
  assert(py_dlg = dlg);
  Py_INCREF(py_mod);
  Py_INCREF(py_dlg);
}

static PyObject *
type_error(const char *msg)
{
  PyErr_SetString(PyExc_TypeError, msg);
  return NULL;
}

static PyObject *
null_error(void)
{
  if (!PyErr_Occurred())
    PyErr_SetString(PyExc_SystemError,
		    "null argument to internal routine");
  return NULL;
}

PyObject *
PyObject_VaCallMethod(PyObject *o, char *name, char *format, va_list va)
{
  PyObject *args, *func = 0, *retval;

  if (o == NULL || name == NULL)
    return null_error();

  func = PyObject_GetAttrString(o, name);
  if (func == NULL) {
    PyErr_SetString(PyExc_AttributeError, name);
    return 0;
  }

  if (!PyCallable_Check(func))
    return type_error("call of non-callable attribute");

  if (format && *format) {
    args = Py_VaBuildValue(format, va);
  }
  else
    args = PyTuple_New(0);

  if (!args)
    return NULL;

  if (!PyTuple_Check(args)) {
    PyObject *a;

    a = PyTuple_New(1);
    if (a == NULL)
      return NULL;
    if (PyTuple_SetItem(a, 0, args) < 0)
      return NULL;
    args = a;
  }

  retval = PyObject_Call(func, args, NULL);

  Py_DECREF(args);
  Py_DECREF(func);

  return retval;
}

bool IvrDialog::callPyEventHandler(char* name, char* fmt, ...)
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

void IvrDialog::onSessionStart(const AmSipRequest& req)
{
  callPyEventHandler("onSessionStart","s",req.hdrs.c_str());
  setInOut(&playlist,&playlist);
  AmB2BCallerSession::onSessionStart(req);
}

void IvrDialog::onBye(const AmSipRequest& req)
{
  if(callPyEventHandler("onBye",NULL))
    AmB2BSession::onBye(req);
}

void IvrDialog::onDtmf(int event, int duration_msec)
{
  if(callPyEventHandler("onDtmf","ii",event,duration_msec))
    AmB2BSession::onDtmf(event,duration_msec);
}

void IvrDialog::onOtherBye(const AmSipRequest& req)
{
  if(callPyEventHandler("onOtherBye",NULL))
    AmB2BSession::onOtherBye(req);
}

bool IvrDialog::onOtherReply(const AmSipReply& r)
{
  if(callPyEventHandler("onOtherReply","is",
			r.code,r.reason.c_str()))
    AmB2BSession::onOtherReply(r);
  return false;
}

void IvrDialog::onSipReply(const AmSipReply& r) {
  AmSipReply* rep_cpy = new AmSipReply(r);
  callPyEventHandler("onSipReply","O",IvrSipReply_FromPtr(rep_cpy));
  AmB2BSession::onSipReply(r);
}

void IvrDialog::onSipRequest(const AmSipRequest& r){
  AmSipRequest* req_cpy = new AmSipRequest(r);
  callPyEventHandler("onSipRequest","O", IvrSipRequest_FromPtr(req_cpy));
  AmB2BSession::onSipRequest(r);
}

void IvrDialog::onRtpTimeout() {
  callPyEventHandler("onRtpTimeout",NULL);
}

void IvrDialog::process(AmEvent* event) 
{
  DBG("IvrDialog::process\n");

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && audio_event->event_id == AmAudioEvent::noAudio){

    callPyEventHandler("onEmptyQueue", NULL);
    event->processed = true;
  }
    
  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {

    callPyEventHandler("onTimer", "i", plugin_event->data.get(0).asInt());
    event->processed = true;
  }

  if (!event->processed)
    AmB2BCallerSession::process(event);

  return;
}


void IvrDialog::connectCallee(const string& remote_party, const string& remote_uri,
			      const string& from_party, const string& from_uri) {
  b2b_callee_from_party = from_party;
  b2b_callee_from_uri  = from_uri;
  AmB2BCallerSession::connectCallee(remote_party, remote_uri);
}

void IvrDialog::createCalleeSession()
{
  AmB2BCalleeSession* callee_session = new AmB2BCalleeSession(this);
  AmSipDialog& callee_dlg = callee_session->dlg;
  
  other_id = AmSession::getNewId();
  
  callee_dlg.local_tag    = other_id;
  callee_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;
  
  // this will be overwritten by ConnectLeg event 
  callee_dlg.remote_party = dlg.local_party;
  callee_dlg.remote_uri   = dlg.local_uri;

  if (b2b_callee_from_party.empty() && b2b_callee_from_uri.empty()) {
    // default: use the original To as From in the callee leg
    callee_dlg.local_party  = dlg.remote_party;
    callee_dlg.local_uri  = dlg.remote_uri;
  } else {
    // if given as parameters, use these
    callee_dlg.local_party  = b2b_callee_from_party;
    callee_dlg.local_uri  = b2b_callee_from_uri;
  }
  
  DBG("Created B2BUA callee leg, From: %s\n",
      callee_dlg.local_party.c_str());

  callee_session->start();
  
  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}
