/*
 * $Id$
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2007 iptego GmbH
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

#include "IvrSipRequest.h"

#include "log.h"

// Data definition
/** \brief IVR wrapper of AmSipRequest */
typedef struct {
    
  PyObject_HEAD
  AmSipRequest* p_req;
} IvrSipRequest;


// Constructor
static PyObject* IvrSipRequest_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {"ivr_req", NULL};
  IvrSipRequest *self;

  self = (IvrSipRequest *)type->tp_alloc(type, 0);
  if (self != NULL) {
	
    PyObject* o_req = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &o_req)){
	    
      Py_DECREF(self);
      return NULL;
    }
    
    if (!PyCObject_Check(o_req)){
	    
      Py_DECREF(self);
      return NULL;
    }
	
    self->p_req = (AmSipRequest*)PyCObject_AsVoidPtr(o_req);
  }

  DBG("IvrSipRequest_new\n");
  return (PyObject *)self;
}

// static void
// IvrSipRequest_dealloc(IvrSipRequest* self) 
// {
//   self->ob_type->tp_free((PyObject*)self);
// }

#define def_IvrSipRequest_GETTER(getter_name, attr)		\
  static PyObject*						\
  getter_name(IvrSipRequest *self, void *closure)		\
  {								\
    return PyString_FromString(self->p_req->attr.c_str());	\
  }					  
								
def_IvrSipRequest_GETTER(IvrSipRequest_getmethod,       method)
def_IvrSipRequest_GETTER(IvrSipRequest_getuser,         user)
def_IvrSipRequest_GETTER(IvrSipRequest_getdomain,       domain)
def_IvrSipRequest_GETTER(IvrSipRequest_getdstip,        dstip)
def_IvrSipRequest_GETTER(IvrSipRequest_getport,         port)
def_IvrSipRequest_GETTER(IvrSipRequest_getr_uri,        r_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getfrom_uri,     from_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getfrom,         from)
def_IvrSipRequest_GETTER(IvrSipRequest_getto,           to)
def_IvrSipRequest_GETTER(IvrSipRequest_getcallid,       callid)
def_IvrSipRequest_GETTER(IvrSipRequest_getfrom_tag,     from_tag)
def_IvrSipRequest_GETTER(IvrSipRequest_getto_tag,       to_tag)

def_IvrSipRequest_GETTER(IvrSipRequest_getroute,        route)
def_IvrSipRequest_GETTER(IvrSipRequest_getnext_hop,     next_hop)

def_IvrSipRequest_GETTER(IvrSipRequest_getbody,         body)


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

static PyGetSetDef IvrSipRequest_getset[] = {
  {"method",        (getter)IvrSipRequest_getmethod, NULL, "method", NULL},
  {"user",          (getter)IvrSipRequest_getuser, NULL, "local user", NULL},
  {"domain",        (getter)IvrSipRequest_getdomain, NULL, "local domain", NULL},
  {"dstip",         (getter)IvrSipRequest_getdstip, NULL, "dstip", NULL},
  {"port",          (getter)IvrSipRequest_getport, NULL, "port", NULL},

  {"r_uri",         (getter)IvrSipRequest_getr_uri, NULL, "port", NULL},
  {"from_uri",      (getter)IvrSipRequest_getfrom_uri, NULL, "port", NULL},
  {"from",          (getter)IvrSipRequest_getfrom, NULL, "port", NULL},
  {"to",            (getter)IvrSipRequest_getto, NULL, "port", NULL},


  {"callid",        (getter)IvrSipRequest_getcallid, NULL, "call id", NULL},
  {"from_tag",      (getter)IvrSipRequest_getfrom_tag, NULL, "remote tag", NULL},
  {"to_tag",        (getter)IvrSipRequest_getto_tag, NULL, "local tag", NULL},
  {"route",       (getter)IvrSipRequest_getroute, NULL, "record routing", NULL},
  {"next_hop",    (getter)IvrSipRequest_getnext_hop, NULL, "next_hop for t_uac_dlg", NULL},
  {"cseq",    (getter)IvrSipRequest_getcseq, NULL, "CSeq for next request", NULL},
  {"body",    (getter)IvrSipRequest_getbody, NULL, "CSeq for next request", NULL},
  {NULL}  /* Sentinel */
};

PyTypeObject IvrSipRequestType = {
    
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrSipRequest",        /*tp_name*/
  sizeof(IvrSipRequest),      /*tp_basicsize*/
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
