#include "fct.h"

#include "log.h"

#include "AmThread.h"

#include <unistd.h>

// AmThreadWatcher reaps the AmThread objects the plug-ins hand to it, and
// both the is_stopped() it asks them about and the destructor it calls are
// virtual, i.e. they land in the module's own code. So it has to be possible
// to end the watcher - and everything still queued in it - before the modules
// are unloaded: run() must honour a stop request.
//
// This suite stops the thread watcher for good, so it is registered next to
// last in sems_tests.cpp.

static volatile bool watched_destroyed = false;

class WatchedThread : public AmThread
{
  AmCondition<bool> stop_flag;

public:
  WatchedThread() : stop_flag(false) {}
  ~WatchedThread() { watched_destroyed = true; }

  void run() { while(!stop_flag.wait_for_to(20)) {} }
  void on_stop() { stop_flag.set(true); }
};

FCTMF_SUITE_BGN(test_threadwatcher) {

  FCT_TEST_BGN(watcher_stops_and_reaps_what_is_queued) {
    WatchedThread* t = new WatchedThread();
    t->start();

    AmThreadWatcher* w = AmThreadWatcher::instance(); // instance() start()s it
    w->add(t);
    fct_chk(!w->is_stopped());

    // the watched thread is still in run(): the watcher has to ask it to
    // stop and wait for it, not put it off to a later pass that shutdown
    // will never reach
    w->request_stop();

    // a stop is honoured at once, well inside the 10s the watcher otherwise
    // lets the threads have. Without a stop flag in run() the watcher never
    // leaves it and the loop below runs out.
    for (int i = 0; i < 200 && !w->is_stopped(); i++) {
      usleep(20000);
    }
    fct_chk(w->is_stopped());
    fct_chk(watched_destroyed);

    // join() only once we know run() returned: on a regression it would
    // block forever and hang the suite instead of failing it
    if (w->is_stopped()) {
      w->stop_and_join();
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(add_after_stop_does_not_queue) {
    // nothing reaps the queue anymore, so a late add() must not take
    // ownership - the object would otherwise be freed after its module is
    // gone, or never at all
    watched_destroyed = false;

    WatchedThread* t = new WatchedThread();
    AmThreadWatcher::instance()->add(t);

    usleep(200000);
    fct_chk(!watched_destroyed);

    delete t;
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
