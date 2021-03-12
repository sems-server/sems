#ifndef IvrAudioMixIn_h
#define IvrAudioMixIn_h

// Python stuff
#include <Python.h>
#include <structmember.h>

#include "AmAudioMixIn.h"

/** \brief python IVR wrapper for AmAudioMixIn */ 
typedef struct {
    
  PyObject_HEAD
  AmAudioMixIn* mix;
    
} IvrAudioMixIn;

extern PyTypeObject IvrAudioMixInType;

#endif
