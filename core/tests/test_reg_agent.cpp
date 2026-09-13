#include "fct.h"

#include "log.h"

#include "AmApi.h"
#include "AmArg.h"
#include "AmConfig.h"
#include "AmPlugIn.h"
#include "AmThread.h"
#include "AmUtils.h"

#include "../../apps/reg_agent/RegistrationAgent.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unistd.h>

// The reg_agent dialer thread has to honour a stop request, both while it
// waits (2 s before the first round, 10 s between rounds) and between two
// registrations of a round, and RegistrationAgentFactory's destructor has to
// stop and join it: AmPlugIn::~AmPlugIn() releases the factories and then
// dlclose()s the modules.
//
// The dialer talks to a stub registrar_client DI interface. Each dialer
// thread that calls into the stub leaves a thread-specific value whose
// destructor records that the thread has exited, so a dialer that outlives
// its factory fails an assertion instead of only a sanitizer run.

namespace {

long long now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

RegInfo account(const char *user) {
  RegInfo ri;
  ri.domain = "example.com";
  ri.user = user;
  ri.auth_user = user;
  return ri;
}

class StubRegistrarClient : public AmDynInvokeFactory, public AmDynInvoke {
public:
  // createRegistration calls; only the dialer thread writes it
  AmCondition<int> creations;
  // a dialer thread that called into the stub has exited
  AmCondition<bool> exited;
  // requested to stop from within createRegistration; set before the
  // dialer is started
  RegThread *stop_on_create;

  StubRegistrarClient()
      : AmDynInvokeFactory("registrar_client"), creations(0), exited(false), stop_on_create(NULL) {
    pthread_key_create(&exit_key, &StubRegistrarClient::on_thread_exit);
  }

  void reset(RegThread *stopper) {
    creations.set(0);
    exited.set(false);
    stop_on_create = stopper;
  }

  int onLoad() override { return 0; }

  AmDynInvoke *getInstance() override { return this; }

  void invoke(const std::string &method, const AmArg &, AmArg &ret) override {
    pthread_setspecific(exit_key, this);
    if (method == "createRegistration") {
      int n = creations.get() + 1;
      creations.set(n);
      if (stop_on_create) {
        stop_on_create->request_stop();
      }
      ret.push(("stub-" + int2str(n)).c_str());
    } else if (method == "getRegistrationState") {
      ret.push(1);    // exists
      ret.push(1);    // active
      ret.push(3600); // expires
    }
  }

private:
  pthread_key_t exit_key;

  static void on_thread_exit(void *stub) { static_cast<StubRegistrarClient *>(stub)->exited.set(true); }
};

// registered once and kept for the rest of the run, like a loaded module
StubRegistrarClient *registrar_client_stub() {
  static StubRegistrarClient *stub = NULL;
  if (!stub) {
    stub = new StubRegistrarClient();
    AmPlugIn::registerDIInterface("registrar_client", stub);
  }
  return stub;
}

// points AmConfig::ModConfigPath at a temporary directory holding
// reg_agent.conf
struct TempModConfig {
  std::string saved;
  std::string dir;
  std::string file;
  bool ok;

  explicit TempModConfig(const char *content) : saved(AmConfig::ModConfigPath), ok(false) {
    char tmpl[] = "/tmp/sems_test_reg_agent.XXXXXX";
    if (!mkdtemp(tmpl)) {
      return;
    }
    dir = tmpl;
    file = dir + "/reg_agent.conf";
    FILE *fp = fopen(file.c_str(), "w");
    if (!fp) {
      return;
    }
    bool written = fputs(content, fp) >= 0;
    ok = (fclose(fp) == 0) && written;
    AmConfig::ModConfigPath = dir + "/";
  }

  ~TempModConfig() {
    AmConfig::ModConfigPath = saved;
    if (!file.empty()) {
      unlink(file.c_str());
    }
    if (!dir.empty()) {
      rmdir(dir.c_str());
    }
  }
};

void *request_stop_later(void *dialer) {
  usleep(50000);
  static_cast<RegThread *>(dialer)->request_stop();
  return NULL;
}

} // namespace

FCTMF_SUITE_BGN(test_reg_agent) {

  FCT_TEST_BGN(dialer_stops_during_startup_wait) {
    StubRegistrarClient *stub = registrar_client_stub();
    stub->reset(NULL);
    RegThread dialer;
    dialer.add_reg(account("alice"));

    long long start = now_ms();
    dialer.start();
    // the stop comes from another thread while this one is already in
    // join(), so join() does wait for the thread
    pthread_t stopper;
    bool stopper_started = pthread_create(&stopper, NULL, request_stop_later, &dialer) == 0;
    if (!stopper_started) {
      dialer.request_stop();
    }
    dialer.join();
    if (stopper_started) {
      pthread_join(stopper, NULL);
    }

    fct_chk(stopper_started);
    fct_chk(now_ms() - start < 1000);
    fct_chk(dialer.is_stopped());
    fct_chk_eq_int(stub->creations.get(), 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(dialer_stops_within_a_registration_round) {
    StubRegistrarClient *stub = registrar_client_stub();
    RegThread dialer;
    stub->reset(&dialer);
    dialer.add_reg(account("alice"));
    dialer.add_reg(account("bob"));

    long long start = now_ms();
    dialer.start();
    // the stub requests the stop while creating the first registration
    dialer.join();

    // bob is not registered, and there is no 10 s wait after the round
    fct_chk_eq_int(stub->creations.get(), 1);
    fct_chk(now_ms() - start < 5000);
    fct_chk(stub->exited.get());
    stub->reset(NULL);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(factory_destruction_stops_the_dialer) {
    StubRegistrarClient *stub = registrar_client_stub();
    stub->reset(NULL);
    TempModConfig conf("domain=example.com\nuser=alice\n");
    fct_req(conf.ok);

    RegistrationAgentFactory *factory = new RegistrationAgentFactory("reg_agent");
    inc_ref(factory); // the reference AmPlugIn holds
    int loaded = factory->onLoad();
    fct_chk_eq_int(loaded, 0);

    // after the first round the dialer sits in its 10 s wait
    fct_chk(stub->creations.wait_for_to(5000));
    usleep(100000);

    long long start = now_ms();
    dec_ref(factory); // as AmPlugIn::~AmPlugIn() releases it
    fct_chk(now_ms() - start < 1000);
    fct_chk(stub->exited.wait_for_to(1000));
    fct_chk_eq_int(stub->creations.get(), 1);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(factory_without_accounts_is_destroyed_at_once) {
    TempModConfig conf("display_name=nobody\n");
    fct_req(conf.ok);

    RegistrationAgentFactory *factory = new RegistrationAgentFactory("reg_agent");
    inc_ref(factory);
    // no complete account: the dialer is never started
    int loaded = factory->onLoad();
    fct_chk_eq_int(loaded, 0);

    long long start = now_ms();
    dec_ref(factory);
    fct_chk(now_ms() - start < 1000);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
