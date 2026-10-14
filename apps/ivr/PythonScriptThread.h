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

#ifndef _PYTHON_SCRIPT_THREAD_H_
#define _PYTHON_SCRIPT_THREAD_H_

#include <Python.h>

#include "AmThread.h"

/**
 * \brief C++ wrapper for extra thread created by Python IVR script
 *
 * The thread runs a Python callable until it returns. It is handed to the
 * AmThreadWatcher, which on shutdown stop()s it and waits a bounded time for
 * run() to return before the module is unloaded. on_stop() therefore has to
 * be able to end a callable that would otherwise run forever: it raises
 * SystemExit in the script's Python thread, which the interpreter delivers
 * at the next bytecode the script executes. A script blocked inside a single
 * C call (e.g. a long time.sleep()) only sees it once that call returns.
 */
class PythonScriptThread : public AmThread {
  PyObject* py_thread_object;

  // all three are only accessed with the GIL held
  bool stop_requested;
  bool in_call;              // run() is inside the Python callable
  unsigned long py_thread_id; // the script's Python thread, while in_call

 protected:
  void run();
  void on_stop();

 public:
  PythonScriptThread(PyObject* py_thread_object_)
    : py_thread_object(py_thread_object_),
      stop_requested(false), in_call(false), py_thread_id(0) { }
};

#endif
