#include "fct.h"

#include "AmArg.h"
#include "jsonArg.h"

#include "../../apps/registrar_client/SIPRegistrarClient.h"

#include "AmEvent.h"
#include "AmThread.h"

#include <string>
#include <thread>
#include <unistd.h>

// SIPRegistrarClient::invoke() is reachable from any control interface
// client (jsonrpc, xmlrpc2di), so it must not trust the types of its
// arguments: AmArg::asCStr() returns the union member without looking at the
// type tag, and an integer 12345 read that way is the pointer 0x3039. A wrong
// type has to come back as AmArg::TypeMismatchException, which the RPC
// servers report as "invalid params".
//
// The arguments are built from JSON with json2arg(), as the jsonrpc server
// does with the "params" of a request. fct_chk_eq_int() evaluates its
// arguments twice, so the calls are checked with fct_chk().

namespace {

enum Outcome {
  Returned,
  TypeMismatch,
  OutOfBounds,
  BadJson
};

Outcome call(const char *method, const char *json_params, AmArg &ret) {
  AmArg params;
  if (!json2arg(json_params, params)) {
    return BadJson;
  }

  try {
    SIPRegistrarClient::instance()->invoke(method, params, ret);
  } catch (const AmArg::TypeMismatchException &) {
    return TypeMismatch;
  } catch (const AmArg::OutOfBoundsException &) {
    return OutOfBounds;
  }
  return Returned;
}

Outcome call(const char *method, const char *json_params) {
  AmArg ret;
  return call(method, json_params, ret);
}


// SIPRegistrarClient is also the plug-in factory, and AmPlugIn::dispose()
// deletes it and then dlclose()s the module. Its thread must be gone by then,
// whether or not there is a registration: an idle client sits in
// waitForEvent() and only a stop request that also wakes it gets it out.
// The client's run() is wrapped to record that it really returned.
AmCondition<bool> client_running(false);
AmCondition<bool> client_run_returned(false);

class ObservedClient : public SIPRegistrarClient {
public:
  ObservedClient() : SIPRegistrarClient("registrar_client") {}
  void run() override {
    client_running.set(true);
    SIPRegistrarClient::run();
    client_run_returned.set(true);
  }
};

// start() returns before the new thread is in run(). Wait for it to get
// there - a delete before that would already have destroyed the
// ObservedClient part, so the thread would start the base class run() - and
// give it time to block in waitForEvent(), where a stop request has to find
// it.
ObservedClient *start_idle_client() {
  client_running.set(false);
  client_run_returned.set(false);
  ObservedClient *c = new ObservedClient();
  c->start();
  client_running.wait_for_to(2000);
  usleep(20000);
  return c;
}

// deletes c on another thread; true if that returned within timeout_ms. On a
// regression the delete may block for good, so it is then left behind
// instead of hanging the suite.
bool delete_within(ObservedClient *c, unsigned int timeout_ms) {
  AmCondition<bool> *done = new AmCondition<bool>(false);
  std::thread t([c, done]() {
    delete c;
    done->set(true);
  });
  if (!done->wait_for_to(timeout_ms)) {
    t.detach();
    return false;
  }
  t.join();
  delete done;
  return true;
}

} // namespace

FCTMF_SUITE_BGN(test_registrar_client) {

  FCT_TEST_BGN(registrar_client_create_rejects_int_argument) {
    // sess_link given as a number
    fct_chk(call("createRegistration", "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\", 12345]") == TypeMismatch);
    // domain given as a number
    fct_chk(call("createRegistration", "[12345, \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\", \"reg_agent\"]") == TypeMismatch);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(registrar_client_create_rejects_null_required_argument) {
    fct_chk(call("createRegistration", "[\"example.com\", null, \"Alice\", \"alice\", "
                                       "\"secret\", \"reg_agent\"]") == TypeMismatch);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(registrar_client_create_rejects_non_string_optional_argument) {
    // proxy, contact and handle in turn
    fct_chk(call("createRegistration", "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\", \"reg_agent\", 12345]") == TypeMismatch);
    fct_chk(call("createRegistration", "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\", \"reg_agent\", \"\", 12345]") == TypeMismatch);
    fct_chk(call("createRegistration", "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\", \"reg_agent\", \"\", \"\", 12345]") == TypeMismatch);
    fct_chk(call("createRegistration", "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\", \"reg_agent\", [\"sip:p\"]]") == TypeMismatch);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(registrar_client_create_accepts_null_optional_arguments) {
    AmArg ret;
    fct_chk(call("createRegistration",
                 "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                 "\"secret\", \"reg_agent\", null, null, \"test-handle\"]",
                 ret) == Returned);
    fct_req(isArgArray(ret) && ret.size() == 1 && isArgCStr(ret.get(0)));
    fct_chk_eq_str(ret.get(0).asCStr(), "test-handle");

    // all strings, without the optional ones: a handle is generated
    AmArg ret2;
    fct_chk(call("createRegistration",
                 "[\"example.com\", \"bob\", \"Bob\", \"bob\", "
                 "\"secret\", \"reg_agent\"]",
                 ret2) == Returned);
    fct_req(isArgArray(ret2) && ret2.size() == 1 && isArgCStr(ret2.get(0)));
    fct_chk(std::string(ret2.get(0).asCStr()).length() > 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(registrar_client_create_rejects_missing_arguments) {
    fct_chk(call("createRegistration", "[\"example.com\", \"alice\", \"Alice\", \"alice\", "
                                       "\"secret\"]") == OutOfBounds);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(registrar_client_remove_and_state_reject_non_string_handle) {
    fct_chk(call("removeRegistration", "[12345]") == TypeMismatch);
    fct_chk(call("removeRegistration", "[null]") == TypeMismatch);
    fct_chk(call("getRegistrationState", "[12345]") == TypeMismatch);
    fct_chk(call("getRegistrationState", "[null]") == TypeMismatch);

    // a string handle still works; this one is not registered
    AmArg ret;
    fct_chk(call("getRegistrationState", "[\"no-such-handle\"]", ret) == Returned);
    fct_req(isArgArray(ret) && ret.size() == 1 && isArgInt(ret.get(0)));
    fct_chk_eq_int(ret.get(0).asInt(), 0);
    fct_chk(call("removeRegistration", "[\"no-such-handle\"]") == Returned);
  }
  FCT_TEST_END();

  // AmPlugIn::dispose() deletes the factory: the thread has to be stopped
  // and joined by the time the destructor returns
  FCT_TEST_BGN(registrar_client_destructor_stops_idle_thread) {
    ObservedClient *c = start_idle_client();
    fct_req(client_running.get());
    fct_chk(!c->is_stopped());
    fct_chk(!client_run_returned.get());

    fct_chk(delete_within(c, 2000));
    // no wait here: run() must have returned before delete did
    fct_chk(client_run_returned.get());
  }
  FCT_TEST_END();

  // AmThread::stop() goes through on_stop(), which has to wake the thread
  FCT_TEST_BGN(registrar_client_on_stop_wakes_idle_thread) {
    ObservedClient *c = start_idle_client();
    c->stop();
    fct_chk(client_run_returned.wait_for_to(2000));
    for (int i = 0; i < 100 && !c->is_stopped(); i++) {
      usleep(10000);
    }
    fct_chk(c->is_stopped());
    fct_chk(delete_within(c, 2000));
  }
  FCT_TEST_END();

  // the ServerShutdown broadcast still ends the thread
  FCT_TEST_BGN(registrar_client_server_shutdown_ends_thread) {
    ObservedClient *c = start_idle_client();
    c->postEvent(new AmSystemEvent(AmSystemEvent::ServerShutdown));
    fct_chk(client_run_returned.wait_for_to(2000));
    fct_chk(delete_within(c, 2000));
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
