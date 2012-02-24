#include "IvrDialogBase.h"
#include "IvrAudio.h"
#include "IvrAudioMixIn.h"

#include "Ivr.h"

#include "IvrSipDialog.h"
#include "IvrSipRequest.h"
#include "AmMediaProcessor.h"


/** \brief python wrapper of IvrDialog, the base class for python IVR sessions */
typedef struct {
    
  PyObject_HEAD
  PyObject* dialog;
  PyObject* invite_req;
  IvrDialog* p_dlg;
    
} IvrDialogBase;


// Constructor
static PyObject* IvrDialogBase_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {(char*)"ivr_dlg", NULL};
  IvrDialogBase *self;

  self = (IvrDialogBase *)type->tp_alloc(type, 0);
  if (self != NULL) {
	
    PyObject* o_dlg = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &o_dlg)){
	    
      Py_DECREF(self);
      return NULL;
    }
    
    if ((NULL == o_dlg) || !PyCObject_Check(o_dlg)){
	    
      Py_DECREF(self);
      return NULL;
    }
	
    self->p_dlg = (IvrDialog*)PyCObject_AsVoidPtr(o_dlg);
	
    // initialize self.dialog
    self->dialog = IvrSipDialog_FromPtr(&self->p_dlg->dlg);
    if(!self->dialog){
      PyErr_Print();
      ERROR("IvrDialogBase: while creating IvrSipDialog instance\n");
      Py_DECREF(self);
      return NULL;
    }

    // initialize self.invite_req - AmSipRequest is not owned!
    self->invite_req = IvrSipRequest_BorrowedFromPtr(self->p_dlg->getInviteReq());
    if(!self->invite_req){
      PyErr_Print();
      ERROR("IvrDialogBase: while creating IvrSipRequest instance for invite_req\n");
      Py_DECREF(self);
      return NULL;
    }

  }

  DBG("IvrDialogBase_new\n");
  return (PyObject *)self;
}

static void
IvrDialogBase_dealloc(IvrDialogBase* self) 
{
  DBG("IvrDialogBase_dealloc\n");
  Py_XDECREF(self->dialog);
  self->dialog=NULL;
  Py_XDECREF(self->invite_req);
  self->invite_req=NULL;
  self->ob_type->tp_free((PyObject*)self);
}

//
// Event handlers
//
static PyObject* IvrDialogBase_onRtpTimeout(IvrDialogBase* self, PyObject*)
{
  DBG("no script implementation for onRtpTimeout(). Stopping session. \n");

  assert(self->p_dlg);
  self->p_dlg->setStopped();
  self->p_dlg->postEvent(0);

  Py_INCREF(Py_None);
  return Py_None;
}

//
// Call control
//
static PyObject* IvrDialogBase_stopSession(IvrDialogBase* self, PyObject*)
{
  assert(self->p_dlg);
  self->p_dlg->setStopped();
  self->p_dlg->postEvent(0);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_dropSession(IvrDialogBase* self, PyObject*)
{
  assert(self->p_dlg);
  self->p_dlg->drop();
  self->p_dlg->postEvent(0);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_bye(IvrDialogBase* self, PyObject* args)
{
  char* hdrs = (char*)"";

  assert(self->p_dlg);

  if(!PyArg_ParseTuple(args,"|s", &hdrs))
    return NULL;

  self->p_dlg->dlg.bye(hdrs);
  Py_INCREF(Py_None);
  return Py_None;
}

//
// Media control
//
static PyObject* IvrDialogBase_enqueue(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
    
  PyObject *o_play, *o_rec;
  AmAudio *a_play=NULL;
  AmAudioFile *a_rec=NULL;
    
  if(!PyArg_ParseTuple(args,"OO",&o_play,&o_rec))
    return NULL;
    
  if (o_play != Py_None){
	
    if(PyObject_TypeCheck(o_play,&IvrAudioFileType)){
      ((IvrAudioFile*)o_play)->af->rewind();
      a_play = ((IvrAudioFile*)o_play)->af;

    } else if(PyObject_TypeCheck(o_play,&IvrAudioMixInType)){

      a_play = ((IvrAudioMixIn*)o_play)->mix;

    } else { 
      PyErr_SetString(PyExc_TypeError,"Argument 1 is no IvrAudioFile");
      return NULL;
    }
	
  }
    
  if (o_rec != Py_None){
	
    if(!PyObject_TypeCheck(o_rec,&IvrAudioFileType)){
	    
      PyErr_SetString(PyExc_TypeError,"Argument 2 is no IvrAudioFile");
      return NULL;
    }
	
    a_rec = ((IvrAudioFile*)o_rec)->af;
  }
    
  self->p_dlg->playlist.addToPlaylist(new AmPlaylistItem(a_play,a_rec));
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_flush(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->playlist.flush();
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_queueIsEmpty(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  return PyBool_FromLong(self->p_dlg->playlist.isEmpty());
}

static PyObject* IvrDialogBase_mute(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setMute(true);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_unmute(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setMute(false);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_enableReceiving(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setReceiving(true);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_disableReceiving(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setReceiving(false);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_enableDTMFReceiving(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setForceDtmfReceiving(true);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_disableDTMFReceiving(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setForceDtmfReceiving(false);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_remove_mediaprocessor(IvrDialogBase* self, 
						     PyObject* args)
{
  assert(self->p_dlg);

  AmMediaProcessor::instance()->removeSession(self->p_dlg);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_add_mediaprocessor(IvrDialogBase* self, 
						  PyObject* args)
{
  assert(self->p_dlg);

  AmMediaProcessor::instance()->addSession(self->p_dlg, 
					   self->p_dlg->getCallgroup());
    
  Py_INCREF(Py_None);
  return Py_None;
}

// DTMF

static PyObject* IvrDialogBase_enableDTMFDetection(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setDtmfDetectionEnabled(true);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_disableDTMFDetection(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);

  self->p_dlg->setDtmfDetectionEnabled(false);
    
  Py_INCREF(Py_None);
  return Py_None;
}

// B2B methods
static PyObject* IvrDialogBase_b2b_connectCallee(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
    
  string remote_party, remote_uri, local_party, local_uri;

  PyObject* py_o;

  if((PyArg_ParseTuple(args,"O",&py_o)) && (py_o == Py_None)) {
    DBG("args == Py_None\n");
    remote_party = self->p_dlg->getOriginalRequest().to;
    remote_uri = self->p_dlg->getOriginalRequest().r_uri;
  } else {
    DBG("args != Py_None\n");
    char* rp = 0; char* ru = 0; char* lp = 0; char* lu = 0;
    if(!PyArg_ParseTuple(args,"ss|ss",&rp, &ru, &lp, &lu))
      return NULL;
    else {
      remote_party = string(rp);
      remote_uri = string(ru);
      if (lp && lu) {
	local_party = string(lp);
	local_uri = string(lu);
      }
    } 
  }
    
  self->p_dlg->connectCallee(remote_party, remote_uri, local_party, local_uri);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_b2b_set_relayonly(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
  self->p_dlg->set_sip_relay_only(true);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_b2b_set_norelayonly(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
  self->p_dlg->set_sip_relay_only(false);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_b2b_terminate_leg(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
  self->p_dlg->terminateLeg();
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_b2b_terminate_other_leg(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
  self->p_dlg->terminateOtherLeg();
    
  Py_INCREF(Py_None);
  return Py_None;
}


// Timer methods
static PyObject* IvrDialogBase_setTimer(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
    
  int id = 0, interval = 0;
  if(!PyArg_ParseTuple(args,"ii",&id, &interval))
    return NULL;
    
  if (id <= 0) {
    ERROR("IVR script tried to set timer with non-positive ID.\n");
    return NULL;
  }

  self->p_dlg->setTimer(id, interval);
    
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrDialogBase_removeTimer(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
    
  int id = 0;
  if(!PyArg_ParseTuple(args,"i",&id))
    return NULL;
    
  if (id <= 0) {
    ERROR("IVR script tried to remove timer with non-positive ID.\n");
    return NULL;
  }

  self->p_dlg->removeTimer(id);
    
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject* IvrDialogBase_removeTimers(IvrDialogBase* self, PyObject* args)
{
  assert(self->p_dlg);
    
  self->p_dlg->removeTimers();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject*
IvrDialogBase_getinvite_req(IvrDialogBase *self, void *closure)
{
  Py_INCREF(self->invite_req);
  return self->invite_req;
}

static PyObject*
IvrDialogBase_getdialog(IvrDialogBase *self, void *closure)
{
  Py_INCREF(self->dialog);
  return self->dialog;
}

static PyObject*
IvrDialogBase_redirect(IvrDialogBase *self, PyObject* args)
{
  assert(self->p_dlg);
    
  char* refer_to=0;
  if(!PyArg_ParseTuple(args,"s",&refer_to))
    return NULL;
    
  if(self->p_dlg->transfer(refer_to)){
    ERROR("redirect failed\n");
    return NULL;
  }
    
  Py_INCREF(Py_None);
  return Py_None;
    
}

static PyObject*
IvrDialogBase_refer(IvrDialogBase *self, PyObject* args)
{
  assert(self->p_dlg);
    
  char* refer_to=0;
  int expires;
  if(!PyArg_ParseTuple(args,"si",&refer_to, &expires))
    return NULL;
    
  if(self->p_dlg->refer(refer_to, expires)){
    ERROR("REFER failed\n");
    return NULL;
  }
    
  Py_INCREF(Py_None);
  return Py_None;
    
}

static PyObject* 
IvrDialogBase_getAppParam(IvrDialogBase *self, PyObject* args)
{
  const char* param_name;
  if(!PyArg_ParseTuple(args,"s",&param_name))
    return NULL;

  string app_param = self->p_dlg->getAppParam(param_name);
  return PyString_FromString(app_param.c_str());
}


static PyMethodDef IvrDialogBase_methods[] = {
    

  // Event handlers

  {"onRtpTimeout", (PyCFunction)IvrDialogBase_onRtpTimeout, METH_NOARGS,
   "Gets called on RTP timeout"
  },

  //     {"onSessionStart", (PyCFunction)IvrDialogBase_onSessionStart, METH_VARARGS,
  //      "Gets called on session start"
  //     },
  //     {"onBye", (PyCFunction)IvrDialogBase_onBye, METH_NOARGS,
  //      "Gets called if we received a BYE"
  //     },
  //     {"onEmptyQueue", (PyCFunction)IvrDialogBase_onEmptyQueue, METH_NOARGS,
  //      "Gets called when the audio queue runs out of items"
  //     },
  //     {"onDtmf", (PyCFunction)IvrDialogBase_onDtmf, METH_VARARGS,
  //      "Gets called when dtmf have been received"
  //     },
  //     {"onTimer", (PyCFunction)IvrDialogBase_onTimer, METH_VARARGS,
  //      "Gets called when a timer is fired"
  //     },
    
  // Call control
  {"stopSession", (PyCFunction)IvrDialogBase_stopSession, METH_NOARGS,
   "Stop the session"
  },
  {"bye", (PyCFunction)IvrDialogBase_bye, METH_VARARGS,
   "Send a BYE"
  },
  {"redirect", (PyCFunction)IvrDialogBase_redirect, METH_VARARGS,
   "Transfers the remote party to some third party."
  },   
  {"refer", (PyCFunction)IvrDialogBase_refer, METH_VARARGS,
   "Refers the remote party to some third party."
  },   
  {"dropSession", (PyCFunction)IvrDialogBase_dropSession, METH_NOARGS,
   "Drop the session and forget it without replying"
  },
  // Media control
  {"enqueue", (PyCFunction)IvrDialogBase_enqueue, METH_VARARGS,
   "Add some audio to the queue (mostly IvrAudioFile)"
  },
  {"queueIsEmpty", (PyCFunction)IvrDialogBase_queueIsEmpty, METH_NOARGS,
   "Is the audio queue empty?"
  },
  {"flush", (PyCFunction)IvrDialogBase_flush, METH_NOARGS,
   "Flush the queue"
  },
  {"mute", (PyCFunction)IvrDialogBase_mute, METH_NOARGS,
   "mute the RTP stream (don't send packets)"
  },
  {"unmute", (PyCFunction)IvrDialogBase_unmute, METH_NOARGS,
   "unmute the RTP stream (send packets)"
  },
  {"enableReceiving", (PyCFunction)IvrDialogBase_enableReceiving, METH_NOARGS,
   "enable receiving of RTP packets"
  },
  {"disableReceiving", (PyCFunction)IvrDialogBase_disableReceiving, METH_NOARGS,
   "disable receiving of RTP packets"
  },
  {"enableDTMFReceiving", (PyCFunction)IvrDialogBase_enableDTMFReceiving, METH_NOARGS,
   "enable receiving of RFC-2833 DTMF packets even if RTP receiving is disabled"
  },
  {"disableDTMFReceiving", (PyCFunction)IvrDialogBase_disableDTMFReceiving, METH_NOARGS,
   "disable receiving of RFC-2833 DTMF packets when RTP receiving is disabled"
  },
  {"connectMedia", (PyCFunction)IvrDialogBase_add_mediaprocessor, METH_NOARGS,
   "enable the processing of audio and RTP"
  },
  {"disconnectMedia", (PyCFunction)IvrDialogBase_remove_mediaprocessor, METH_NOARGS,
   "disable the processing of audio and RTP"
  },
  // DTMF
  {"enableDTMFDetection", (PyCFunction)IvrDialogBase_enableDTMFDetection, METH_NOARGS,
   "enable the dtmf detection"
  },
  {"disableDTMFDetection", (PyCFunction)IvrDialogBase_disableDTMFDetection, METH_NOARGS,
   "disable the dtmf detection"
  },    
  // B2B
  {"connectCallee", (PyCFunction)IvrDialogBase_b2b_connectCallee, METH_VARARGS,
   "call given party as (new) callee,"
   "if remote_party and remote_uri are empty (None),"
   "we will connect to the callee of the initial caller request"
  },
  {"terminateLeg", (PyCFunction)IvrDialogBase_b2b_terminate_leg, METH_VARARGS,
   "Terminate our leg and forget the other"
  },
  {"terminateOtherLeg", (PyCFunction)IvrDialogBase_b2b_terminate_other_leg, METH_VARARGS,
   "Terminate the other leg and forget it"
  },
  {"setRelayonly", (PyCFunction)IvrDialogBase_b2b_set_relayonly, METH_NOARGS,
   "sip requests will be relayed, and not processed"
  },
  {"setNoRelayonly", (PyCFunction)IvrDialogBase_b2b_set_norelayonly, METH_NOARGS,
   "sip requests will be processed"
  },
  // Timers
  {"setTimer", (PyCFunction)IvrDialogBase_setTimer, METH_VARARGS,
   "set a timer with id and t seconds timeout"
  },
  {"removeTimer", (PyCFunction)IvrDialogBase_removeTimer, METH_VARARGS,
   "remove a timer by id"
  },    
  {"removeTimers", (PyCFunction)IvrDialogBase_removeTimers, METH_NOARGS,
   "remove all timers"
  },    
  // App params
  {"getAppParam", (PyCFunction)IvrDialogBase_getAppParam, METH_VARARGS,
   "retrieves an application parameter"
  },

  {NULL}  /* Sentinel */
};

static PyGetSetDef IvrDialogBase_getset[] = {
  {(char*)"dialog", 
   (getter)IvrDialogBase_getdialog, NULL,
   (char*)"the dialog property",
   NULL},
  {(char*)"invite_req", 
   (getter)IvrDialogBase_getinvite_req, NULL,
   (char*)"the initial invite request",
   NULL},
  {NULL}  /* Sentinel */
};

PyTypeObject IvrDialogBaseType = {
    
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrDialogBase",       /*tp_name*/
  sizeof(IvrDialogBase),     /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)IvrDialogBase_dealloc,     /*tp_dealloc*/
  0,                         /*tp_print*/
  0,                         /*tp_getattr*/
  0,                         /*tp_setattr*/
  0,                         /*tp_compare*/
  0,                         /*tp_repr*/
  0,                         /*tp_as_number*/
  0,                         /*tp_as_sequence*/
  0,                         /*tp_as_mapping*/
  0,                         /*tp_hash */
  0,                         /*tp_call*/
  0,                         /*tp_str*/
  0,                         /*tp_getattro*/
  0,                         /*tp_setattro*/
  0,                         /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
  "Base class for IvrDialog", /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  IvrDialogBase_methods,     /* tp_methods */
  0,                         /* tp_members */
  IvrDialogBase_getset,      /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrDialogBase_new,         /* tp_new */
};
