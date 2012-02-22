/* Copyright (C) 2002-2003 Fhg Fokus
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

#include "IvrSipRequest.h"

#include "log.h"

// Data definition
/** \brief IVR wrapper of AmSipRequest */
typedef struct {
    
  PyObject_HEAD
  AmSipRequest* p_req;
  bool own_p_req;
} IvrSipRequest;


// Constructor
static PyObject* IvrSipRequest_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {(char*)"ivr_req", NULL};
  IvrSipRequest *self;

  self = (IvrSipRequest *)type->tp_alloc(type, 0);
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
	
    self->p_req = (AmSipRequest*)PyCObject_AsVoidPtr(o_req);
    self->own_p_req = true;
  }

  DBG("IvrSipRequest_new\n");
  return (PyObject *)self;
}

static PyObject* IvrSipRequest_newRef(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {(char*)"ivr_req", NULL};
  IvrSipRequest *self;

  self = (IvrSipRequest *)type->tp_alloc(type, 0);
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
	
    self->p_req = (AmSipRequest*)PyCObject_AsVoidPtr(o_req);
    self->own_p_req = false;
  }

  DBG("IvrSipRequest_newRef\n");
  return (PyObject *)self;
}

static void
IvrSipRequest_dealloc(IvrSipRequest* self) 
{
  DBG("IvrSipRequest_dealloc\n");

  if(self->own_p_req)
    delete self->p_req;

  self->ob_type->tp_free((PyObject*)self);
}

#define def_IvrSipRequest_GETTER(getter_name, attr)		\
  static PyObject*						\
  getter_name(IvrSipRequest *self, void *closure)		\
  {								\
    return PyString_FromString(self->p_req->attr.c_str());	\
  }					  
								
def_IvrSipRequest_GETTER(IvrSipRequest_getmethod,       method)
def_IvrSipRequest_GETTER(IvrSipRequest_getuser,         user)
def_IvrSipRequest_GETTER(IvrSipRequest_getdomain,       domain)
def_IvrSipRequest_GETTER(IvrSipRequest_getr_uri,        r_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getfrom_uri,     from_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getfrom,         from)
def_IvrSipRequest_GETTER(IvrSipRequest_getto,           to)
def_IvrSipRequest_GETTER(IvrSipRequest_getcallid,       callid)
def_IvrSipRequest_GETTER(IvrSipRequest_getfrom_tag,     from_tag)
def_IvrSipRequest_GETTER(IvrSipRequest_getto_tag,       to_tag)
def_IvrSipRequest_GETTER(IvrSipRequest_getroute,        route)
//def_IvrSipRequest_GETTER(IvrSipRequest_getbody,         body)
def_IvrSipRequest_GETTER(IvrSipRequest_gethdrs,         hdrs)

#undef def_IvrSipRequest_GETTER
// static PyObject*
// IvrSipRequest_getuser(IvrSipRequest *self, void *closure)
// {
//   return PyString_FromString(self->p_req->user.c_str());
// }

static PyObject*
IvrSipRequest_getcseq(IvrSipRequest *self, void *closure)
{
  return PyInt_FromLong(self->p_req->cseq);
}

#define def_IvrSipRequest_SETTER(setter_name, attr)			\
  static int								\
  setter_name(IvrSipRequest *self, PyObject* value, void *closure)	\
  {									\
    char* text;								\
    if(!PyArg_Parse(value,"s",&text))					\
      return -1;							\
									\
    self->p_req->attr = text;						\
    return 0;								\
  } 

def_IvrSipRequest_SETTER(IvrSipRequest_sethdrs,   hdrs)

#undef def_IvrSipRequest_SETTER

static PyGetSetDef IvrSipRequest_getset[] = {
  {(char*)"method", (getter)IvrSipRequest_getmethod, NULL, (char*)"method", NULL},
  {(char*)"user",   (getter)IvrSipRequest_getuser, NULL, (char*)"local user", NULL},
  {(char*)"domain", (getter)IvrSipRequest_getdomain, NULL, (char*)"local domain", NULL},
  {(char*)"r_uri",  (getter)IvrSipRequest_getr_uri, NULL, (char*)"port", NULL},
  {(char*)"from_uri", (getter)IvrSipRequest_getfrom_uri, NULL, (char*)"port", NULL},
  {(char*)"from",   (getter)IvrSipRequest_getfrom, NULL, (char*)"port", NULL},
  {(char*)"to",     (getter)IvrSipRequest_getto, NULL, (char*)"port", NULL},
  {(char*)"callid",  (getter)IvrSipRequest_getcallid, NULL, (char*)"call id", NULL},
  {(char*)"from_tag", (getter)IvrSipRequest_getfrom_tag, NULL, (char*)"remote tag", NULL},
  {(char*)"to_tag", (getter)IvrSipRequest_getto_tag, NULL, (char*)"local tag", NULL},
  {(char*)"route",  (getter)IvrSipRequest_getroute, NULL, (char*)"record routing", NULL},
  {(char*)"cseq",   (getter)IvrSipRequest_getcseq, NULL, (char*)"CSeq for next request", NULL},
  //{(char*)"body",   (getter)IvrSipRequest_getbody, NULL, (char*)"Body", NULL},
  {(char*)"hdrs",   (getter)IvrSipRequest_gethdrs, (setter)IvrSipRequest_sethdrs, (char*)"Additional headers", NULL},
  {NULL}  /* Sentinel */
};

PyTypeObject IvrSipRequestType = {
    
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrSipRequest",        /*tp_name*/
  sizeof(IvrSipRequest),      /*tp_basicsize*/
  0,                         /*tp_itemsize*/
    (destructor)IvrSipRequest_dealloc,                         /*tp_dealloc*/
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
  "Wrapper class for AmSipRequest", /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  0,                         /* tp_methods */
  0,                         /* tp_members */
  IvrSipRequest_getset,       /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrSipRequest_new,          /* tp_new */
};


PyObject* IvrSipRequest_FromPtr(AmSipRequest* req)
{
  PyObject* c_req = PyCObject_FromVoidPtr(req,NULL);
  PyObject* args = Py_BuildValue("(O)",c_req);
    
  PyObject* py_req = IvrSipRequest_new(&IvrSipRequestType,args,NULL);
    
  Py_DECREF(args);
  Py_DECREF(c_req);

  return py_req;
}

PyObject* IvrSipRequest_BorrowedFromPtr(AmSipRequest* req)
{
  PyObject* c_req = PyCObject_FromVoidPtr(req,NULL);
  PyObject* args = Py_BuildValue("(O)",c_req);
    
  PyObject* py_req = IvrSipRequest_newRef(&IvrSipRequestType,args,NULL);
    
  Py_DECREF(args);
  Py_DECREF(c_req);

  return py_req;
}
