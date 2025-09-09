/* Copyright (C) 2006 Stefan Sayer
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

#include "IvrUAC.h"
#include "AmUAC.h"

#include "log.h"

static PyObject* IvrUAC_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  IvrUAC *self;

  self = (IvrUAC *)type->tp_alloc(type, 0);
	
  return (PyObject *)self;
}

static void IvrUAC_dealloc(IvrUAC* self)
{
  self->ob_base.ob_type->tp_free((PyObject*)self);
}

static PyObject* IvrUAC_dialout(IvrUAC* self, PyObject* args)
{ 
  char* user;
  char* app_name;
  char* r_uri;
  char* from;
  char* from_uri;
  char* to;
  const char* local_tag = "";
  const char* hdrs = "";
  PyObject *sp = NULL;

  if(!PyArg_ParseTuple(args,"ssssss|ssO", &user, &app_name, &r_uri,
        &from, &from_uri, &to, &local_tag, &hdrs, &sp))
    return NULL;

  AmArg* session_params = NULL;
  if(sp) {
    if(PyList_Check(sp)) {
      session_params = new AmArg();
      session_params->assertArray();

      int size = PyList_Size(sp);
      for (int ii = 0; ii < size; ii++) {
        PyObject *item = PyList_GetItem(sp, ii);
        const char *str = PyUnicode_AsUTF8(item);
        session_params->push(string(str));
      }
    } else if(PyDict_Check(sp)) {
      session_params = new AmArg();
      session_params->assertStruct();

      PyObject *key, *value;
      Py_ssize_t pos = 0;
      while (PyDict_Next(sp, &pos, &key, &value)) {
        if(PyUnicode_Check(value))
          (*session_params)[PyUnicode_AsUTF8(key)] = PyUnicode_AsUTF8(value);
      }
    }
  }

  AmUAC::dialout(user, app_name, r_uri, from, from_uri, to,
      local_tag, hdrs, session_params);

  Py_INCREF(Py_None);
  return Py_None;
}
    
static PyMethodDef IvrUAC_methods[] = {
  {"dialout", (PyCFunction)IvrUAC_dialout, METH_VARARGS,
   "place a new call"
  },
  {NULL}  /* Sentinel */
};

PyTypeObject IvrUACType = {	
  PyObject_HEAD_INIT(NULL)
  "ivr.IvrUAC",              /*tp_name*/
  sizeof(IvrUAC),            /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)IvrUAC_dealloc, /*tp_dealloc*/
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
  "UAC  Class",              /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  IvrUAC_methods,            /* tp_methods */
  0,                         /* tp_members */
  0,                         /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrUAC_new,                /* tp_new */
  0,                         /* tp_free */
  0,                         /* *tp_is_gc */
  0,                         /* tp_bases */
  0,                         /* tp_mro */
  0,                         /* tp_cache */
  0,                         /* tp_subclasses */
  0,                         /* tp_weaklist */
  0,                         /* tp_del */
  0,                         /* tp_version_tag */
  nullptr,                   /* tp_finalize */
};
