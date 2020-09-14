#include "IvrNullAudio.h"
#include "IvrAudio.h"

#include "log.h"

static PyObject* IvrNullAudio_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DBG("---------- IvrNullAudio_alloc -----------\n");
  IvrNullAudio *self;

  self = (IvrNullAudio *)type->tp_alloc(type, 0);
	
  if (self != NULL) {
    self->nullaudio = NULL;
  }

  return (PyObject *)self;
}

static void IvrNullAudio_dealloc(IvrNullAudio* self)
{
  DBG("---------- IvrNullAudio_dealloc -----------\n");
  if (self->nullaudio) {
    delete self->nullaudio;
    self->nullaudio = NULL;
  }

  self->ob_type->tp_free((PyObject*)self);
}


static PyObject* IvrNullAudio_init(IvrNullAudio* self, PyObject* args)
{
  int read_msec = -1;
  int write_msec = -1;

  if(!PyArg_ParseTuple(args,"|ii", &read_msec, &write_msec))
    return NULL;

  self->nullaudio = new AmNullAudio(read_msec, write_msec);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrNullAudio_setReadLength(IvrNullAudio* self, PyObject* args)
{
  int read_msec = -1;

  if(!PyArg_ParseTuple(args,"i", &read_msec))
    return NULL;

  self->nullaudio->setReadLength(read_msec);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* IvrNullAudio_setWriteLength(IvrNullAudio* self, PyObject* args)
{
  int write_msec = -1;

  if(!PyArg_ParseTuple(args,"i", &write_msec))
    return NULL;

  self->nullaudio->setWriteLength(write_msec);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef IvrNullAudio_methods[] = {
  {"init", (PyCFunction)IvrNullAudio_init, METH_VARARGS,
   "nullaudio, plays silence, and recording goes to void, parameters: play and rec length in ms"
  },
  {"setReadLength", (PyCFunction)IvrNullAudio_setReadLength, METH_VARARGS,
   "(re) set maximum read length"
  },
  {"setWriteLength", (PyCFunction)IvrNullAudio_setWriteLength, METH_VARARGS,
   "(re) set maximum write length"
  },
  {NULL}  /* Sentinel */
};



static PyGetSetDef IvrNullAudio_getseters[] = {
  {NULL}  /* Sentinel */
};
    
PyTypeObject IvrNullAudioType = {
	
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrNullAudio",        /*tp_name*/
  sizeof(IvrNullAudio),      /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)IvrNullAudio_dealloc, /*tp_dealloc*/
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
  "A null audio file",       /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  IvrNullAudio_methods,      /* tp_methods */
  0,                         /* tp_members */
  IvrNullAudio_getseters,    /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrNullAudio_new,          /* tp_new */
};
