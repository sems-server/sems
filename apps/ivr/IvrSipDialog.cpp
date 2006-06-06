#include "IvrSipDialog.h"
//#include "AmSessionTimer.h"
#include "AmSipDialog.h"
#include "log.h"

// Data definition
typedef struct {
    
  PyObject_HEAD
  AmSipDialog* p_dlg;
} IvrSipDialog;

// Constructor
static PyObject* IvrSipDialog_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"ivr_dlg", NULL};
    IvrSipDialog *self;

    self = (IvrSipDialog *)type->tp_alloc(type, 0);
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

#define def_IvrSipDialog_GETTER(getter_name, attr) \
static PyObject* \
getter_name(IvrSipDialog *self, void *closure) \
{ \
  return PyString_FromString(self->p_dlg->attr.c_str()); \
} \

def_IvrSipDialog_GETTER(IvrSipDialog_getuser,         user)
def_IvrSipDialog_GETTER(IvrSipDialog_getdomain,       domain)
def_IvrSipDialog_GETTER(IvrSipDialog_getsip_ip,       sip_ip)
def_IvrSipDialog_GETTER(IvrSipDialog_getsip_port,     sip_port)
def_IvrSipDialog_GETTER(IvrSipDialog_getlocal_uri,    local_uri)
def_IvrSipDialog_GETTER(IvrSipDialog_getremote_uri,   remote_uri)
def_IvrSipDialog_GETTER(IvrSipDialog_getcontact_uri,  contact_uri)
def_IvrSipDialog_GETTER(IvrSipDialog_getcallid,       callid)
def_IvrSipDialog_GETTER(IvrSipDialog_getremote_tag,   remote_tag)
def_IvrSipDialog_GETTER(IvrSipDialog_getlocal_tag,    local_tag)
def_IvrSipDialog_GETTER(IvrSipDialog_getremote_party, remote_party)
def_IvrSipDialog_GETTER(IvrSipDialog_getlocal_party,  local_party)
def_IvrSipDialog_GETTER(IvrSipDialog_getroute,        getRoute())
def_IvrSipDialog_GETTER(IvrSipDialog_getnext_hop,     next_hop)

// static PyObject*
// IvrSipDialog_getuser(IvrSipDialog *self, void *closure)
// {
//   return PyString_FromString(self->p_dlg->user.c_str());
// }

static PyObject*
IvrSipDialog_getcseq(IvrSipDialog *self, void *closure)
{
  return PyInt_FromLong(self->p_dlg->cseq);
}

static PyGetSetDef IvrSipDialog_getset[] = {
    {"user",        (getter)IvrSipDialog_getuser, NULL, "local user", NULL},
    {"domain",      (getter)IvrSipDialog_getdomain, NULL, "local domain", NULL},
    {"sip_ip",      (getter)IvrSipDialog_getsip_ip, NULL, "destination IP of first received message", NULL},
    {"sip_port",    (getter)IvrSipDialog_getsip_port, NULL, "optional: SIP port", NULL},
    {"local_uri",   (getter)IvrSipDialog_getlocal_uri, NULL, "local uri", NULL},
    {"remote_uri",  (getter)IvrSipDialog_getremote_uri, NULL, "remote uri", NULL},
    {"contact_uri", (getter)IvrSipDialog_getcontact_uri, NULL, "pre-calculated contact uri", NULL},
    {"callid",      (getter)IvrSipDialog_getcallid, NULL, "call id", NULL},
    {"remote_tag",  (getter)IvrSipDialog_getremote_tag, NULL, "remote tag", NULL},
    {"local_tag",   (getter)IvrSipDialog_getlocal_tag, NULL, "local tag", NULL},
    {"remote_party",(getter)IvrSipDialog_getremote_party, NULL, "To/From", NULL},
    {"local_party", (getter)IvrSipDialog_getlocal_party, NULL, "To/From", NULL},
    {"route",       (getter)IvrSipDialog_getroute, NULL, "record routing", NULL},
    {"next_hop",    (getter)IvrSipDialog_getnext_hop, NULL, "next_hop for t_uac_dlg", NULL},
    {"cseq",    (getter)IvrSipDialog_getcseq, NULL, "CSeq for next request", NULL},
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
