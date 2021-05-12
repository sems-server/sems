/* Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
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
#include "IvrNullAudio.h"
#include "IvrUAC.h"
#include "Ivr.h"
#include "IvrEvent.h"

#include "AmSessionContainer.h"

#include "AmConfigReader.h"
#include "AmConfig.h"
#include "log.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmEventDispatcher.h"

#ifdef USE_MONITORING
#include "ampi/MonitoringAPI.h"
#include "AmSessionContainer.h"
#endif

#include <unistd.h>
#include <pthread.h>
#include <regex.h>
#include <dirent.h>

#include <set>
#include <string>
using std::set;
using std::string;


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

  PyObject* SemsError;

  static PyObject* ivr_log(PyObject*, PyObject* args)
  {
    int level;
    char *msg;

    if(!PyArg_ParseTuple(args,"is",&level,&msg))
      return NULL;

    _LOG(level, "%s", msg);
	
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* ivr_getHeader(PyObject*, PyObject* args)
  {
    char* headers;
    char* header_name;
    if(!PyArg_ParseTuple(args,"ss",&headers,&header_name))
      return NULL;

    string res = getHeader(headers,header_name, true);
    return PyUnicode_FromString(res.c_str());
  }

  static PyObject* ivr_getHeaders(PyObject*, PyObject* args)
  {
    char* headers;
    char* header_name;
    if(!PyArg_ParseTuple(args,"ss",&headers,&header_name))
      return NULL;

    string res = getHeader(headers,header_name);
    return PyUnicode_FromString(res.c_str());
  }

  static PyObject* ivr_ignoreSigchld(PyObject*, PyObject* args)
  {
    int* ignore;
    if(!PyArg_ParseTuple(args,"i",&ignore))
      return NULL;

    AmConfig::IgnoreSIGCHLD = ignore;
    DBG("%sgnoring SIGCHLD.\n", ignore?"I":"Not i");

    Py_INCREF(Py_None);
    return Py_None;
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
	if (PyCapsule_CheckExact(ivrFactory))
	  pIvrFactory = (IvrFactory*)PyCapsule_GetPointer(ivrFactory, "ivrFactory");
	Py_DECREF(ivrFactory);
      }
    }
    if (pIvrFactory) 
      pIvrFactory->addDeferredThread(py_thread_object);
    else 
      ERROR("Could not find __c_ivrFactory in Python state.\n");

    Py_INCREF(Py_None);
    return Py_None;
  }

  // Log a line in the monitoring log
  static PyObject* ivr_monitorLog(PyObject* self, PyObject* args)
  {
#ifdef USE_MONITORING
    char *callid;
    char *property;
    char *value;
    if(!PyArg_ParseTuple(args, "sss", &callid, &property, &value))
      return NULL;

    try {
      AmArg di_args,ret;
      di_args.push(AmArg(callid));
      di_args.push(AmArg(property));
      di_args.push(AmArg(value));
      AmSessionContainer::monitoring_di->invoke("log", di_args, ret);
    }
    catch(...) {}
#endif
    Py_INCREF(Py_None);
    return Py_None;
  }

  // Add a log line to the monitoring log
  static PyObject* ivr_monitorLogAdd(PyObject* self, PyObject* args)
  {
#ifdef USE_MONITORING
    char *callid;
    char *property;
    char *value;
    if(!PyArg_ParseTuple(args, "sss", &callid, &property, &value))
      return NULL;

    try {
      AmArg di_args,ret;
      di_args.push(AmArg(callid));
      di_args.push(AmArg(property));
      di_args.push(AmArg(value));
      AmSessionContainer::monitoring_di->invoke("logAdd", di_args, ret);
    }
    catch(...) {}
#endif
    Py_INCREF(Py_None);
    return Py_None;
  }

  // Mark the session finished in the monitoring log
  static PyObject* ivr_monitorFinish(PyObject* self, PyObject* args)
  {
#ifdef USE_MONITORING
    char *callid;
    if(!PyArg_ParseTuple(args, "s", &callid))
      return NULL;

    try {
      AmArg di_args,ret;
      di_args.push(AmArg(callid));
      AmSessionContainer::monitoring_di->invoke("markFinished", di_args, ret);
    }
    catch(...) {}
#endif
    Py_INCREF(Py_None);
    return Py_None;
  }

  // Send inter-session message
  static PyObject* ivr_sendMessage(PyObject* self, PyObject* args)
  {
    char *dest;
    char *msg;
    if(!PyArg_ParseTuple(args, "ss", &dest, &msg))
      return NULL;

    AmEventDispatcher::instance()->post(dest, new IvrEvent(msg));
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyMethodDef ivr_methods[] = {
    {"log", (PyCFunction)ivr_log, METH_VARARGS,"Log a message using Sems' logging system"},
    {"getHeader", (PyCFunction)ivr_getHeader, METH_VARARGS,"Python getHeader wrapper"},
    {"getHeaders", (PyCFunction)ivr_getHeaders, METH_VARARGS,"Python getHeaders wrapper"},
    {"createThread", (PyCFunction)ivr_createThread, METH_VARARGS, "Create another interpreter thread"},
    {"setIgnoreSigchld", (PyCFunction)ivr_ignoreSigchld, METH_VARARGS, "ignore SIGCHLD signal"},

    // Log a line in the monitoring log
    {"monitorLog",  (PyCFunction)ivr_monitorLog, METH_VARARGS,
     "log a line in the monitoring log"
    },
    // Add a log line to the monitoring log
    {"monitorLogAdd",  (PyCFunction)ivr_monitorLogAdd, METH_VARARGS,
     "add a log line to the monitoring log"
    },
    // Mark the session finished in the monitoring log
    {"monitorFinish",  (PyCFunction)ivr_monitorFinish, METH_VARARGS,
     "mark the session finished in the monitoring log"
    },

    // Send inter-session message
    {"sendMessage", (PyCFunction)ivr_sendMessage, METH_VARARGS,
     "send inter-session message"
    },

    {NULL}  /* Sentinel */
  };

  static struct PyModuleDef ivrmoduledef = {
        PyModuleDef_HEAD_INIT,
        "ivr",
        NULL,
        -1,
        ivr_methods,
        NULL,
        NULL,
        NULL,
        NULL
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
  : AmSessionFactory(_app_name)
{
}

AmConfigReader IvrFactory::cfg;
AmSessionEventHandlerFactory* IvrFactory::session_timer_f = NULL;

void IvrFactory::import_object(PyObject* m, const char* name, PyTypeObject* type)
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
  PyObject* m = PyImport_GetModuleDict();
  PyObject* nameobj = PyUnicode_FromString("ivr");
  ivr_module = PyModule_Create(&ivrmoduledef);
  PyObject_SetItem(m, nameobj, ivr_module);
  Py_DECREF(nameobj);

  PyObject* pIvrFactory = PyCapsule_New((void*)this, "ivrFactory", NULL);
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

  // IvrNullAudio
  import_object(ivr_module,"IvrNullAudio",&IvrNullAudioType);

  // IvrUAC
  import_object(ivr_module,"IvrUAC",&IvrUACType);

  PyModule_AddIntConstant(ivr_module, "AUDIO_READ",AUDIO_READ);
  PyModule_AddIntConstant(ivr_module, "AUDIO_WRITE",AUDIO_WRITE);

  PyModule_AddStringConstant(ivr_module, "LOCAL_SIP_IP", AmConfig::SIP_Ifs[0].LocalIP.c_str());
  PyModule_AddIntConstant(ivr_module, "LOCAL_SIP_PORT", AmConfig::SIP_Ifs[0].LocalPort);

  // add exception to handle the AmSession's exception
  SemsError = PyErr_NewException("ivr.semserror", NULL, NULL);
  Py_INCREF(SemsError);
  PyModule_AddObject(ivr_module, "semserror", SemsError);

  // add log level for the log module
  PyModule_AddIntConstant(ivr_module, "SEMS_LOG_LEVEL",log_level);

  PyObject* log_mod_name = PyUnicode_FromString("log");
  PyObject* log_mod = PyImport_Import(log_mod_name);
  Py_DECREF(log_mod_name);

  if(!log_mod){
    PyErr_PrintEx(0);
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
  PyEval_SaveThread();
}

void IvrFactory::set_sys_path(const string& script_path)
{

  PyObject* py_mod_name = PyUnicode_FromString("sys");
  PyObject* py_mod = PyImport_Import(py_mod_name);
  Py_DECREF(py_mod_name);
    
  if(!py_mod){
    PyErr_PrintEx(0);
    ERROR("IvrFactory: could not import 'sys' module.\n");
    ERROR("IvrFactory: please check your installation.\n");
    return;
  }

  PyObject* sys_path_str = PyUnicode_FromString("path");
  PyObject* sys_path = PyObject_GetAttr(py_mod,sys_path_str);
  Py_DECREF(sys_path_str);

  if(!sys_path){
    PyErr_PrintEx(0);
    Py_DECREF(py_mod);
    return;
  }

  PyObject* script_path_str = PyUnicode_FromString(script_path.c_str());
  if(!PyList_Insert(sys_path, 0, script_path_str)){
    PyErr_PrintEx(0);
  }
  Py_DECREF(script_path_str);
}

IvrDialog* IvrFactory::newDlg(const string& name, AmArg* session_params)
{
  PYLOCK;

  map<string,IvrScriptDesc>::iterator mod_it = mod_reg.find(name);
  if(mod_it == mod_reg.end()){
    ERROR("Unknown script name '%s'\n", name.c_str());
    throw AmSession::Exception(500,"Unknown Application");
  }

  IvrScriptDesc& mod_desc = mod_it->second;

  IvrDialog* dlg = new IvrDialog();

  PyObject* c_dlg = PyCapsule_New(dlg, "IvrDialog", NULL);
  PyObject* dlg_inst = PyObject_CallMethod(mod_desc.dlg_class,
					   (char*)"__new__",(char*)"OO",
					   mod_desc.dlg_class,c_dlg);
  Py_DECREF(c_dlg);

  if(!dlg_inst){

    delete dlg;

    PyErr_PrintEx(0);
    ERROR("IvrFactory: while loading \"%s\": could not create instance\n",
	  name.c_str());
    throw AmSession::Exception(500,"Internal error in IVR plug-in.\n");

    return NULL;
  }    

  dlg->setPyPtrs(mod_desc.mod, dlg_inst, session_params);
  Py_DECREF(dlg_inst);

  setupSessionTimer(dlg);
  return dlg;
}

bool IvrFactory::loadScript(const string& path)
{
  PYLOCK;
    
  PyObject *modName=NULL,*mod=NULL,*dict=NULL,*dlg_class=NULL,*config=NULL;

  // load module configuration
  AmConfigReader cfg;
  string cfg_file = add2path(AmConfig::ModConfigPath,1,(path + ".conf").c_str());
  config = PyDict_New();
  if(!config){
    ERROR("could not allocate new dict for config\n");
    goto error2;
  }

  if(cfg.loadFile(cfg_file)){
    WARN("could not load config file at %s\n", cfg_file.c_str());
  } else {
    for(map<string,string>::const_iterator it = cfg.begin();
	it != cfg.end(); it++){
      PyObject *f = PyUnicode_FromString(it->first.c_str());
      PyObject *s = PyUnicode_FromString(it->second.c_str());
      PyDict_SetItem(config, f, s);
      Py_DECREF(f);
      Py_DECREF(s);
    }
  }

  // set config ivr ivr_module while loading
  Py_INCREF(config);
  PyObject_SetAttrString(ivr_module, "config", config);
  
  // load module
  modName = PyUnicode_FromString(path.c_str());
  
  mod     = PyImport_Import(modName);
  if (NULL != config) {
    // remove config ivr ivr_module while loading
    PyObject_DelAttrString(ivr_module, "config");
    Py_DECREF(config);
  }
    
  if(!mod){
    PyErr_PrintEx(0);
    WARN("IvrFactory: Failed to load \"%s\"\n", path.c_str());

    // before python 2.4,
    // it can happen that the module
    // is still in the dictionnary.
    dict = PyImport_GetModuleDict();
    Py_INCREF(dict);
    if(PyDict_Contains(dict, modName)){
      PyDict_DelItem(dict, modName);
    }
    Py_DECREF(dict);
    Py_DECREF(modName);

    return false;
  }

  Py_DECREF(modName);
  dict = PyModule_GetDict(mod);
  dlg_class = PyDict_GetItemString(dict, "IvrDialog");

  if(!dlg_class){

    PyErr_PrintEx(0);
    WARN("IvrFactory: class IvrDialog not found in \"%s\"\n", path.c_str());
    goto error1;
  }

  Py_INCREF(dlg_class);

  if(!PyObject_IsSubclass(dlg_class,(PyObject*)&IvrDialogBaseType)){

    WARN("IvrFactory: in \"%s\": IvrDialog is not a subtype of IvrDialogBase\n",
	 path.c_str());
    goto error2;
  }

  PyObject_SetAttrString(mod, "config", config);

  mod_reg.insert(std::make_pair(path,
			        IvrScriptDesc(mod, dlg_class)));

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
  if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1,MOD_NAME ".conf")))
    return -1;

#ifdef USE_MONITORING
  if(!AmPlugIn::instance()->getFactory4Di("monitoring")) {
    ERROR("Monitoring plugin not available, bailing!\n");
    return -1;
  }
#endif

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

  std::set<string> unique_entries;
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
  for(std::set<string>::iterator it = unique_entries.begin();
      it != unique_entries.end(); it++) {

    if(loadScript(*it)){
      bool res = plugin->registerFactory4App(*it,this);
      if(res)
	INFO("Application script registered: %s.\n",
	     it->c_str());
    }
  }

  if(cfg.hasParameter("enable_session_timer") &&
     (cfg.getParameter("enable_session_timer") == string("yes")) ){
    DBG("enabling session timers\n");
    session_timer_f = AmPlugIn::instance()->getFactory4Seh("session_timer");
    if(session_timer_f == NULL){
      ERROR("Could not load the session_timer module: disabling session timers.\n");
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

int IvrDialog::drop()
{
  int res = dlg->drop();
  if (res) 
    setStopped();
	
  return res;
}

void IvrFactory::setupSessionTimer(AmSession* s) {
  if (NULL != session_timer_f) {

    AmSessionEventHandler* h = session_timer_f->getHandler(s);
    if (NULL == h)
      return;

    if(h->configure(cfg)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      s->addHandler(h);
    }
  }
}

/**
 * Load a script using the app_name.
 * Note: there is no default script.
 */
AmSession* IvrFactory::onInvite(const AmSipRequest& req, const string& app_name,
				const map<string,string>& app_params)
{
  DBG("IvrFactory::onInvite\n");

  return newDlg(app_name);
}

AmSession* IvrFactory::onInvite(const AmSipRequest& req, const string& app_name,
            AmArg& session_params)
{
  DBG("IvrFactory::onInvite UAC\n");

  return newDlg(app_name, &session_params);
}


IvrDialog::IvrDialog()
  : py_mod(NULL), 
    py_dlg(NULL),
    playlist(this),
    session_params(NULL)
{
  set_sip_relay_only(false);
}

IvrDialog::~IvrDialog()
{
  DBG("----------- IvrDialog::~IvrDialog() ------------- \n");

  if(session_params) delete session_params;

  playlist.flush();
  
  PYLOCK;
  Py_XDECREF(py_mod);
  Py_XDECREF(py_dlg);
}

void IvrDialog::setPyPtrs(PyObject *mod, PyObject *dlg, AmArg *sp)
{
  assert(py_mod = mod);
  assert(py_dlg = dlg);
  Py_INCREF(py_mod);
  Py_INCREF(py_dlg);

  session_params = sp;
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

    DBG("method %s is not implemented, "
	"trying default one (params: '%s')\n",
	name,format);

    Py_RETURN_TRUE;
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

bool IvrDialog::callPyEventHandler(const char* name, const char* fmt, ...)
{
  bool ret=false;
  va_list va;

  PYLOCK;

  va_start(va, fmt);
  PyObject* o = PyObject_VaCallMethod(py_dlg,(char*)name,(char*)fmt,va);
  va_end(va);

  if(!o) {
    if(PyErr_Occurred()) {
      PyObject* type;
      PyObject* value;
      PyObject* tb;
      PyErr_Fetch(&type, &value, &tb);
      if(PyErr_GivenExceptionMatches(type, SemsError)) {
        PyObject* args;
        PyObject* dict;
        bool error = false;
        if(PyDict_Check(value)) {
          dict = value;
        } else if((args = PyObject_GetAttrString(value, "args")))
        {
          Py_ssize_t s = PyTuple_Size(args);
          bool found = false;
          for(Py_ssize_t i = 0; i < s; ++i)
          {
            dict = PyTuple_GetItem(args, i);
            if(PyDict_Check(dict)) {
              found = true;
              break;
            }
          }
          Py_DECREF(args);
          if(!found) error = true;
        } else error = true;
        long code = 0;
        string reason;
        string hdrs;
        if(!error) {
          PyObject* tmp;
          if((tmp = PyDict_GetItemString(dict, "code"))) {
            if(PyLong_Check(tmp)) {
              code = PyLong_AsLong(tmp);
            } else error = true;
          } else error = true;
          if((tmp = PyDict_GetItemString(dict, "reason"))) {
            if(PyUnicode_Check(tmp)) {
              reason = PyUnicode_AsUTF8(tmp);
            } else error = true;
          } else error = true;
          if((tmp = PyDict_GetItemString(dict, "hdrs"))) {
            if(PyUnicode_Check(tmp)) {
              reason = PyUnicode_AsUTF8(tmp);
            } else error = true;
          } else error = true;
        }
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(tb);
        if(!error) throw Exception(code, reason, hdrs);
      } else {
        PyErr_Restore(type, value, tb);
        PyErr_PrintEx(0);
      }
    } else {
      ERROR("NULL result without error in PyObject_Call");
    }
  }
  else {
    if(PyBool_Check(o) && (o == Py_True)) {
      ret = true;
    }

    Py_DECREF(o);
  }

  return ret;
}

void IvrDialog::onInvite(const AmSipRequest& req)
{
  invite_req = req;
  est_invite_cseq = req.cseq;
  if(callPyEventHandler("onInvite","(s)",req.hdrs.c_str()))
    AmB2BSession::onInvite(req);
}

void IvrDialog::onSessionStart()
{
  callPyEventHandler("onSessionStart",NULL);
  setInOut(&playlist,&playlist);
  AmB2BCallerSession::onSessionStart();
}

int IvrDialog::onSdpCompleted(const AmSdp& offer, const AmSdp& answer)
{
  AmMimeBody* sdp_body = invite_req.body.hasContentType(SIP_APPLICATION_SDP);
  if(!sdp_body) {
    sdp_body = invite_req.body.addPart(SIP_APPLICATION_SDP);
  }

  if(sdp_body) {
    string sdp_buf;
    answer.print(sdp_buf);
    sdp_body->setPayload((const unsigned char*)sdp_buf.c_str(),
			 sdp_buf.length());
  }

  return AmB2BCallerSession::onSdpCompleted(offer,answer);
}

void IvrDialog::onBye(const AmSipRequest& req)
{
  if(callPyEventHandler("onBye",NULL))
    AmB2BCallerSession::onBye(req);
}

void IvrDialog::onDtmf(int event, int duration_msec)
{
  if(callPyEventHandler("onDtmf","(ii)",event,duration_msec))
    AmB2BCallerSession::onDtmf(event,duration_msec);
}

bool IvrDialog::onOtherBye(const AmSipRequest& req)
{
  if(callPyEventHandler("onOtherBye",NULL))
    return AmB2BCallerSession::onOtherBye(req);
  else
    return true;
}

bool IvrDialog::onOtherReply(const AmSipReply& r)
{
  if(callPyEventHandler("onOtherReply","(is)",
			r.code,r.reason.c_str()))
    AmB2BCallerSession::onOtherReply(r);
  return false;
}

PyObject * getPySipReply(const AmSipReply& r)
{
  PYLOCK;
  return IvrSipReply_FromPtr(new AmSipReply(r));
}

PyObject * getPySipRequest(const AmSipRequest& r)
{
  PYLOCK;
  return IvrSipRequest_FromPtr(new AmSipRequest(r));
}

void safe_Py_DECREF(PyObject* pyo)
{
  PYLOCK;
  Py_DECREF(pyo);
}

struct ObjScope
{
  PyObject* obj;
  ObjScope(PyObject* o): obj(o) {}
  ~ObjScope() {safe_Py_DECREF(obj);}
};

void IvrDialog::onSipReply(const AmSipRequest& req,
			   const AmSipReply& reply, 
			   AmBasicSipDialog::Status old_dlg_status) 
{
  ObjScope pyrp(getPySipReply(reply));
  ObjScope pyrq(getPySipRequest(req));
  callPyEventHandler("onSipReply","(OO)", pyrq.obj, pyrp.obj);
  AmB2BCallerSession::onSipReply(req, reply, old_dlg_status);
}

void IvrDialog::onSipRequest(const AmSipRequest& r)
{
  mReq = r;
  ObjScope pyo(getPySipRequest(r));
  callPyEventHandler("onSipRequest","(O)", pyo.obj);
  AmB2BCallerSession::onSipRequest(r);
}

void IvrDialog::onRtpTimeout()
{
  callPyEventHandler("onRtpTimeout",NULL);
}

void IvrDialog::process(AmEvent* event) 
{
  DBG("IvrDialog::process\n");

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event) {
    if(audio_event->event_id == AmAudioEvent::noAudio) {
      callPyEventHandler("onEmptyQueue", NULL);
      event->processed = true;
    } else if(audio_event->event_id == AmAudioEvent::cleared) {
      callPyEventHandler("onAudioCleared", NULL);
      event->processed = true;
    }
  }

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    if (plugin_event->data.get(0).asInt() >= 0) {
      callPyEventHandler("onTimer", "(i)", plugin_event->data.get(0).asInt());
      event->processed = true;
    }
  }
  
  IvrEvent* ivr_event = dynamic_cast<IvrEvent*>(event);
  if(ivr_event) {
    callPyEventHandler("onIvrMessage", "(s)", ivr_event->msg.c_str());
    event->processed = true;
  }
  
  AmSystemEvent* sys_event = dynamic_cast<AmSystemEvent*>(event);
  if(sys_event) {
    if(sys_event->sys_event == AmSystemEvent::User1) {
      callPyEventHandler("onUser1", NULL);
      event->processed = true;
    } else if(sys_event->sys_event == AmSystemEvent::User2) {
      callPyEventHandler("onUser2", NULL);
      event->processed = true;
    } else if(sys_event->sys_event == AmSystemEvent::ServerShutdown) {
      callPyEventHandler("onServerShutdown", NULL);
      event->processed = true;
    }
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
  AmSipDialog* callee_dlg = callee_session->dlg;
  
  setOtherId(AmSession::getNewId());
  
  callee_dlg->setLocalTag(getOtherId());
  callee_dlg->setCallid(AmSession::getNewId());
  
  // this will be overwritten by ConnectLeg event 
  callee_dlg->setRemoteParty(dlg->getLocalParty());
  callee_dlg->setRemoteUri(dlg->getLocalUri());

  if (b2b_callee_from_party.empty() && b2b_callee_from_uri.empty()) {
    // default: use the original To as From in the callee leg
    callee_dlg->setLocalParty(dlg->getRemoteParty());
    callee_dlg->setLocalUri(dlg->getRemoteUri());
  } else {
    // if given as parameters, use these
    callee_dlg->setLocalParty(b2b_callee_from_party);
    callee_dlg->setLocalUri(b2b_callee_from_uri);
  }
  
  DBG("Created B2BUA callee leg, From: %s\n",
      callee_dlg->getLocalParty().c_str());

  callee_session->start();
  
  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(getOtherId(),callee_session);
}
