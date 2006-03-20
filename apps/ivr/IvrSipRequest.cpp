#include "IvrSipRequest.h"
//#include "AmSessionTimer.h"
#include "AmSipRequest.h"

#if 0
// Data definition
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

#define def_IvrSipRequest_GETTER(getter_name, attr) \
static PyObject* \
getter_name(IvrSipRequest *self, void *closure) \
{ \
  return PyString_FromString(self->p_req->attr.c_str()); \
} \

def_IvrSipRequest_GETTER(IvrSipRequest_getuser,         user)
def_IvrSipRequest_GETTER(IvrSipRequest_getdomain,       domain)
def_IvrSipRequest_GETTER(IvrSipRequest_getsip_ip,       sip_ip)
def_IvrSipRequest_GETTER(IvrSipRequest_getsip_port,     sip_port)
def_IvrSipRequest_GETTER(IvrSipRequest_getlocal_uri,    local_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getremote_uri,   remote_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getcontact_uri,  contact_uri)
def_IvrSipRequest_GETTER(IvrSipRequest_getcallid,       callid)
def_IvrSipRequest_GETTER(IvrSipRequest_getremote_tag,   remote_tag)
def_IvrSipRequest_GETTER(IvrSipRequest_getlocal_tag,    local_tag)
def_IvrSipRequest_GETTER(IvrSipRequest_getremote_party, remote_party)
def_IvrSipRequest_GETTER(IvrSipRequest_getlocal_party,  local_party)
def_IvrSipRequest_GETTER(IvrSipRequest_getroute,        route)
def_IvrSipRequest_GETTER(IvrSipRequest_getnext_hop,     next_hop)

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
    {"user",        (getter)IvrSipRequest_getuser, NULL, "local user", NULL},
    {"domain",      (getter)IvrSipRequest_getdomain, NULL, "local domain", NULL},
    {"sip_ip",      (getter)IvrSipRequest_getsip_ip, NULL, "destination IP of first received message", NULL},
    {"sip_port",    (getter)IvrSipRequest_getsip_port, NULL, "optional: SIP port", NULL},
    {"local_uri",   (getter)IvrSipRequest_getlocal_uri, NULL, "local uri", NULL},
    {"remote_uri",  (getter)IvrSipRequest_getremote_uri, NULL, "remote uri", NULL},
    {"contact_uri", (getter)IvrSipRequest_getcontact_uri, NULL, "pre-calculated contact uri", NULL},
    {"callid",      (getter)IvrSipRequest_getcallid, NULL, "call id", NULL},
    {"remote_tag",  (getter)IvrSipRequest_getremote_tag, NULL, "remote tag", NULL},
    {"local_tag",   (getter)IvrSipRequest_getlocal_tag, NULL, "local tag", NULL},
    {"remote_party",(getter)IvrSipRequest_getremote_party, NULL, "To/From", NULL},
    {"local_party", (getter)IvrSipRequest_getlocal_party, NULL, "To/From", NULL},
    {"route",       (getter)IvrSipRequest_getroute, NULL, "record routing", NULL},
    {"next_hop",    (getter)IvrSipRequest_getnext_hop, NULL, "next_hop for t_uac_dlg", NULL},
    {"cseq",    (getter)IvrSipRequest_getcseq, NULL, "CSeq for next request", NULL},
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

#endif
