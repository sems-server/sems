#include "fct.h"

#include "log.h"

#include "../../apps/db_reg_agent/RegistrationTimer.h"

#include <atomic>
#include <sys/time.h>
#include <thread>
#include <time.h>
#include <unistd.h>

// RegistrationTimer (db_reg_agent) keeps its timers in a circular array of
// TIMER_BUCKETS buckets of TIMER_BUCKET_LENGTH seconds each, so it can only
// place timers less than TIMER_BUCKETS * TIMER_BUCKET_LENGTH (~111 h) ahead.
//
// insert_timer_leastloaded() scans the array from the bucket of from_time to
// the bucket of to_time. A to_time beyond that horizon made the scan look for
// a negative index the circular walk never reaches, so it spun forever while
// holding the bucket mutex. The timers below run each insert on a worker
// thread and give up after a deadline, so the old behaviour fails the test
// instead of hanging the suite; the stuck worker is detached and leaked.

namespace {

const time_t HORIZON = (time_t)TIMER_BUCKETS * TIMER_BUCKET_LENGTH;

time_t wall_now() {
  struct timeval now;
  gettimeofday(&now, 0);
  return now.tv_sec;
}

// A RegistrationTimer whose (private) current_bucket_start is known: it is
// the second the constructor read, so retry until no second boundary was
// crossed while constructing. The timer thread is not started, so
// current_bucket stays 0 and nothing fires behind the test's back.
RegistrationTimer *new_timer(time_t &start) {
  for (;;) {
    time_t before = wall_now();
    RegistrationTimer *t = new RegistrationTimer();
    if (wall_now() == before) {
      start = before;
      return t;
    }
    delete t;
  }
}

struct LeastLoadedCall {
  RegistrationTimer *timer;
  RegTimer *reg;
  time_t from, to;
  std::atomic<bool> done;
};

// Returns whether insert_timer_leastloaded() returned within 2 s. On a
// timeout the worker (and the objects it uses) are leaked: it holds the
// bucket mutex and never returns.
bool leastloaded_returns(RegistrationTimer *timer, RegTimer *reg, time_t from, time_t to) {
  LeastLoadedCall *call = new LeastLoadedCall();
  call->timer = timer;
  call->reg = reg;
  call->from = from;
  call->to = to;
  call->done = false;

  std::thread worker([call]() {
    call->timer->insert_timer_leastloaded(call->reg, call->from, call->to);
    call->done = true;
  });

  for (int i = 0; i < 200 && !call->done; i++) {
    usleep(10000);
  }

  if (!call->done) {
    ERROR("insert_timer_leastloaded(%ld, %ld) did not return - leaking the "
          "spinning thread\n",
          (long)from, (long)to);
    worker.detach();
    return false;
  }

  worker.join();
  delete call;
  return true;
}

} // namespace

FCTMF_SUITE_BGN(test_registration_timer) {

  // the regular case: the whole window is inside the array
  FCT_TEST_BGN(leastloaded_in_range) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer reg;

    fct_req(leastloaded_returns(timer, &reg, start + 100, start + 1000));
    fct_chk(reg.expires >= start + 100);
    fct_chk(reg.expires < start + 1000);
    fct_chk(timer->remove_timer(&reg));
    delete timer;
  }
  FCT_TEST_END();

  // from_time in range, to_time beyond the horizon: the search is limited to
  // the buckets that exist
  FCT_TEST_BGN(leastloaded_to_beyond_horizon_returns) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer reg;

    fct_req(leastloaded_returns(timer, &reg, start + 100, start + HORIZON + 3600));
    fct_chk(reg.expires >= start + 100);
    fct_chk(reg.expires < start + HORIZON);
    fct_chk(timer->remove_timer(&reg));
    delete timer;
  }
  FCT_TEST_END();

  // from_time in the past, to_time beyond the horizon
  FCT_TEST_BGN(leastloaded_from_past_to_beyond_horizon_returns) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer reg;

    fct_req(leastloaded_returns(timer, &reg, start - 100, start + 2 * HORIZON));
    fct_chk(reg.expires >= start);
    fct_chk(reg.expires < start + HORIZON);
    fct_chk(timer->remove_timer(&reg));
    delete timer;
  }
  FCT_TEST_END();

  // to_time so far ahead that its distance does not fit an int: it must not
  // wrap around to some bucket inside the array (or to an arbitrary negative
  // index the scan never reaches)
  FCT_TEST_BGN(leastloaded_to_beyond_int_range_returns) {
    if (sizeof(time_t) > 4) {
      time_t start;
      RegistrationTimer *timer = new_timer(start);
      RegTimer reg;

      // 2^31 + 10 s: truncated to int this is negative
      time_t far = start + (time_t)0x7fffffff + 11;
      fct_req(leastloaded_returns(timer, &reg, start + 100, far));
      fct_chk(reg.expires >= start + 100);
      fct_chk(timer->remove_timer(&reg));

      // 2^32 + 1000 s: truncated to int this is 1000 s
      far = start + (time_t)0xffffffff + 1001;
      fct_req(leastloaded_returns(timer, &reg, start + 100, far));
      fct_chk(reg.expires >= start + 100);
      fct_chk(timer->remove_timer(&reg));
      delete timer;
    }
  }
  FCT_TEST_END();

  // the whole window beyond the horizon: the timer is scheduled in the last
  // bucket instead of being dropped (which would stop the re-registration)
  FCT_TEST_BGN(leastloaded_window_beyond_horizon_uses_last_bucket) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer reg;

    fct_req(leastloaded_returns(timer, &reg, start + HORIZON + 3600, start + HORIZON + 7200));
    fct_chk(reg.expires >= start + HORIZON - TIMER_BUCKET_LENGTH);
    fct_chk(reg.expires < start + HORIZON);
    fct_chk(timer->remove_timer(&reg));
    delete timer;
  }
  FCT_TEST_END();

  // exactly TIMER_BUCKETS buckets ahead is one bucket past the last one; it
  // must not alias the current bucket (which would fire ~111 h early)
  FCT_TEST_BGN(leastloaded_window_at_horizon_edge_uses_last_bucket) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer reg;

    fct_req(leastloaded_returns(timer, &reg, start + HORIZON, start + HORIZON + 5));
    fct_chk(reg.expires >= start + HORIZON - TIMER_BUCKET_LENGTH);
    fct_chk(reg.expires < start + HORIZON);
    fct_chk(timer->remove_timer(&reg));
    delete timer;
  }
  FCT_TEST_END();

  // an inverted window (to_time in the past, from_time not) must not scan for
  // the negative to_index either; the timer goes to the bucket of from_time
  FCT_TEST_BGN(leastloaded_inverted_window_uses_from_bucket) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer reg;

    fct_req(leastloaded_returns(timer, &reg, start + 1000, start - 100));
    fct_chk(reg.expires >= start + 1000);
    fct_chk(reg.expires < start + 1000 + TIMER_BUCKET_LENGTH);
    fct_chk(timer->remove_timer(&reg));
    delete timer;
  }
  FCT_TEST_END();

  // insert_timer() at the edge of the array: the last second still fits, the
  // next one does not (and must not land in the current bucket)
  FCT_TEST_BGN(insert_timer_horizon_edge) {
    time_t start;
    RegistrationTimer *timer = new_timer(start);
    RegTimer last, past_last, far;

    last.expires = start + HORIZON - 1;
    fct_chk(timer->insert_timer(&last));
    fct_chk(timer->remove_timer(&last));

    past_last.expires = start + HORIZON;
    fct_chk(!timer->insert_timer(&past_last));
    fct_chk(!timer->remove_timer(&past_last));

    if (sizeof(time_t) > 4) {
      far.expires = start + (time_t)0xffffffff + 1001;
      fct_chk(!timer->insert_timer(&far));
      fct_chk(!timer->remove_timer(&far));
    }
    delete timer;
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
