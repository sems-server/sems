#include "fct.h"

#include "log.h"

#include "AmThread.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

// An AmThread that leaves run() by itself still has to be joined or detached,
// or its stack and descriptor stay allocated until the process exits.
// join() used to skip pthread_join() once the thread was stopped, and nothing
// detached a thread that was deleted without stop(), so every such thread
// leaked its 1 MB stack - e.g. each DSM system thread the thread watcher reaps.
//
// The leak is measured as growth of the address space over many threads; a
// thread that is released has its stack reused by the next one. That needs
// /proc, so on other systems the checks are skipped.

namespace {

class ShortThread : public AmThread {
protected:
  void run() override {}
  void on_stop() override {}
};

// the process' virtual memory size in kB, or -1 if unknown
long vm_size_kb() {
  FILE *f = fopen("/proc/self/status", "r");
  if (!f) {
    return -1;
  }
  char line[256];
  long kb = -1;
  while (fgets(line, sizeof(line), f)) {
    if (!strncmp(line, "VmSize:", 7)) {
      kb = strtol(line + 7, NULL, 10);
      break;
    }
  }
  fclose(f);
  return kb;
}

bool stops_within(AmThread *t, int ms) {
  for (int i = 0; i < ms && !t->is_stopped(); i++) {
    usleep(1000);
  }
  return t->is_stopped();
}

const int THREADS = 100;
// each leaked thread keeps its 1 MB stack mapped
const long MAX_GROWTH_KB = 32 * 1024;

} // namespace

FCTMF_SUITE_BGN(test_amthread) {

  // join() after the thread has already left run() must still reap it
  FCT_TEST_BGN(join_releases_a_finished_thread) {
    long before = vm_size_kb();
    for (int i = 0; i < THREADS; i++) {
      ShortThread *t = new ShortThread();
      t->start();
      fct_req(stops_within(t, 2000));
      t->join();
      delete t;
    }
    long after = vm_size_kb();
    if (before >= 0 && after >= 0) {
      fct_xchk(after - before < MAX_GROWTH_KB, "address space grew by %ld kB over %d joined threads",
               after - before, THREADS);
    }
  }
  FCT_TEST_END();

  // a finished thread that is deleted without join() or stop() - what the
  // thread watcher does - must be released as well
  FCT_TEST_BGN(deleting_a_finished_thread_releases_it) {
    long before = vm_size_kb();
    for (int i = 0; i < THREADS; i++) {
      ShortThread *t = new ShortThread();
      t->start();
      fct_req(stops_within(t, 2000));
      delete t;
    }
    long after = vm_size_kb();
    if (before >= 0 && after >= 0) {
      fct_xchk(after - before < MAX_GROWTH_KB, "address space grew by %ld kB over %d deleted threads",
               after - before, THREADS);
    }
  }
  FCT_TEST_END();

  // join() still waits for a thread that is running, and a second join() or
  // a join() after stop() is a no-op instead of joining a stale handle
  FCT_TEST_BGN(join_waits_once_and_not_after_stop) {
    ShortThread *t = new ShortThread();
    t->start();
    t->join();
    fct_chk(t->is_stopped());
    t->join();
    delete t;

    ShortThread *s = new ShortThread();
    s->start();
    s->stop();
    s->join();
    fct_chk(stops_within(s, 2000));
    delete s;
  }
  FCT_TEST_END();

  // a thread object can be started again once its previous run is over
  FCT_TEST_BGN(restart_after_a_finished_run) {
    long before = vm_size_kb();
    ShortThread *t = new ShortThread();
    for (int i = 0; i < THREADS; i++) {
      t->start();
      fct_req(stops_within(t, 2000));
    }
    t->join();
    delete t;
    long after = vm_size_kb();
    if (before >= 0 && after >= 0) {
      fct_xchk(after - before < MAX_GROWTH_KB, "address space grew by %ld kB over %d restarts",
               after - before, THREADS);
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
