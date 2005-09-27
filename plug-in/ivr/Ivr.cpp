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
#include "IvrAudio.h"
#include "Ivr.h"

#include "SemsConfiguration.h"
#include "AmConfig.h"
#include "log.h"
#include "AmApi.h"
#include "AmUtils.h"
#include "AmSessionScheduler.h"

#include <unistd.h>
#include <pthread.h>

// #ifdef IVR_WITH_TTS
// #define CACHE_PATH "/tmp/"
// extern "C" cst_voice *register_cmu_us_kal();
// #endif //ivr_with_tts


EXPORT_FACTORY(IvrFactory,MOD_NAME);


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

    static PyMethodDef ivr_methods[] = {
 	{"log", (PyCFunction)ivr_log, METH_VARARGS,
 	 "Log a message using Sems' logging system"
 	},
	{NULL}  /* Sentinel */
    };
}

IvrFactory::IvrFactory(const string& _app_name)
  : AmStateFactory(_app_name)
{
}

void IvrFactory::setScriptPath(const string& path)
{
    script_path = AmConfig::PlugInPath + "/:" + path;
    setenv("PYTHONPATH",script_path.c_str(),1);

    if(script_path.length() && script_path[script_path.length()-1] != '/')
	script_path += '/';
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
    PyObject* m = Py_InitModule("ivr",ivr_methods);

    // IvrDialogBase
    import_object(m,"IvrDialogBase",&IvrDialogBaseType);

    // IvrAudioFile
    import_object(m,"IvrAudioFile",&IvrAudioFileType);

    PyModule_AddIntConstant(m, "AUDIO_READ",AUDIO_READ);
    PyModule_AddIntConstant(m, "AUDIO_WRITE",AUDIO_WRITE);
    // ivr module - end
    
    PyObject* log_mod_name = PyString_FromString("log");
    PyObject* log_mod = PyImport_Import(log_mod_name);
    Py_DECREF(log_mod_name);

    if(!log_mod){
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

IvrDialog* IvrFactory::loadScript(const string& path)
{
    PYLOCK;
    
    PyObject *modName,*mod,*dict, *dlg_class;

    modName = PyString_FromString(path.c_str());
    mod     = PyImport_Import(modName);

    Py_DECREF(modName);

    if(!mod){
        PyErr_Print();
        ERROR("IvrFactory: Failed to load \"%s\"\n", path.c_str());

	dict = PyImport_GetModuleDict();
	Py_INCREF(dict);
	PyDict_DelItemString(dict,path.c_str());
	Py_DECREF(dict);

	return NULL;
    }

    dict = PyModule_GetDict(mod);
    dlg_class = PyDict_GetItemString(dict, "IvrDialog");

    if(!dlg_class){

	Py_DECREF(mod);
	PyErr_Print();
	ERROR("IvrFactory: class IvrDialog not found in \"%s\"\n", path.c_str());
	return NULL;
    }

    Py_INCREF(dlg_class);

    if(!PyObject_IsSubclass(dlg_class,(PyObject*)&IvrDialogBaseType)){

	Py_DECREF(dlg_class);
	Py_DECREF(mod);
	ERROR("IvrFactory: in \"%s\": IvrDialog is not a subtype of IvrDialogBase\n",
	      path.c_str());
	return NULL;
    }


    IvrDialog* dlg = new IvrDialog();

    PyObject* c_dlg = PyCObject_FromVoidPtr(dlg,NULL);
    PyObject* dlg_inst = PyObject_CallMethod(dlg_class,"__new__","OO",dlg_class,c_dlg);

    Py_DECREF(c_dlg);
    Py_DECREF(dlg_class);

    if(!dlg_inst){

	Py_DECREF(mod);
	delete dlg;

	PyErr_Print();
	ERROR("IvrFactory: while loading \"%s\": could not create instance\n",
	      path.c_str());

	return NULL;
    }    

    dlg->setPyPtrs(mod,dlg_inst);

    return dlg;
}

/**
 * Loads python script path and default script file from configuration file
 */
int IvrFactory::onLoad()
{
    SemsConfiguration mIvrConfig;

    if(mIvrConfig.reloadModuleConfig(MOD_NAME) == 1) {

      char* pSF = 0;

      if(((pSF = mIvrConfig.getValueForKey("script_path")) != NULL) && ( *pSF != '\0') ){
	  
	  setScriptPath(pSF);
      }
      
//       if(((pSF = mIvrConfig.getValueForKey("default_script")) != NULL) && ( *pSF != '\0') ){
// 	  default_script = string(pSF);
//       }

//       if(!checkCfg()){
// 	  ERROR("Ivr probably won't work, as the default script could not be found.\n");
// 	  ERROR("Please set 'script_path' and 'default_script' in your configuration file.\n");
//       }

      init_python_interpreter();

// #ifdef IVR_WITH_TTS
//       char* p =0;
//       if( ((p = mIvrConfig.getValueForKey("tts_caching")) != NULL) && (*p != '\0') ) {
// 	tts_caching = ((*p=='y') || (*p=='Y'));
//       } else {
// 	WARN("no tts_caching (y/n) specified in configuration\n");
// 	WARN("file for module ivr.\n");
// 	tts_caching = true;
//       }

//       if( ((p = mIvrConfig.getValueForKey("tts_cache_path")) != NULL) && (*p != '\0') )
// 	tts_cache_path = p;
//       else {
// 	WARN("no cache_path specified in configuration\n");
// 	WARN("file for module semstalkflite.\n");
// 	tts_cache_path  = CACHE_PATH;
//       }
//       if( !tts_cache_path.empty()
// 	  && tts_cache_path[tts_cache_path.length()-1] != '/' )
// 	tts_cache_path += "/";
// #endif

    } // success loading configuration


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
//     DBG("**     default script file: \'%s\'\n", default_script.c_str());

// #ifdef IVR_WITH_TTS
//     DBG("**     TTS caching:          %s\n", tts_caching ? "yes":"no");
//     DBG("**     TTS cache path:      \'%s\'\n", tts_cache_path.c_str());
// #endif

    return 0; // don't stop sems from starting up
}

/**
 * Load a script using user name from URI.
 * Note: there is no default script.
 */
AmDialogState* IvrFactory::onInvite(AmCmd& cmd)
{
    return loadScript(cmd.user);
}

IvrDialog::IvrDialog()
    : py_mod(NULL), 
      py_dlg(NULL),
      playlist(this)
{
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
}

void IvrDialog::callPyEventHandler(char* name)
{
    PyObject* o = PyObject_CallMethod(py_dlg,name,NULL);
    Py_XDECREF(o);
    
    if(!o) 
	PyErr_Print();
}


void IvrDialog::onBeforeCallAccept(AmRequest* req, 
				   unsigned int& reply_code, 
				   string& reply_reason) 
{
//   ivrPython->pCmd = &(req->cmd);
//   ivrPython->onBeforeCallAccept(reply_code, reply_reason);
}

void IvrDialog::onSessionStart(AmRequest* req)
{
    PYLOCK;
    callPyEventHandler("onSessionStart");
    getSession()->setInOut(&playlist,&playlist);
}

void IvrDialog::onBye(AmRequest* req)
{
    PYLOCK;
    callPyEventHandler("onBye");
}

int IvrDialog::onOther(AmSessionEvent* event)
{
//     if(event->event_id == AmSessionEvent::Notify){
// 	DBG("Notify received...\n");
//  	AmNotifySessionEvent* notifyEvent = dynamic_cast<AmNotifySessionEvent*>(event);
//  	if (!notifyEvent) {
//  	    ERROR("Invalid In-Session Notify event received.\n");
//  	    return 1;
//  	}
//  	DBG("evtpackage == %s\n", notifyEvent->getEventPackage().c_str());
	
//  	if (strncasecmp(notifyEvent->getEventPackage().c_str(), "refer", 5) == 0 ) { // we only handle refer-NOTIFYs
//  	    if (event->request.reply(200, "OK")) 
//  		DBG("Could not reply 200 (request was NOTIFY)\n");
//  	    DBG("NOTIFY Event Body: %s\n", event->request.getBody().c_str());
// 	    AmNotifySessionEvent* referStatusEvent = 
// 		new AmNotifySessionEvent(*notifyEvent);   // create new event for IvrPython's queue
// 	    ivrPython->postScriptEvent(new IvrScriptEvent(IvrScriptEvent::IVR_ReferStatus, 
// 							  referStatusEvent));
//  	}	       
//     }
    return 0;
}

int IvrDialog::onUACRequestStatus(AmRequestUACStatusEvent* event) 
{
//     if (strncasecmp(event->request.cmd.method.c_str(), "REFER", 5) == 0 )
// 	ivrPython->postScriptEvent(new IvrScriptEventReferResponse(event->request.cmd.method, 
// 								   event->request.cmd.cseq,
// 								   event->code, event->reason));    
//     else
// 	ERROR("unknown UAC Request Status Event received: method = %s.\n", event->request.cmd.method.c_str());
    return 0;
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
    }
    
    AmDialogState::process(event);

//     // IvrMediaEvent ?
//     IvrMediaEvent* evt = dynamic_cast<IvrMediaEvent* >(event);
//     if (evt) { // this one is for us
// 	DBG("IvrDialog processing event...\n");
// 	if (handleMediaEvent(evt)) { 
// 	    ERROR("while processing Media event (ID=%i).\n",event->event_id);
// 	}
// 	return;
//     } 


//     IvrDialogControlEvent* dlgctlevt = dynamic_cast<IvrDialogControlEvent*>(event);
//     if (dlgctlevt) {
//       DBG("IvrDialog processing dlg ctl event...\n");
//       if (handleDialogControlEvent(dlgctlevt)) {
// 	ERROR("while processing dlg ctl event.\n");
//       }
//       return;
//     };

//     // AmSessionEvent ?
//     AmSessionEvent* session_event = dynamic_cast<AmSessionEvent*>(event);
//     if(session_event){
// 	DBG("in-dialog event received: %s\n",
// 	    session_event->request.cmd.method.c_str());

// 	if(onOther(session_event))
// 	    ERROR("while processing session event (ID=%i).\n",session_event->event_id);
// 	return;
//     } 

//     //  AmRequestUACStatusEvent?
//     AmRequestUACStatusEvent* status_event = dynamic_cast<AmRequestUACStatusEvent*>(event);
//     if(status_event){
// 	DBG("in-dialog RequestUAC Staus event received: method=%s, cseq = %d, \n\t\tcode = %d, reason = %s\n",
// 	    status_event->request.cmd.method.c_str(), status_event->request.cmd.cseq, status_event->code, status_event->reason.c_str());

// 	if(onUACRequestStatus(status_event))
// 	    ERROR("while processing session event (ID=%i).\n",session_event->event_id);

// 	return;
//     } 

//     ERROR("AmSession: invalid event received.\n");
    return;
}

// int IvrDialog::handleMediaEvent(IvrMediaEvent* evt) 
// {
//   evt->processed = true; // we eat up all media events at the moment
//   if (!mediaHandler) {
//     ERROR("no MediaHandler to process event.\n");
//     return 1;
//   }
//   switch (evt->event_id) {
//   case IvrMediaEvent::IVR_enqueueMediaFile: {
//       DBG("IvrDialog::handleMediaEvent: received IvrMediaEvent::IVR_enqueueMediaFile\n");
//     return mediaHandler->enqueueMediaFile(evt->MediaFile, evt->front, evt->loop);
//   }; break;
//   case IvrMediaEvent::IVR_emptyMediaQueue: {
//     return mediaHandler->emptyMediaQueue();
//   }; break;
//   case IvrMediaEvent::IVR_startRecording: {
//     return mediaHandler->startRecording(evt->MediaFile);
//   }; break;
//   case IvrMediaEvent::IVR_stopRecording: {
//     return mediaHandler->stopRecording();
//   }; break;
//   case IvrMediaEvent::IVR_enableDTMFDetection: {
//       return 0;//mediaHandler->enableDTMFDetection();
//   }; break;
//   case IvrMediaEvent::IVR_disableDTMFDetection: {
//       return 0;//mediaHandler->disableDTMFDetection();
//   }; break;

//   case IvrMediaEvent::IVR_mthr_usleep: {
//       DBG("Media Thread sleeping %d usec\n", evt->usleep_time);
//       usleep(evt->usleep_time);
//       DBG("Media Thread: done sleeping.\n");
//     return 0;
//   }; break;
//   }
  
//   return 0;
// }

// int IvrDialog::handleDialogControlEvent(IvrDialogControlEvent* evt) 
// {
//   evt->processed = true;
//   switch (evt->event_id) {
//     case IvrDialogControlEvent::ChangeSendByePolicy: {
//       DBG("when I will end this Dialog, I will %s send a BYE.\n", 
// 	  evt->bool_value ? "do":"not" );
//       sendBye = evt->bool_value;
//       return 0;
//     };
//   };
//   return 1;
// }
