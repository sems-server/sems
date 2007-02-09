#include "PySemsAudio.h"
#include "AmAudio.h"
#include "AmSession.h"

#include "log.h"

#ifdef IVR_WITH_TTS

#define TTS_CACHE_PATH "/tmp/"
extern "C" cst_voice *register_cmu_us_kal();

#endif //ivr_with_tts


static PyObject* PySemsAudioFile_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DBG("---------- PySemsAudioFile_alloc -----------\n");
    PySemsAudioFile *self;

    self = (PySemsAudioFile *)type->tp_alloc(type, 0);
	
    if (self != NULL) {

	self->af = new AmAudioFile();
	if(!self->af){
	    Py_DECREF(self);
	    return NULL;
	}

#ifdef IVR_WITH_TTS
	flite_init();
	self->tts_voice = register_cmu_us_kal();
	self->filename = new string();
#endif

    }

    return (PyObject *)self;
}

static void PySemsAudioFile_dealloc(PySemsAudioFile* self)
{
    DBG("---------- PySemsAudioFile_dealloc -----------\n");
    delete self->af;
    self->af = NULL;

#ifdef IVR_WITH_TTS
    if(self->del_file && !self->filename->empty())
	unlink(self->filename->c_str());
    delete self->filename;
#endif

    self->ob_type->tp_free((PyObject*)self);
}

static PyObject* PySemsAudioFile_open(PySemsAudioFile* self, PyObject* args)
{
    int                   ivr_open_mode;
    char*                 filename;
    bool                  is_tmp;
    PyObject*             py_is_tmp = NULL;
    AmAudioFile::OpenMode open_mode;

    if(!PyArg_ParseTuple(args,"si|O",&filename,&ivr_open_mode,&py_is_tmp))
	return NULL;

    switch(ivr_open_mode){
    case AUDIO_READ:
	open_mode = AmAudioFile::Read;
	break;
    case AUDIO_WRITE:
	open_mode = AmAudioFile::Write;
	break;
    default:
	PyErr_SetString(PyExc_TypeError,"Unknown open mode");
	return NULL;
	break;
    }

    if((py_is_tmp == NULL) || (py_is_tmp == Py_False))
	is_tmp = false;
    else if(py_is_tmp == Py_True)
	is_tmp = true;
    else {
	PyErr_SetString(PyExc_TypeError,"third parameter should be of type PyBool");
	return NULL;
    }

    if(self->af->open(filename,open_mode,is_tmp)){
	PyErr_SetString(PyExc_IOError,"Could not open file");
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* PySemsAudioFile_fpopen(PySemsAudioFile* self, PyObject* args)
{
    int                   ivr_open_mode;
    char*                 filename;
    PyObject*             py_file = NULL;
    AmAudioFile::OpenMode open_mode;

    if(!PyArg_ParseTuple(args,"siO",&filename,&ivr_open_mode,&py_file))
	return NULL;

    switch(ivr_open_mode){
    case AUDIO_READ:
	open_mode = AmAudioFile::Read;
	break;
    case AUDIO_WRITE:
	open_mode = AmAudioFile::Write;
	break;
    default:
	PyErr_SetString(PyExc_TypeError,"Unknown open mode");
	return NULL;
	break;
    }

    FILE* fp = PyFile_AsFile(py_file);
    if(!fp){
	PyErr_SetString(PyExc_IOError,"Could not get FILE pointer");
	return NULL;
    }

    if(self->af->fpopen(filename,open_mode,fp)){
	PyErr_SetString(PyExc_IOError,"Could not open file");
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* PySemsAudioFile_rewind(PySemsAudioFile* self, PyObject* args)
{
    self->af->rewind();
    Py_INCREF(Py_None);
    return Py_None;
}

#ifdef IVR_WITH_TTS
static PyObject* PySemsAudioFile_tts(PyObject* cls, PyObject* args)
{
    char* text;
    if(!PyArg_ParseTuple(args,"s",&text))
	return NULL;
    
    PyObject* constr_args = Py_BuildValue("(O)",Py_None);
    PyObject* tts_file = PyObject_CallObject(cls,constr_args);
    Py_DECREF(constr_args);

    if(tts_file == NULL){
	PyErr_Print();
	PyErr_SetString(PyExc_RuntimeError,"could not create new PySemsAudioFile object");
	return NULL;
    }

    PySemsAudioFile* self = (PySemsAudioFile*)tts_file;

    *self->filename = string(TTS_CACHE_PATH) + AmSession::getNewId() + string(".wav");
    self->del_file = true;
    flite_text_to_speech(text,self->tts_voice,self->filename->c_str());
    
    if(self->af->open(self->filename->c_str(),AmAudioFile::Read)){
	Py_DECREF(tts_file);
	PyErr_SetString(PyExc_IOError,"could not open TTS file");
	return NULL;
    }

    return tts_file;
}
#endif
    
static PyObject* PySemsAudioFile_close(PySemsAudioFile* self, PyObject*)
{
    self->af->close();
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* PySemsAudioFile_getDataSize(PySemsAudioFile* self, PyObject*)
{
    return PyInt_FromLong(self->af->getDataSize());
}

static PyObject* PySemsAudioFile_setRecordTime(PySemsAudioFile* self, PyObject* args)
{
    int rec_time;
    if(!PyArg_ParseTuple(args,"i",&rec_time))
	return NULL;

    self->af->setRecordTime(rec_time);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* PySemsAudioFile_exportRaw(PySemsAudioFile* self, PyObject*)
{
    if(self->af->getMode() == AmAudioFile::Write)
	self->af->on_close();
    
    self->af->rewind();

    return PyFile_FromFile(self->af->getfp(),"","rwb",NULL);
}


static PyMethodDef PySemsAudioFile_methods[] = {
    {"open", (PyCFunction)PySemsAudioFile_open, METH_VARARGS,
     "open the audio file"
    },
    {"fpopen", (PyCFunction)PySemsAudioFile_fpopen, METH_VARARGS,
     "open the audio file"
    },
    {"close", (PyCFunction)PySemsAudioFile_close, METH_NOARGS,
     "close the audio file"
    },
    {"rewind", (PyCFunction)PySemsAudioFile_rewind, METH_NOARGS,
     "rewind the audio file"
    },
    {"getDataSize", (PyCFunction)PySemsAudioFile_getDataSize, METH_NOARGS,
     "returns the recorded data size"
    },
    {"setRecordTime", (PyCFunction)PySemsAudioFile_setRecordTime, METH_VARARGS,
     "set the maximum record time in millisecond"
    },
    {"exportRaw", (PyCFunction)PySemsAudioFile_exportRaw, METH_NOARGS,
     "creates a new Python file with the actual file"
     " and eventually flushes headers (audio->on_stop)"
    },
#ifdef IVR_WITH_TTS
    {"tts", (PyCFunction)PySemsAudioFile_tts, METH_CLASS | METH_VARARGS,
     "text to speech"
    },
#endif
    {NULL}  /* Sentinel */
};


static PyObject* PySemsAudioFile_getloop(PySemsAudioFile* self, void*)
{
    PyObject* loop = self->af->loop.get() ? Py_True : Py_False;
    Py_INCREF(loop);
    return loop;
}

static int PySemsAudioFile_setloop(PySemsAudioFile* self, PyObject* value, void*)
{
    if (value == NULL) {
	PyErr_SetString(PyExc_TypeError, "Cannot delete the first attribute");
	return -1;
    }
  
    if(value == Py_True)
	self->af->loop.set(true);

    else if(value == Py_False)
	self->af->loop.set(false);

    else {
	PyErr_SetString(PyExc_TypeError, 
			"The first attribute value must be a boolean");
	return -1;
    }

    return 0;
}

static PyGetSetDef PySemsAudioFile_getseters[] = {
    {"loop", 
     (getter)PySemsAudioFile_getloop, (setter)PySemsAudioFile_setloop,
     "repeat mode",
     NULL},
    {NULL}  /* Sentinel */
};
    
PyTypeObject PySemsAudioFileType = {
	
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "ivr.PySemsAudioFile",        /*tp_name*/
    sizeof(PySemsAudioFile),      /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PySemsAudioFile_dealloc, /*tp_dealloc*/
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
    PySemsAudioFile_methods,      /* tp_methods */
    0,                         /* tp_members */
    PySemsAudioFile_getseters,    /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PySemsAudioFile_new,          /* tp_new */
};
