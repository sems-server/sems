/*
 * Copyright (C) 2007 iptego GmbH
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

#include "IvrSipReply.h"

#include "log.h"

/** \brief IVR wrapper of AmSipReply */
typedef struct {
    
  PyObject_HEAD
  AmSipReply* p_req;
} IvrSipReply;


// Constructor
static PyObject* IvrSipReply_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {(char*)"ivr_req", NULL};
  IvrSipReply *self;

  self = (IvrSipReply *)type->tp_alloc(type, 0);
  if (self != NULL) {
	
    PyObject* o_req = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &o_req)){
	    
      Py_DECREF(self);
      return NULL;
    }
    
    if ((NULL == o_req) || !PyCObject_Check(o_req)){
	    
      Py_DECREF(self);
      return NULL;
    }
	
    self->p_req = (AmSipReply*)PyCObject_AsVoidPtr(o_req);
  }

  DBG("IvrSipReply_new\n");
  return (PyObject *)self;
}

static void
IvrSipReply_dealloc(IvrSipReply* self) 
{
  delete self->p_req;
  self->ob_type->tp_free((PyObject*)self);
}

#define def_IvrSipReply_GETTER(getter_name, attr)		\
  static PyObject*						\
  getter_name(IvrSipReply *self, void *closure)			\
  {								\
    return PyString_FromString(self->p_req->attr.c_str());	\
  }								
								
								
def_IvrSipReply_GETTER(IvrSipReply_getreason,     reason)
def_IvrSipReply_GETTER(IvrSipReply_getcontact,    contact)
def_IvrSipReply_GETTER(IvrSipReply_gethdrs,       hdrs)
def_IvrSipReply_GETTER(IvrSipReply_getfrom_tag,   from_tag)
def_IvrSipReply_GETTER(IvrSipReply_getto_tag,     to_tag)
def_IvrSipReply_GETTER(IvrSipReply_getroute,      route)
//def_IvrSipReply_GETTER(IvrSipReply_getbody,       body)

static PyObject*
IvrSipReply_getcseq(IvrSipReply *self, void *closure)
{
  return PyInt_FromLong(self->p_req->cseq);
}

static PyObject*
IvrSipReply_getcode(IvrSipReply *self, void *closure)
{
  return PyInt_FromLong(self->p_req->code);
}

static PyGetSetDef IvrSipReply_getset[] = {
  {(char*)"code",     (getter)IvrSipReply_getcode, NULL, (char*)"code", NULL},
  {(char*)"reason",   (getter)IvrSipReply_getreason, NULL, (char*)"reason", NULL},
  {(char*)"contact",(getter)IvrSipReply_getcontact, NULL, (char*)"contact", NULL},
  {(char*)"route",    (getter)IvrSipReply_getroute, NULL, (char*)"route", NULL},
  {(char*)"hdrs",     (getter)IvrSipReply_gethdrs, NULL, (char*)"hdrs", NULL},
  //{(char*)"body",     (getter)IvrSipReply_getbody, NULL, (char*)"body", NULL},
  {(char*)"from_tag", (getter)IvrSipReply_getfrom_tag, NULL, (char*)"from_tag", NULL},
  {(char*)"to_tag",   (getter)IvrSipReply_getto_tag, NULL, (char*)"to_tag", NULL},
  {(char*)"cseq",     (getter)IvrSipReply_getcseq, NULL, (char*)"cseq", NULL},
  {NULL}  /* Sentinel */
};

PyTypeObject IvrSipReplyType = {
    
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrSipReply",        /*tp_name*/
  sizeof(IvrSipReply),      /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)IvrSipReply_dealloc,                         /*tp_dealloc*/
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
  "Wrapper class for AmSipReply", /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  0,                         /* tp_methods */
  0,                         /* tp_members */
  IvrSipReply_getset,       /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrSipReply_new,          /* tp_new */
};


PyObject* IvrSipReply_FromPtr(AmSipReply* req)
{
  PyObject* c_req = PyCObject_FromVoidPtr(req,NULL);
  PyObject* args = Py_BuildValue("(O)",c_req);
    
  PyObject* py_req = IvrSipReply_new(&IvrSipReplyType,args,NULL);
    
  Py_DECREF(args);
  Py_DECREF(c_req);

  return py_req;
}
