#include "fct.h"

#include "AmArg.h"
#include "jsonArg.h"

#include "../../apps/registrar_client/SIPRegistrarClient.h"

#include <string>

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
}
FCTMF_SUITE_END();
