#include <Python.h>

#include "fct.h"

#include "log.h"

#include "../PythonScriptThread.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// PythonScriptThread runs a callable an IVR script handed to the module. On
// shutdown the AmThreadWatcher stop()s it and waits a bounded time for run()
// to return before the module is unloaded; a callable that is still running
// after that executes code that is about to be unmapped. So stop() has to be
// able to end a script that would otherwise never return.
//
// The tests run the threads against an embedded interpreter, set up the way
// IvrFactory::init_python_interpreter() does it: initialised once, with the
// GIL released for the script threads to take.

// The test is linked without the core's log.cpp, which would pull in the
// configuration and the plug-in API: log to stderr only.
extern "C" {
int log_level = 2;
int log_stderr = 1;
const char *log_level2str[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
void run_log_hooks(int, pid_t, pthread_t, const char *, const char *, int, char *) {}
}

// The interpreter is never finalized, here as in SEMS itself, so its own
// allocations show up as leaks in sanitizer builds.
extern "C" const char *__lsan_default_suppressions() { return "leak:libpython\n"; }

namespace {

PyObject *script_globals = NULL;

// A SystemExit that reaches PyErr_Print() makes Python exit the process -
// with status 0 for a plain SystemExit, which would pass as success. Any
// exit before the suite is through is a failure.
bool tests_finished = false;

void fail_premature_exit() {
  if (!tests_finished) {
    fprintf(stderr, "ivr_tests: the process exited before the tests were through\n");
    _exit(1);
  }
}

const char *script = "import sys, threading, time\n"
                     "ran = False\n"
                     "mark_ident = 0\n"
                     "spin_ident = 0\n"
                     "def spin():\n"
                     "    global spin_ident\n"
                     "    spin_ident = threading.get_ident()\n"
                     "    while True:\n"
                     "        pass\n"
                     "def nap():\n"
                     "    while True:\n"
                     "        time.sleep(0.01)\n"
                     "def mark():\n"
                     "    global ran, mark_ident\n"
                     "    mark_ident = threading.get_ident()\n"
                     "    ran = True\n"
                     "def leave():\n"
                     "    sys.exit(3)\n"
                     "def boom():\n"
                     "    raise ValueError('raised by the test script')\n";

bool init_python() {
  Py_Initialize();
  script_globals = PyDict_New();
  PyDict_SetItemString(script_globals, "__builtins__", PyEval_GetBuiltins());
  PyObject *res = PyRun_String(script, Py_file_input, script_globals, script_globals);
  bool ok = res != NULL;
  if (!ok) {
    PyErr_Print();
  }
  Py_XDECREF(res);
  PyEval_SaveThread();
  return ok;
}

// a borrowed reference to one of the script's functions
PyObject *script_function(const char *name) {
  PyGILState_STATE gst = PyGILState_Ensure();
  PyObject *f = PyDict_GetItemString(script_globals, name);
  PyGILState_Release(gst);
  return f;
}

bool script_ran() {
  PyGILState_STATE gst = PyGILState_Ensure();
  PyObject *ran = PyDict_GetItemString(script_globals, "ran");
  bool res = ran && PyObject_IsTrue(ran);
  PyGILState_Release(gst);
  return res;
}

// The thread ident a script recorded, read under the GIL - that is the
// synchronisation with the script's thread. It is the thread's pthread_t.
unsigned long script_ident(const char *name) {
  PyGILState_STATE gst = PyGILState_Ensure();
  PyObject *ident = PyDict_GetItemString(script_globals, name);
  unsigned long res = ident ? PyLong_AsUnsignedLong(ident) : 0;
  PyGILState_Release(gst);
  return res;
}

// the same bounded wait AmThreadWatcher gives a thread it asked to stop
bool stops_within(AmThread *t, int ms) {
  for (int i = 0; i < ms / 10 && !t->is_stopped(); i++) {
    usleep(10000);
  }
  return t->is_stopped();
}

// gives the test the stop request the watcher would make, without going
// through AmThread::stop(), which is a no-op for a thread that is not running
class TestScriptThread : public PythonScriptThread {
public:
  TestScriptThread(PyObject *f) : PythonScriptThread(f) {}
  void request_stop() { on_stop(); }
};

} // namespace

FCT_BGN() {
  atexit(fail_premature_exit);

  if (!init_python()) {
    fprintf(stderr, "could not set up the test script\n");
    return 1;
  }

  FCT_SUITE_BGN(ivr_script_thread) {

    // a script that never returns and never sleeps
    FCT_TEST_BGN(stop_ends_a_busy_script) {
      PythonScriptThread *t = new PythonScriptThread(script_function("spin"));
      t->start();
      usleep(100000);
      fct_req(!t->is_stopped());

      t->stop();
      fct_chk(stops_within(t, 2000));
      if (t->is_stopped()) {
        delete t;
      }
    }
    FCT_TEST_END();

    // the usual shape of a background script: a loop around short sleeps
    FCT_TEST_BGN(stop_ends_a_sleeping_loop) {
      PythonScriptThread *t = new PythonScriptThread(script_function("nap"));
      t->start();
      usleep(100000);
      fct_req(!t->is_stopped());

      t->stop();
      fct_chk(stops_within(t, 2000));
      if (t->is_stopped()) {
        delete t;
      }
    }
    FCT_TEST_END();

    // a stop that comes before run() got to the script: it is not started
    FCT_TEST_BGN(stop_before_run_skips_the_script) {
      TestScriptThread *t = new TestScriptThread(script_function("mark"));
      t->request_stop();
      t->start();
      fct_chk(stops_within(t, 2000));
      fct_chk(!script_ran());
      if (t->is_stopped()) {
        delete t;
      }
    }
    FCT_TEST_END();

    // A stop that comes after the script returned must not raise anything
    // anywhere: the thread id the script ran under may already belong to
    // another script thread. The finished thread is joined here, which
    // releases its id, so that the next thread started gets it.
    FCT_TEST_BGN(late_stop_leaves_other_scripts_alone) {
      TestScriptThread *done = new TestScriptThread(script_function("mark"));
      done->start();
      fct_req(stops_within(done, 2000));
      fct_chk(script_ran());
      unsigned long done_id = script_ident("mark_ident");
      fct_req(done_id != 0);
      done->join();
      usleep(50000);

      PythonScriptThread *other = new PythonScriptThread(script_function("spin"));
      other->start();
      usleep(100000);
      fct_req(!other->is_stopped());
      // the case this is about; with glibc the id is handed out again
      fct_xchk(script_ident("spin_ident") == done_id, "the thread id was not reused, the test proves little");

      done->request_stop();
      usleep(200000);
      fct_chk(!other->is_stopped());
      delete done;

      other->stop();
      fct_chk(stops_within(other, 2000));
      if (other->is_stopped()) {
        delete other;
      }
    }
    FCT_TEST_END();

    // A script may end its own thread with sys.exit(). That is a SystemExit
    // as well, and printing it with PyErr_Print() would exit the process.
    FCT_TEST_BGN(sys_exit_in_a_script_ends_only_its_thread) {
      PythonScriptThread *t = new PythonScriptThread(script_function("leave"));
      t->start();
      fct_chk(stops_within(t, 2000));
      if (t->is_stopped()) {
        delete t;
      }
    }
    FCT_TEST_END();

    // an exception ends the thread and is reported, not left pending
    FCT_TEST_BGN(exception_in_a_script_ends_its_thread) {
      PythonScriptThread *t = new PythonScriptThread(script_function("boom"));
      t->start();
      fct_chk(stops_within(t, 2000));
      if (t->is_stopped()) {
        delete t;
      }
    }
    FCT_TEST_END();
  }
  FCT_SUITE_END();

  tests_finished = true;
}
FCT_END();
