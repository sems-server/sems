#include "fct.h"

#include "log.h"

#include "sip/resolver.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

// The DNS cache maintenance thread is started by the _resolver constructor and
// runs for the whole life of the process. It walks the cache and reads the
// wheel timer's clock, so it has to be possible to end it before either of
// those is torn down: run() must honour a stop request.
//
// This suite stops the resolver for good, so it is registered last in
// sems_tests.cpp.

FCTMF_SUITE_BGN(test_resolver) {

  // A host taken verbatim out of a SIP message carries an IPv6 address in the
  // reference form '[addr]'. str2ip() has to accept that form, or every caller
  // handing it such a host (trans_layer uses the Via 'sent-by' host when
  // force_via_address is set on the interface) treats a perfectly valid
  // address as unparseable.
  FCT_TEST_BGN(str2ip_accepts_ipv6_reference) {
    _resolver *r = resolver::instance();
    const address_type both = (address_type)(IPv4 | IPv6);

    sockaddr_storage sa;
    sockaddr_in6 *sin6 = (sockaddr_in6 *)&sa;
    in6_addr expected;
    fct_chk(inet_pton(AF_INET6, "2001:db8::1", &expected) == 1);

    memset(&sa, 0, sizeof(sa));
    fct_chk(r->str2ip("2001:db8::1", &sa, both) == 1);
    fct_chk(sa.ss_family == AF_INET6);
    fct_chk(memcmp(&sin6->sin6_addr, &expected, sizeof(expected)) == 0);

    memset(&sa, 0, sizeof(sa));
    fct_chk(r->str2ip("[2001:db8::1]", &sa, both) == 1);
    fct_chk(sa.ss_family == AF_INET6);
    fct_chk(memcmp(&sin6->sin6_addr, &expected, sizeof(expected)) == 0);

    // IPv4 and plain host names must keep working, and a bracket pair is not
    // an address of its own
    memset(&sa, 0, sizeof(sa));
    fct_chk(r->str2ip("127.0.0.1", &sa, both) == 1);
    fct_chk(sa.ss_family == AF_INET);

    memset(&sa, 0, sizeof(sa));
    fct_chk(r->str2ip("[]", &sa, both) == 0);

    memset(&sa, 0, sizeof(sa));
    fct_chk(r->str2ip("example.com", &sa, both) == 0);
  }
  FCT_TEST_END();

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
