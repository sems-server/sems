#ifndef IvrAudio_h
#define IvrAudio_h

// Python stuff
#include <Python.h>
#include <structmember.h>

#include "AmAudio.h"
#include "AmAudioFile.h"

#define AUDIO_READ  1
#define AUDIO_WRITE 2

#ifdef IVR_WITH_TTS
#include "flite.h"
#endif

/** \brief IVR wrapper of AmAudioFile */
typedef struct {
    
  PyObject_HEAD
  AmAudioFile* af;

#ifdef IVR_WITH_TTS
  cst_voice* tts_voice;
  string*    filename;
  bool       del_file;
#endif

  PyObject* py_file;
    
} IvrAudioFile;

extern PyTypeObject IvrAudioFileType;

#endif
