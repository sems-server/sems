#ifndef IvrSipDialog_h
#define IvrSipDialog_h

// Python stuff
#include <Python.h>
#include "structmember.h"

extern PyTypeObject IvrSipDialogType;

class AmSipDialog;
PyObject* IvrSipDialog_FromPtr(AmSipDialog* dlg);

#endif
