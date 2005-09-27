#include "IvrDialogBase.h"
#include "IvrAudio.h"
#include "Ivr.h"

// Data definition
typedef struct {
    
    PyObject_HEAD
    IvrDialog* p_dlg;
    
} IvrDialogBase;


// Constructor
static PyObject* IvrDialogBase_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"ivr_dlg", NULL};
    IvrDialogBase *self;

    self = (IvrDialogBase *)type->tp_alloc(type, 0);
    if (self != NULL) {
	
    	PyObject* o_dlg = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &o_dlg)){
	    
	    Py_DECREF(self);
	    return NULL;
	}
    
	if (!PyCObject_Check(o_dlg)){
	    
	    Py_DECREF(self);
	    return NULL;
	}
	
	self->p_dlg = (IvrDialog*)PyCObject_AsVoidPtr(o_dlg);
    }

    DBG("IvrDialogBase_new\n");
    return (PyObject *)self;
}

//
// Event handlers
//
static PyObject* IvrDialogBase_onSessionStart(IvrDialogBase* self, PyObject*)
{
    PyErr_SetNone(PyExc_NotImplementedError);
    return NULL; // no return value
}

static PyObject* IvrDialogBase_onBye(IvrDialogBase* self, PyObject*)
{
    PyErr_SetNone(PyExc_NotImplementedError);
    return NULL; // no return value
}

static PyObject* IvrDialogBase_onEmptyQueue(IvrDialogBase* self, PyObject*)
{
    PyErr_SetNone(PyExc_NotImplementedError);
    return NULL; // no return value
}

static PyObject* IvrDialogBase_onDtmf(IvrDialogBase* self, PyObject* args)
{
    PyErr_SetNone(PyExc_NotImplementedError);
    return NULL; // no return value
}

//
// Call control
//
static PyObject* IvrDialogBase_stopSession(IvrDialogBase* self, PyObject*)
{
    assert(self->p_dlg);
    self->p_dlg->stopSession();
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* IvrDialogBase_bye(IvrDialogBase* self, PyObject*)
{
    assert(self->p_dlg);
    self->p_dlg->getSession()->req->bye();
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
    AmAudio  *a_play=NULL, *a_rec=NULL;
    
    if(!PyArg_ParseTuple(args,"OO",&o_play,&o_rec))
	return NULL;
    
    if (o_play != Py_None){
	
	if(!PyObject_TypeCheck(o_play,&IvrAudioFileType)){
	    
	    PyErr_SetString(PyExc_TypeError,"Argument 1 is no IvrAudioFile");
	    return NULL;
	}
	
	a_play = ((IvrAudioFile*)o_play)->af;
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

    self->p_dlg->playlist.close();
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef IvrDialogBase_methods[] = {
    
    // Event handlers
    {"onSessionStart", (PyCFunction)IvrDialogBase_onSessionStart, METH_NOARGS,
     "Gets called on session start"
    },
    {"onBye", (PyCFunction)IvrDialogBase_onBye, METH_NOARGS,
     "Gets called if we received a BYE"
    },
    {"onEmptyQueue", (PyCFunction)IvrDialogBase_onEmptyQueue, METH_NOARGS,
     "Gets called when the audio queue runs out of items"
    },
    {"onDtmf", (PyCFunction)IvrDialogBase_onDtmf, METH_VARARGS,
     "Gets called when dtmf have been received"
    },
    
    // Call control
    {"stopSession", (PyCFunction)IvrDialogBase_stopSession, METH_NOARGS,
     "Stop the session"
    },
    {"bye", (PyCFunction)IvrDialogBase_bye, METH_NOARGS,
     "Send a BYE"
    },
    
    // Media control
    {"enqueue", (PyCFunction)IvrDialogBase_enqueue, METH_VARARGS,
     "Add some audio to the queue (mostly IvrAudioFile)"
    },
    {"flush", (PyCFunction)IvrDialogBase_flush, METH_NOARGS,
     "Flush the queue"
    },
    
    
    {NULL}  /* Sentinel */
};


PyTypeObject IvrDialogBaseType = {
    
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "ivr.IvrDialogBase",       /*tp_name*/
    sizeof(IvrDialogBase),     /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
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
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    IvrDialogBase_new,         /* tp_new */
};
