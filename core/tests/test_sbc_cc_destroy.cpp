#include "fct.h"

#include "log.h"

#include "AmApi.h"
#include "AmArg.h"
#include "AmSession.h"

#include "../../apps/sbc/ExtendedCCInterface.h"
#include "../../apps/sbc/SBCCallLeg.h"

#include <string>
#include <vector>

// SBCCallLeg::onBeforeDestroy() hands every extended call control module its
// onDestroyLeg(). It runs on the session's own thread, from
// AmSession::finalize() and from the error path of AmSession::startup(), and
// nothing above them catches: neither AmSession::run() nor
// AmSessionProcessorThread::run() nor AmThread::_start(). An exception a
// module lets out there leaves the thread function and std::terminate()
// takes the whole process down. It also skips destroy() and
// session_stopped(), and the modules after the failing one never get to
// clean up after the leg.
//
// The legs below run the real startup()/finalize() sequences with two call
// control modules: the first one throws from onDestroyLeg(), the second one
// records that it was called. The last test runs AmSession::run() on the
// leg's own thread, as SEMS does without SESSION_THREADPOOL: without the
// guard it aborts the test binary. The ERRORs logged for the throwing module
// and for the failing onStart() are expected here.

namespace {

class TestCC : public AmObject, public ExtendedCCInterface {
public:
  bool throws;
  int destroyed;

  explicit TestCC(bool throws) : throws(throws), destroyed(0) {}

  void onDestroyLeg(SBCCallLeg * /* call */) override {
    destroyed++;
    if (throws) {
      // like AmSession::Exception, not a std::exception
      throw AmArg::TypeMismatchException();
    }
  }
};

class TestCCModule : public AmDynInvoke {
public:
  TestCC *cc;

  explicit TestCCModule(TestCC *cc) : cc(cc) {}

  void invoke(const std::string &method, const AmArg & /* args */, AmArg &ret) override {
    if (method == "getExtendedInterfaceHandler") {
      ret.push((AmObject *)cc);
      return;
    }
    throw AmDynInvoke::NotImplemented(method);
  }
};

class TestLeg : public SBCCallLeg {
public:
  bool fail_start;
  int destroyed;

  explicit TestLeg(bool fail_start) : SBCCallLeg(SBCCallProfile()), fail_start(fail_start), destroyed(0) {}

  void onStart() override {
    if (fail_start) {
      throw AmSession::Exception(500, "test start failure");
    }
  }

  // the session is not registered with the session container
  void destroy() override { destroyed++; }

  bool runStartup() { return startup(); }
  void runFinalize() { finalize(); }
};

struct Setup {
  TestCC throwing, recording;
  TestCCModule throwing_mod, recording_mod;
  TestLeg *leg;

  explicit Setup(bool fail_start)
      : throwing(true), recording(false), throwing_mod(&throwing), recording_mod(&recording),
        leg(new TestLeg(fail_start)) {
    CCInterfaceListT cc_list;
    cc_list.push_back(CCInterface("throwing"));
    cc_list.push_back(CCInterface("recording"));
    std::vector<AmDynInvoke *> cc_di;
    cc_di.push_back(&throwing_mod);
    cc_di.push_back(&recording_mod);
    leg->initCCExtModules(cc_list, cc_di);
  }

  ~Setup() { delete leg; }
};

} // namespace

FCTMF_SUITE_BGN(test_sbc_cc_destroy) {

  // AmSession::startup(): onStart() fails, onBeforeDestroy() runs inside the
  // catch handler
  FCT_TEST_BGN(sbc_cc_destroy_leg_throw_contained_on_startup_failure) {
    Setup s(true);
    bool escaped = false;
    bool started = false;
    try {
      started = s.leg->runStartup();
    } catch (...) {
      escaped = true;
    }
    fct_xchk(!escaped, "exception from onDestroyLeg() escaped AmSession::startup()");
    fct_chk(!started);
    fct_chk_eq_int(s.throwing.destroyed, 1);
    fct_xchk(s.recording.destroyed == 1, "module after the throwing one got onDestroyLeg() %d times",
             s.recording.destroyed);
    fct_xchk(s.leg->destroyed == 1, "destroy() called %d times", s.leg->destroyed);
  }
  FCT_TEST_END();

  // AmSession::finalize(), the regular end of a session
  FCT_TEST_BGN(sbc_cc_destroy_leg_throw_contained_on_finalize) {
    Setup s(false);
    fct_req(s.leg->runStartup());
    bool escaped = false;
    try {
      s.leg->runFinalize();
    } catch (...) {
      escaped = true;
    }
    fct_xchk(!escaped, "exception from onDestroyLeg() escaped AmSession::finalize()");
    fct_chk_eq_int(s.throwing.destroyed, 1);
    fct_xchk(s.recording.destroyed == 1, "module after the throwing one got onDestroyLeg() %d times",
             s.recording.destroyed);
    fct_xchk(s.leg->destroyed == 1, "destroy() called %d times", s.leg->destroyed);
  }
  FCT_TEST_END();

#ifndef SESSION_THREADPOOL
  // AmSession::run() -> startup() on the session thread: nothing above it
  // catches, so without the guard this ends in std::terminate()
  FCT_TEST_BGN(sbc_cc_destroy_leg_throw_does_not_abort_session_thread) {
    Setup s(true);
    s.leg->start();
    s.leg->join();
    fct_chk_eq_int(s.recording.destroyed, 1);
    fct_chk_eq_int(s.leg->destroyed, 1);
  }
  FCT_TEST_END();
#endif
}
FCTMF_SUITE_END();
