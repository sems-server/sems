#ifndef IvrNullAudio_h
#define IvrNullAudio_h

// Python stuff
#include <Python.h>
#include <structmember.h>

#include "AmAdvancedAudio.h"

/** \brief python IVR wrapper for AmNullAudio */ 
typedef struct {
    
  PyObject_HEAD
  AmNullAudio* nullaudio;
    
} IvrNullAudio;

extern PyTypeObject IvrNullAudioType;

#endif
