#include "fct.h"

#include "log.h"

#include "AmConfig.h"
#include "AmMediaProcessor.h"
#include "AmThread.h"

#include <unistd.h>

// AmMediaProcessor records, for each session it processes, the callgroup the
// session belongs to and the thread that runs the callgroup. A session whose
// readStreams() or writeStreams() fails is taken out of processing by the
// media thread; that has to go through the processor as well, or the records
// keep the session for good: onMediaProcessingTerminated() makes
// isProcessingMedia() false, and the session's own stopMediaProcessing()
// returns early after that.
//
// The tests run a real media thread. A stand-in session records the callbacks
// the thread makes and fails readStreams()/writeStreams() when told to. Each
// test waits for its session to be inserted before it ends, so the thread is
// in its loop by the time the processor is disposed of.

namespace {

class FakeSession : public AmMediaSession {
public:
  AmCondition<bool> inserted;   // the thread has taken the session
  AmCondition<bool> terminated; // onMediaProcessingTerminated() was called
  AmSharedVar<int> reads, writes, audio_clears, terminations, inserts;
  AmSharedVar<int> read_result, write_result;
  AmSharedVar<unsigned long> thread_id; // the media thread that processes it

  FakeSession()
      : inserted(false), terminated(false), reads(0), writes(0), audio_clears(0), terminations(0),
        inserts(0), read_result(0), write_result(0), thread_id(0) {}

  int readStreams(unsigned long long, unsigned char *) override {
    thread_id.set((unsigned long)pthread_self());
    reads.set(reads.get() + 1);
    return read_result.get();
  }

  int writeStreams(unsigned long long, unsigned char *) override {
    writes.set(writes.get() + 1);
    return write_result.get();
  }

  void processDtmfEvents() override {}

  void clearAudio() override { audio_clears.set(audio_clears.get() + 1); }

  // called by the thread when it inserts the session
  void clearRTPTimeout() override {
    inserts.set(inserts.get() + 1);
    inserted.set(true);
  }

  void onMediaProcessingTerminated() override {
    AmMediaSession::onMediaProcessingTerminated();
    terminations.set(terminations.get() + 1);
    terminated.set(true);
  }
};

// runs the media thread(s) for a test and stops them at the end
struct MediaThread {
  MediaThread(unsigned int threads = 1) {
    AmConfig::MediaProcessorThreads = threads;
    AmMediaProcessor::instance()->init();
  }

  ~MediaThread() {
    AmMediaProcessor::dispose();
    AmConfig::MediaProcessorThreads = 1;
  }
};

AmMediaProcessor *processor() { return AmMediaProcessor::instance(); }

// waits until a media thread has processed s at least once
bool processed(FakeSession &s) {
  for (int i = 0; i < 200 && !s.reads.get(); i++) {
    usleep(10000);
  }
  return s.reads.get() > 0;
}

} // namespace

FCTMF_SUITE_BGN(test_mediaprocessor) {

  // the thread clears a failing session and tells it so, and the processor
  // forgets it, so that a later stopMediaProcessing() has nothing left to do
  FCT_TEST_BGN(failing_session_is_cleared_and_forgotten) {
    for (int dir = 0; dir < 2; dir++) {
      const char *what = dir ? "writeStreams" : "readStreams";
      FakeSession s;
      MediaThread mt;
      processor()->addSession(&s, "call-1");
      fct_req(s.inserted.wait_for_to(2000));
      fct_chk(processor()->hasSession(&s));
      fct_chk(s.isProcessingMedia());

      if (dir) {
        s.write_result.set(-1);
      } else {
        s.read_result.set(-1);
      }
      fct_xchk(s.terminated.wait_for_to(2000), "%s failing: the session was not terminated", what);
      fct_xchk(!processor()->hasSession(&s), "%s failing: the processor still has the session", what);
      fct_chk_eq_int(s.audio_clears.get(), 1);
      fct_chk_eq_int(s.terminations.get(), 1);
      fct_chk(!s.isProcessingMedia());

      // and it is not processed any more
      int reads = s.reads.get();
      int writes = s.writes.get();
      usleep(50000);
      fct_chk_eq_int(s.reads.get(), reads);
      fct_chk_eq_int(s.writes.get(), writes);
    }
  }
  FCT_TEST_END();

  // a session failing in both directions is cleared once, whether or not the
  // thread sees both failures in the same cycle
  FCT_TEST_BGN(session_failing_both_ways_is_cleared_once) {
    FakeSession s;
    MediaThread mt;
    processor()->addSession(&s, "call-2");
    fct_req(s.inserted.wait_for_to(2000));

    s.read_result.set(-1);
    s.write_result.set(-1);
    fct_req(s.terminated.wait_for_to(2000));
    usleep(50000);
    fct_chk(!processor()->hasSession(&s));
    fct_chk_eq_int(s.audio_clears.get(), 1);
    fct_chk_eq_int(s.terminations.get(), 1);
  }
  FCT_TEST_END();

  // removeSession() and clearSession() take a session out of processing and
  // out of the records; only clearSession() clears its audio
  FCT_TEST_BGN(removed_and_cleared_sessions_are_forgotten) {
    for (int clear = 0; clear < 2; clear++) {
      const char *what = clear ? "clearSession" : "removeSession";
      FakeSession s;
      MediaThread mt;
      processor()->addSession(&s, "call-3");
      fct_req(s.inserted.wait_for_to(2000));

      if (clear) {
        processor()->clearSession(&s);
      } else {
        processor()->removeSession(&s);
      }
      fct_xchk(s.terminated.wait_for_to(2000), "%s: the session was not terminated", what);
      fct_xchk(!processor()->hasSession(&s), "%s: the processor still has the session", what);
      fct_chk_eq_int(s.audio_clears.get(), clear);
      fct_chk_eq_int(s.terminations.get(), 1);
    }
  }
  FCT_TEST_END();

  // removing a session the processor does not have, because it was never
  // added or has been taken out already, does nothing to it or to the others
  FCT_TEST_BGN(removing_an_unknown_session_does_nothing) {
    FakeSession s, other;
    MediaThread mt;
    processor()->addSession(&other, "call-4");
    fct_req(other.inserted.wait_for_to(2000));

    processor()->removeSession(&s);
    processor()->clearSession(&s);
    fct_chk(!processor()->hasSession(&s));
    fct_chk(processor()->hasSession(&other));

    // taken out by the thread, then a stopMediaProcessing() that comes late
    other.write_result.set(-1);
    fct_req(other.terminated.wait_for_to(2000));
    processor()->removeSession(&other);
    usleep(50000);
    fct_chk(!processor()->hasSession(&other));
    fct_chk_eq_int(other.terminations.get(), 1);
    fct_chk_eq_int(s.terminations.get(), 0);
    fct_chk_eq_int(s.audio_clears.get(), 0);
  }
  FCT_TEST_END();

  // adding a session the processor already has is ignored: it neither resets
  // the RTP timeout of the running session nor duplicates its bookkeeping, so
  // a single removeSession() still takes it out for good
  FCT_TEST_BGN(adding_a_session_twice_is_ignored) {
    FakeSession s;
    MediaThread mt;
    processor()->addSession(&s, "call-5");
    fct_req(s.inserted.wait_for_to(2000));
    fct_chk_eq_int(s.inserts.get(), 1);

    processor()->addSession(&s, "call-6");
    usleep(50000);
    fct_xchk(s.inserts.get() == 1, "the second add reset the RTP timeout (inserts=%d)",
             s.inserts.get());
    fct_chk_eq_int(s.terminations.get(), 0);
    fct_chk(processor()->hasSession(&s));

    processor()->removeSession(&s);
    fct_req(s.terminated.wait_for_to(2000));
    fct_chk(!processor()->hasSession(&s));
    fct_chk_eq_int(s.terminations.get(), 1);

    // and it is not processed any more
    int reads = s.reads.get();
    usleep(50000);
    fct_chk_eq_int(s.reads.get(), reads);
  }
  FCT_TEST_END();

  // A second add into the same callgroup must not leave a member behind once
  // the session is removed: the callgroup would otherwise stay pinned to its
  // media thread for the life of the process (callgroup2thread is only
  // erased once the callgroup has no member left), and every later session
  // of that callgroup would go there however loaded that thread is. With two
  // threads that shows up as the next session of the callgroup being put on
  // the busy thread instead of the idle one.
  FCT_TEST_BGN(adding_a_session_twice_does_not_pin_its_callgroup) {
    MediaThread mt(2);
    FakeSession a, busy, b;

    processor()->addSession(&a, "call-7");
    fct_req(a.inserted.wait_for_to(2000));
    processor()->addSession(&a, "call-7");
    usleep(20000);

    processor()->removeSession(&a);
    fct_req(a.terminated.wait_for_to(2000));
    fct_chk(!processor()->hasSession(&a));

    // both threads are idle now, so this lands on the first one - the one
    // call-7 was on
    processor()->addSession(&busy, "call-8");
    fct_req(busy.inserted.wait_for_to(2000));
    fct_req(processed(busy));

    // call-7 has no member left, so its next session goes to the idle thread
    processor()->addSession(&b, "call-7");
    fct_req(b.inserted.wait_for_to(2000));
    fct_req(processed(b));
    fct_xchk(b.thread_id.get() != busy.thread_id.get(),
             "call-7 is still pinned to the busy media thread");

    processor()->removeSession(&b);
    processor()->removeSession(&busy);
    fct_chk(b.terminated.wait_for_to(2000));
    fct_chk(busy.terminated.wait_for_to(2000));
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
