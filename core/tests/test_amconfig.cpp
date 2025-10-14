#include "fct.h"

#include "AmConfig.h"

#include <map>
#include <string>

// Restores LocalSIPIP2If map after each test manipulation.
struct LocalSIPIP2IfGuard {
  std::map<std::string, unsigned short> backup;

  LocalSIPIP2IfGuard() : backup(AmConfig::LocalSIPIP2If) {}

  ~LocalSIPIP2IfGuard() { AmConfig::LocalSIPIP2If = backup; }
};

FCTMF_SUITE_BGN(test_amconfig) {

  FCT_TEST_BGN(lookup_ipv6_bracketed_entry) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[2001:db8::1]"] = 42;

    unsigned short idx = 0;
    // Lookup should succeed even when test omits IPv6 brackets.
    fct_chk(AmConfig::lookupLocalSIPInterface("2001:db8::1", idx));
    fct_chk_eq_int(idx, 42);

    idx = 0;
    // Bracketed literal resolves without rewriting the stored key.
    fct_chk(AmConfig::lookupLocalSIPInterface("[2001:db8::1]", idx));
    fct_chk_eq_int(idx, 42);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv6_unbracketed_entry) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["2001:db8::5"] = 7;

    unsigned short idx = 0;
    // Helper adds brackets when the map stores plain IPv6 addresses.
    fct_chk(AmConfig::lookupLocalSIPInterface("[2001:db8::5]", idx));
    fct_chk_eq_int(idx, 7);

    idx = 0;
    // Plain literal still hits the same interface index.
    fct_chk(AmConfig::lookupLocalSIPInterface("2001:db8::5", idx));
    fct_chk_eq_int(idx, 7);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv4_passthrough) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["127.0.0.1"] = 3;

    unsigned short idx = 0;
    // IPv4 entries are returned unchanged by the helper.
    fct_chk(AmConfig::lookupLocalSIPInterface("127.0.0.1", idx));
    fct_chk_eq_int(idx, 3);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv4_unknown_address) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();

    unsigned short idx = 88;
    // IPv4 addresses not in the map should fail without touching idx.
    fct_chk(!AmConfig::lookupLocalSIPInterface("192.0.2.10", idx));
    fct_chk_eq_int(idx, 88);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_unknown_address) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[2001:db8::8]"] = 9;

    unsigned short idx = 1234;
    // Unknown addresses leave the output index unchanged and return false.
    fct_chk(!AmConfig::lookupLocalSIPInterface("2001:db8::9", idx));
    fct_chk_eq_int(idx, 1234);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
