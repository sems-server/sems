/*
 * This file is part of SEMS, a free SIP media server.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PythonScriptThread.h"

#include <pythread.h>

#include "log.h"

namespace {

struct ScriptThreadGIL
{
  PyGILState_STATE gst;

  ScriptThreadGIL() { gst = PyGILState_Ensure(); }
  ~ScriptThreadGIL(){ PyGILState_Release(gst);   }
};

}

void PythonScriptThread::run() {
  ScriptThreadGIL gil;

  // stopped before it got here: don't start the script at all
  if(stop_requested) {
    DBG("PythonScriptThread - stopped before the python function was called.\n");
    return;
  }

  DBG("PythonScriptThread - calling python function.\n");
  py_thread_id = PyThread_get_thread_ident();
  in_call = true;
  PyObject* res = PyObject_CallObject(py_thread_object, NULL);
  in_call = false;

  if(!res) {
    // SystemExit is what on_stop() raises; a script may also sys.exit() to
    // end its thread. PyErr_Print() would exit the whole process on it, so
    // only print other exceptions.
    if(PyErr_ExceptionMatches(PyExc_SystemExit)) {
      DBG("PythonScriptThread - python function ended by SystemExit%s.\n",
	  stop_requested ? " (stop requested)" : "");
      PyErr_Clear();
    }
    else {
      ERROR("PythonScriptThread - python function raised an exception:\n");
      PyErr_Print();
    }
  }
  Py_XDECREF(res);

  DBG("PythonScriptThread - thread finished..\n");
}

void PythonScriptThread::on_stop() {
  DBG("PythonScriptThread::on_stop.\n");

  // With the GIL held, in_call tells for sure whether run() is inside the
  // callable: run() sets and clears it with the GIL held, too. Outside of
  // the call py_thread_id may no longer name the script's thread, and an
  // asynchronous exception must never land in some other Python thread.
  ScriptThreadGIL gil;
  stop_requested = true;
  if(in_call) {
    if(PyThreadState_SetAsyncExc(py_thread_id, PyExc_SystemExit) != 1) {
      WARN("PythonScriptThread - could not raise SystemExit in the script's thread\n");
    }
  }
}
