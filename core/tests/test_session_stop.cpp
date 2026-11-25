#include "fct.h"

#include "log.h"

#include "AmSession.h"
#include "AmSipDialog.h"
#include "AmThread.h"

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

// AmSession::setStopped() runs the application's onStop() exactly once, on
// the call that moves the session from running to stopped. It is reached from
// other threads than the session's own - the media processor, a B2B peer leg,
// the DI/RPC interfaces and app-side threads all call it - so deciding
// whether this call is the one that stopped the session has to be a single
// atomic step. Reading the flag and then writing it as two separate locked
// operations lets two concurrent callers both see "not stopped yet" and both
// run onStop(), which repeats the application's teardown
// (SBCCallLeg::onStop() calls CCEnd() a second time, so every call control
// module gets a second "end" for the same call).

namespace {

// a session that only counts how often its onStop() ran
class CountingSession : public AmSession {
public:
  std::atomic<int> stops;

  CountingSession() : AmSession(new AmSipDialog()), stops(0) {}

  ~CountingSession() override {
    // nothing was registered with the transaction layer
    dlg->dropTransactions();
  }

  void onStop() override { stops++; }
};

// let every thread spin until all of them are ready, so they really do reach
// the contended call at the same time instead of one after the other
void race(unsigned int threads, const std::function<void()> &body) {
  std::atomic<unsigned int> ready(0);
  std::atomic<bool> go(false);
  std::vector<std::thread> workers;

  for (unsigned int i = 0; i < threads; i++) {
    workers.emplace_back([&]() {
      ready++;
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      body();
    });
  }

  while (ready.load() != threads) {
    std::this_thread::yield();
  }
  go.store(true, std::memory_order_release);

  for (std::vector<std::thread>::iterator it = workers.begin(); it != workers.end(); it++) {
    it->join();
  }
}

const unsigned int RACING_THREADS = 8;
const unsigned int ROUNDS = 200;

} // namespace

FCTMF_SUITE_BGN(test_session_stop) {

  // the primitive the fix rests on: of all the threads that set the
  // condition, exactly one is told that it was still unset before
  FCT_TEST_BGN(test_and_set_reports_one_winner) {
    for (unsigned int round = 0; round < ROUNDS; round++) {
      AmCondition<bool> flag(false);
      std::atomic<int> winners(0);

      race(RACING_THREADS, [&]() {
        if (!flag.test_and_set(true)) {
          winners++;
        }
      });

      fct_xchk(winners.load() == 1, "round %u: %d threads saw the condition unset", round, winners.load());
      fct_xchk(flag.get(), "round %u: the condition was left unset", round);
      if (winners.load() != 1) {
        break;
      }
    }
  }
  FCT_TEST_END();

  // ... and test_and_set() leaves get()/set() alone
  FCT_TEST_BGN(test_and_set_returns_the_previous_value) {
    AmCondition<bool> flag(false);

    fct_chk(!flag.test_and_set(true));
    fct_chk(flag.get());
    fct_chk(flag.test_and_set(true));
    fct_chk(flag.get());
    fct_chk(flag.test_and_set(false));
    fct_chk(!flag.get());
    fct_chk(!flag.test_and_set(false));
    fct_chk(!flag.get());

    flag.set(true);
    fct_chk(flag.test_and_set(false));
    fct_chk(!flag.get());
  }
  FCT_TEST_END();

  // concurrent setStopped() calls on one session run onStop() once
  FCT_TEST_BGN(concurrent_setstopped_calls_stop_the_session_once) {
    for (unsigned int round = 0; round < ROUNDS; round++) {
      CountingSession s;

      // wakeup=false: posting E_TERMINATE_LEG needs the event dispatcher and
      // is not what this test is about
      race(RACING_THREADS, [&]() { s.setStopped(false); });

      fct_xchk(s.stops.load() == 1, "round %u: onStop() ran %d times", round, s.stops.load());
      fct_xchk(s.getStopped(), "round %u: the session was left running", round);
      if (s.stops.load() != 1) {
        break;
      }
    }
  }
  FCT_TEST_END();

  // a later setStopped() on an already stopped session does not stop it again
  FCT_TEST_BGN(repeated_setstopped_calls_stop_the_session_once) {
    CountingSession s;

    for (int i = 0; i < 5; i++) {
      s.setStopped(false);
    }

    fct_chk_eq_int(s.stops.load(), 1);
    fct_chk(s.getStopped());
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
