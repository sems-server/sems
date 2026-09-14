#include "fct.h"

#include "log.h"

#include "sip/resolver.h"

#include <unistd.h>

// The DNS cache maintenance thread is started by the _resolver constructor and
// runs for the whole life of the process. It walks the cache and reads the
// wheel timer's clock, so it has to be possible to end it before either of
// those is torn down: run() must honour a stop request.
//
// This suite stops the resolver for good, so it is registered last in
// sems_tests.cpp.

FCTMF_SUITE_BGN(test_resolver) {

  FCT_TEST_BGN(cache_maintenance_thread_honours_stop) {
    _resolver *r = resolver::instance(); // the ctor start()s the thread
    fct_chk(!r->cache_maintenance_stopped());

    r->request_stop();

    // one cache cycle is ~78ms; allow a lot of slack for a loaded CI box.
    // Without a stop flag in run() the thread never leaves it and the loop
    // below runs out.
    for (int i = 0; i < 200 && !r->cache_maintenance_stopped(); i++) {
      usleep(20000);
    }
    fct_chk(r->cache_maintenance_stopped());

    // join() only once we know run() returned: on a regression it would
    // block forever and hang the suite instead of failing it
    if (r->cache_maintenance_stopped()) {
      r->stop_and_join();
      fct_chk(r->cache_maintenance_stopped());
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
