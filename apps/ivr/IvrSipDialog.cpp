#include "IvrSipDialog.h"
//#include "AmSessionTimer.h"
#include "AmSipDialog.h"
#include "log.h"

/** \brief IVR wrapper class of AmSipDialog */
typedef struct {
    
  PyObject_HEAD
  AmSipDialog* p_dlg;
} IvrSipDialog;

// Constructor
static PyObject* IvrSipDialog_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {(char*)"ivr_dlg", NULL};
  IvrSipDialog *self;

  self = (IvrSipDialog *)type->tp_alloc(type, 0);
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
	
    self->p_dlg = (AmSipDialog*)PyCObject_AsVoidPtr(o_dlg);
  }

  DBG("IvrSipDialog_new\n");
  return (PyObject *)self;
}

// static void
// IvrSipDialog_dealloc(IvrSipDialog* self) 
// {
//   self->ob_type->tp_free((PyObject*)self);
// }

#define def_IvrSipDialog_GETTER(getter_name, attr)		\
  static PyObject*						\
  getter_name(IvrSipDialog *self, void *closure)		\
  {								\
    return PyString_FromString(self->p_dlg->attr.c_str());	\
  }								\
								
def_IvrSipDialog_GETTER(IvrSipDialog_getuser,         getUser())
def_IvrSipDialog_GETTER(IvrSipDialog_getdomain,       getDomain())
def_IvrSipDialog_GETTER(IvrSipDialog_getlocal_uri,    getLocalUri())
def_IvrSipDialog_GETTER(IvrSipDialog_getremote_uri,   getRemoteUri())
//def_IvrSipDialog_GETTER(IvrSipDialog_getcontact_uri,  contact_uri)
def_IvrSipDialog_GETTER(IvrSipDialog_getcallid,       getCallid())
def_IvrSipDialog_GETTER(IvrSipDialog_getremote_tag,   getRemoteTag())
def_IvrSipDialog_GETTER(IvrSipDialog_getlocal_tag,    getLocalTag())
def_IvrSipDialog_GETTER(IvrSipDialog_getremote_party, getRemoteParty())
def_IvrSipDialog_GETTER(IvrSipDialog_getlocal_party,  getLocalParty())
def_IvrSipDialog_GETTER(IvrSipDialog_getroute,        getRoute())
def_IvrSipDialog_GETTER(IvrSipDialog_getoutbound_proxy, outbound_proxy)

#define def_IvrSipDialog_SETTER(setter_name, attr)			\
  static int								\
  setter_name(IvrSipDialog *self, PyObject* value, void *closure)	\
  {									\
    char* text;								\
    if(!PyArg_Parse(value,"s",&text))					\
      return -1;							\
									\
    self->p_dlg->attr(text);						\
    return 0;								\
  } 

def_IvrSipDialog_SETTER(IvrSipDialog_setremote_uri,   setRemoteUri)

static PyObject*
IvrSipDialog_getcseq(IvrSipDialog *self, void *closure)
{
  return PyInt_FromLong(self->p_dlg->cseq);
}

static PyObject*
IvrSipDialog_getstatus(IvrSipDialog *self, void *closure)
{
  return PyInt_FromLong((int)self->p_dlg->getStatus());
}

static PyObject*
IvrSipDialog_getstatusstr(IvrSipDialog *self, void *closure)
{
  return PyString_FromString((char*)self->p_dlg->getStatusStr());
}

static PyGetSetDef IvrSipDialog_getset[] = {
  {(char*)"user",        (getter)IvrSipDialog_getuser, NULL, (char*)"local user", NULL},
  {(char*)"domain",      (getter)IvrSipDialog_getdomain, NULL, (char*)"local domain", NULL},
  {(char*)"local_uri",   (getter)IvrSipDialog_getlocal_uri, NULL, (char*)"local uri", NULL},
  {(char*)"remote_uri",  (getter)IvrSipDialog_getremote_uri, (setter)IvrSipDialog_setremote_uri, (char*)"remote uri", NULL},
  //{(char*)"contact_uri", (getter)IvrSipDialog_getcontact_uri, NULL, (char*)"pre-calculated contact uri", NULL},
  {(char*)"callid",      (getter)IvrSipDialog_getcallid, NULL, (char*)"call id", NULL},
  {(char*)"remote_tag",  (getter)IvrSipDialog_getremote_tag, NULL, (char*)"remote tag", NULL},
  {(char*)"local_tag",   (getter)IvrSipDialog_getlocal_tag, NULL, (char*)"local tag", NULL},
  {(char*)"remote_party",(getter)IvrSipDialog_getremote_party, NULL, (char*)"To/From", NULL},
  {(char*)"local_party", (getter)IvrSipDialog_getlocal_party, NULL, (char*)"To/From", NULL},
  {(char*)"route",       (getter)IvrSipDialog_getroute, NULL, (char*)"record routing", NULL},
  {(char*)"outbound_proxy", (getter)IvrSipDialog_getoutbound_proxy, NULL, (char*)"outbound proxy", NULL},
  {(char*)"cseq",    (getter)IvrSipDialog_getcseq, NULL, (char*)"CSeq for next request", NULL},

  {(char*)"status_str",    (getter)IvrSipDialog_getstatusstr, NULL, (char*)"Dialog status "
   "(Disconnected, Trying, Proceeding, Cancelling, Early, Connected, Disconnecting)", NULL},
  {(char*)"status",    (getter)IvrSipDialog_getstatus, NULL, (char*)"Dialog status (0..6)", NULL},

  {NULL}  /* Sentinel */
};

PyTypeObject IvrSipDialogType = {
    
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "ivr.IvrSipDialog",        /*tp_name*/
  sizeof(IvrSipDialog),      /*tp_basicsize*/
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
  "Wrapper class for AmSipDialog", /*tp_doc*/
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  0,                         /* tp_methods */
  0,                         /* tp_members */
  IvrSipDialog_getset,       /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  0,                         /* tp_init */
  0,                         /* tp_alloc */
  IvrSipDialog_new,          /* tp_new */
};


PyObject* IvrSipDialog_FromPtr(AmSipDialog* dlg)
{
  PyObject* c_dlg = PyCObject_FromVoidPtr(dlg,NULL);
  PyObject* args = Py_BuildValue("(O)",c_dlg);
    
  PyObject* py_dlg = IvrSipDialog_new(&IvrSipDialogType,args,NULL);
    
  Py_DECREF(args);
  Py_DECREF(c_dlg);

  return py_dlg;
}
