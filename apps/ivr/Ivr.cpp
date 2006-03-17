/*
 * $Id: Ivr.cpp,v 1.26.2.1 2005/09/02 13:47:46 rco Exp $
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
#include "IvrAudio.h"
#include "Ivr.h"

#include "AmConfigReader.h"
#include "AmConfig.h"
#include "log.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "AmSessionScheduler.h"
#include "AmSessionTimer.h"
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


    static PyMethodDef ivr_methods[] = {
 	{"log", (PyCFunction)ivr_log, METH_VARARGS,"Log a message using Sems' logging system"},
	{"getHeader", (PyCFunction)ivr_getHeader, METH_VARARGS,"Python getHeader wrapper"},
	{NULL}  /* Sentinel */
    };
}

IvrFactory::IvrFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

void IvrFactory::setScriptPath(const string& path)
{
    string python_path = script_path = path;

    
    if(python_path.length()){

	python_path = AmConfig::PlugInPath + ":" + python_path;
    }
    else
	python_path = AmConfig::PlugInPath;

    char* old_path=0;
    if((old_path = getenv("PYTHONPATH")) != 0)
	if(strlen(old_path))
	    python_path += ":" + string(old_path);

    DBG("setting PYTHONPATH to: '%s'\n",python_path.c_str());
    setenv("PYTHONPATH",python_path.c_str(),1);

}

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

    // IvrSipDialog (= AmSipDialog)
    import_object(ivr_module, "IvrSipDialog", &IvrSipDialogType);

    // IvrDialogBase
    import_object(ivr_module,"IvrDialogBase",&IvrDialogBaseType);


    // IvrAudioFile
    import_object(ivr_module,"IvrAudioFile",&IvrAudioFileType);

    PyModule_AddIntConstant(ivr_module, "AUDIO_READ",AUDIO_READ);
    PyModule_AddIntConstant(ivr_module, "AUDIO_WRITE",AUDIO_WRITE);
    // ivr module - end

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

void IvrFactory::init_python_interpreter()
{
    Py_Initialize();
    PyEval_InitThreads();
    import_ivr_builtins();
    PyEval_ReleaseLock();
}

IvrDialog* IvrFactory::newDlg(const string& name)
{
    PYLOCK;

    map<string,IvrScriptDesc>::iterator mod_it = mod_reg.find(name);
    if(mod_it == mod_reg.end()){
	ERROR("Unknown script name\n");
	throw AmSession::Exception(500,"Unknown Application");
	return NULL;
    }

    IvrScriptDesc& mod_desc = mod_it->second;

    IvrDialog* dlg = new IvrDialog();

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

    return dlg;
}

bool IvrFactory::loadScript(const string& path)
{
    PYLOCK;
    
    PyObject *modName,*mod,*dict, *dlg_class, *config=NULL;

    modName = PyString_FromString(path.c_str());
    mod     = PyImport_Import(modName);

    AmConfigReader cfg;
    string cfg_file = add2path(AmConfig::ModConfigPath,1,(path + ".conf").c_str());

    Py_DECREF(modName);

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

    mod_reg.insert(make_pair(path,
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
    AmConfigReader cfg;

    if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1,MOD_NAME ".conf")))
	return -1;

    // get application specific global parameters
    configureModule(cfg);

    setScriptPath(cfg.getParameter("script_path"));
    init_python_interpreter();

    DBG("** IVR compile time configuration:\n");
    DBG("**     built with PYTHON support.\n");

    DBG("**     Text-To-Speech "
#ifdef IVR_WITH_TTS
	"enabled"
#else
	"disabled"
#endif
	"\n");

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

    return 0; // don't stop sems from starting up
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

IvrDialog::IvrDialog()
    : py_mod(NULL), 
      py_dlg(NULL),
      playlist(this)
{
    sip_relay_only = false;
}

IvrDialog::~IvrDialog()
{
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

void IvrDialog::callPyEventHandler(char* name)
{

    PyObject* o = PyObject_CallMethod(py_dlg,name,NULL);
    Py_XDECREF(o);
    
    if(!o) 
	PyErr_Print();
}

void IvrDialog::callPyEventHandler(char* name, int id)
{

    PYLOCK;
    PyObject* o = PyObject_CallMethod(py_dlg,name, "i",id);
    Py_XDECREF(o);
    
    if(!o) 
	PyErr_Print();
}

void IvrDialog::callPyEventHandler(char* name, const string& str)
{

    PYLOCK;
    PyObject* o = PyObject_CallMethod(py_dlg,name, "s",str.c_str());
    Py_XDECREF(o);
    
    if(!o) 
	PyErr_Print();
}

void IvrDialog::onBeforeCallAccept(const AmSipRequest& req, 
				   unsigned int& reply_code, 
				   string& reply_reason) 
{
//   ivrPython->pCmd = &(req->cmd);
//   ivrPython->onBeforeCallAccept(reply_code, reply_reason);
}

void IvrDialog::onSessionStart(const AmSipRequest& req)
{
    PYLOCK;
    callPyEventHandler("onSessionStart",req.hdrs);
    setInOut(&playlist,&playlist);
    AmB2BCallerSession::onSessionStart(req);
}

void IvrDialog::onBye(const AmSipRequest& req)
{
    PYLOCK;
    callPyEventHandler("onBye");
}

void IvrDialog::onDtmf(int event, int duration_msec)
{
    PYLOCK;
    PyObject* o = PyObject_CallMethod(py_dlg,"onDtmf","ii",event,duration_msec);
    Py_XDECREF(o);
    
    if(!o) 
	PyErr_Print();
}

void IvrDialog::process(AmEvent* event) 
{
    DBG("IvrDialog::process\n");

    AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
    if(audio_event && audio_event->event_id == AmAudioEvent::noAudio){
	PYLOCK;
	callPyEventHandler("onEmptyQueue");
	event->processed = true;
    }
    
    AmTimeoutEvent* timeout_event = dynamic_cast<AmTimeoutEvent*>(event);
    if(timeout_event && timeout_event->event_id > 0) {
	PYLOCK;
	callPyEventHandler("onTimer", timeout_event->event_id);
	event->processed = true;
    }
    if (!event->processed)
      AmB2BCallerSession::process(event);

    return;
}

