#include "IvrAudioMixIn.h"
#include "IvrAudio.h"

#include "log.h"

static PyObject* IvrAudioMixIn_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DBG("---------- IvrAudioMixIn_alloc -----------\n");
  IvrAudioMixIn *self;

  self = (IvrAudioMixIn *)type->tp_alloc(type, 0);
	
  if (self != NULL) {
    self->mix = NULL;
  }

  return (PyObject *)self;
}

static void IvrAudioMixIn_dealloc(IvrAudioMixIn* self)
{
  DBG("---------- IvrAudioMixIn_dealloc -----------\n");
  if (self->mix) {
    delete self->mix;
    self->mix = NULL;
  }

  self->ob_type->tp_free((PyObject*)self);
}


static PyObject* IvrAudioMixIn_init(IvrAudioMixIn* self, PyObject* args)
{
  AmAudioFile* a = NULL;
  AmAudioFile* b = NULL;
  int s;
  double l;
  int finish = 0, mix_once=0, mix_immediate=0;

  PyObject *o_a, *o_b;

  if(!PyArg_ParseTuple(args,"OOid|iii", &o_a, &o_b, &s, &l, &finish, &mix_once, &mix_immediate))
    return NULL;

  if (o_a == Py_None) {
    PyErr_SetString(PyExc_TypeError,"Argument 1 is None (need IvrAudioFile)");
    return NULL;
  }

  if (o_b == Py_None) {
    PyErr_SetString(PyExc_TypeError,"Argument 2 is None (need IvrAudioFile)");
    return NULL;
  }

  if(!PyObject_TypeCheck(o_a,&IvrAudioFileType)){
    PyErr_SetString(PyExc_TypeError,"Argument 1 is no IvrAudioFile");
    return NULL;
  }
  a  = ((IvrAudioFile*)o_a)->af;


  if(!PyObject_TypeCheck(o_b,&IvrAudioFileType)){
    PyErr_SetString(PyExc_TypeError,"Argument 2 is no IvrAudioFile");
    return NULL;
  }
  b  = ((IvrAudioFile*)o_b)->af;

  if (NULL !=self->mix) {
    delete self->mix;
  }

  int flags = 0;
  if (finish) flags |=AUDIO_MIXIN_FINISH_B_MIX;
  if (mix_once) flags |=AUDIO_MIXIN_ONCE;
  if (mix_immediate) flags |=AUDIO_MIXIN_IMMEDIATE_START;

  self->mix = new AmAudioMixIn(a, b, s, l, flags);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef IvrAudioMixIn_methods[] = {
  {"init", (PyCFunction)IvrAudioMixIn_init, METH_VARARGS,
   "open the mixin with two audio files, interval and level"
  },
  {NULL}  /* Sentinel */
};



static PyGetSetDef IvrAudioMixIn_getseters[] = {
  {NULL}  /* Sentinel */
};
    
PyTypeObject IvrAudioMixInType = {
	
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrAudioMixIn",        /*tp_name*/
  sizeof(IvrAudioMixIn),      /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)IvrAudioMixIn_dealloc, /*tp_dealloc*/
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
  "An audio file",           /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  IvrAudioMixIn_methods,      /* tp_methods */
  0,                         /* tp_members */
  IvrAudioMixIn_getseters,    /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrAudioMixIn_new,          /* tp_new */
};
