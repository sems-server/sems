#ifndef PySemsAudio_h
#define PySemsAudio_h

// Python stuff
#include <Python.h>
#include "structmember.h"

#include "AmAudioFile.h"

#define AUDIO_READ  1
#define AUDIO_WRITE 2

#ifdef IVR_WITH_TTS
#include "flite.h"
#endif

/** \brief pySems wrapper for AmAudioFile  */
typedef struct {
    
  PyObject_HEAD
  AmAudioFile* af;

#ifdef IVR_WITH_TTS
  cst_voice* tts_voice;
  string*    filename;
  bool       del_file;
#endif
    
} PySemsAudioFile;

extern PyTypeObject PySemsAudioFileType;

#endif
