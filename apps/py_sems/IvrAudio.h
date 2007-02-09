#ifndef IvrAudio_h
#define IvrAudio_h

// Python stuff
#include <Python.h>
#include "structmember.h"

#include "AmAudio.h"

#define AUDIO_READ  1
#define AUDIO_WRITE 2

#ifdef IVR_WITH_TTS
#include "flite.h"
#endif

// Data definition
typedef struct {
    
    PyObject_HEAD
    AmAudioFile* af;

#ifdef IVR_WITH_TTS
    cst_voice* tts_voice;
    string*    filename;
    bool       del_file;
#endif
    
} IvrAudioFile;

extern PyTypeObject IvrAudioFileType;

#endif
