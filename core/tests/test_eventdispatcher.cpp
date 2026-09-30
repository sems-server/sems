#include "fct.h"

#include "log.h"

#include "AmEvent.h"
#include "AmEventDispatcher.h"
#include "AmEventQueue.h"

#include <new>
#include <pthread.h>
#include <unistd.h>

// AmEventDispatcher hands a registered queue's postEvent() the event while
// holding the bucket mutex. postEvent() is a virtual call into the queue
// implementation and it can throw - std::queue::push() allocates, and so does
// every AmEvent subclass a handler builds on the way. With the mutex taken and
// released by hand, such an exception unwinds past the unlock() and the bucket
// stays locked for the rest of the process's life: every session whose local
// tag hashes into it then blocks forever.

namespace {

// a queue whose postEvent() fails the way an allocation in it would
class ThrowingQueue : public AmEventQueueInterface {
public:
  void postEvent(AmEvent *ev) override {
    delete ev;
    throw std::bad_alloc();
  }
};

class SilentQueue : public AmEventQueueInterface {
public:
  unsigned int posted;

  SilentQueue() : posted(0) {}

  void postEvent(AmEvent *ev) override {
    posted++;
    delete ev;
  }
};

struct DelArgs {
  std::string       tag;
  AmSharedVar<bool> done;

  DelArgs(const std::string &t) : tag(t), done(false) {}
};

// takes the same bucket mutex post() was using, from another thread, so a
// leaked lock shows up as a thread that never finishes instead of a hung suite
void *del_event_queue(void *a) {
  DelArgs *args = (DelArgs *)a;
  AmEventDispatcher::instance()->delEventQueue(args->tag);
  args->done.set(true);
  return NULL;
}

bool bucket_is_usable(const std::string &tag) {
  DelArgs    args(tag);
  pthread_t  td;
  if (pthread_create(&td, NULL, del_event_queue, &args) != 0) return false;

  // generous for a loaded CI box; on a regression this runs out
  for (int i = 0; i < 100 && !args.done.get(); i++) {
    usleep(20000);
  }

  bool done = args.done.get();
  if (done) {
    pthread_join(td, NULL);
  } else {
    pthread_detach(td);
  }
  return done;
}

} // namespace

FCTMF_SUITE_BGN(test_eventdispatcher) {

  FCT_TEST_BGN(throwing_post_does_not_leak_the_bucket_lock) {
    const std::string tag = "throwing-post-tag";

    ThrowingQueue q;
    fct_req(AmEventDispatcher::instance()->addEventQueue(tag, &q));

    bool threw = false;
    try {
      AmEventDispatcher::instance()->post(tag, new AmEvent(0));
    } catch (std::bad_alloc &) {
      threw = true;
    }
    fct_chk(threw);

    // the bucket must be free again
    fct_chk(bucket_is_usable(tag));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(post_still_reaches_a_registered_queue) {
    const std::string tag = "regular-post-tag";

    SilentQueue q;
    fct_req(AmEventDispatcher::instance()->addEventQueue(tag, &q));

    fct_chk(AmEventDispatcher::instance()->post(tag, new AmEvent(0)));
    fct_chk_eq_int(q.posted, 1);

    fct_chk(AmEventDispatcher::instance()->delEventQueue(tag) == &q);
    fct_chk(!AmEventDispatcher::instance()->post(tag, new AmEvent(0)));
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
