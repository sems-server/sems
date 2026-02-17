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

  // --- Additional coverage ---

  FCT_TEST_BGN(lookup_empty_string_returns_false) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[::1]"] = 1;

    unsigned short idx = 55;
    fct_chk(!AmConfig::lookupLocalSIPInterface("", idx));
    fct_chk_eq_int(idx, 55);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_issue63_exact_scenario) {
    // Reproduces the exact bug from issue #63: the map stores the IPv6
    // address with brackets (as SEMS does during startup), but
    // get_local_addr_for_dest returns the address without brackets.
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["127.0.0.1"] = 0;
    AmConfig::LocalSIPIP2If["192.168.110.79"] = 1;
    AmConfig::LocalSIPIP2If["[2002:c0a8:6e4f:5::1]"] = 2;

    unsigned short idx = 0;
    // This is the failing lookup from issue #63: bare IPv6 vs bracketed map entry.
    fct_chk(AmConfig::lookupLocalSIPInterface("2002:c0a8:6e4f:5::1", idx));
    fct_chk_eq_int(idx, 2);

    // IPv4 interfaces still resolve correctly alongside.
    idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("192.168.110.79", idx));
    fct_chk_eq_int(idx, 1);

    idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("127.0.0.1", idx));
    fct_chk_eq_int(idx, 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_multiple_ipv6_interfaces) {
    // Verify correct resolution when multiple IPv6 interfaces are registered.
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[2001:db8::1]"] = 10;
    AmConfig::LocalSIPIP2If["[2001:db8::2]"] = 20;
    AmConfig::LocalSIPIP2If["[fd00::1]"] = 30;

    unsigned short idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("2001:db8::1", idx));
    fct_chk_eq_int(idx, 10);

    idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("2001:db8::2", idx));
    fct_chk_eq_int(idx, 20);

    idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("fd00::1", idx));
    fct_chk_eq_int(idx, 30);

    // Non-existent address among the registered ones.
    idx = 99;
    fct_chk(!AmConfig::lookupLocalSIPInterface("2001:db8::3", idx));
    fct_chk_eq_int(idx, 99);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv6_loopback) {
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[::1]"] = 5;

    unsigned short idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("::1", idx));
    fct_chk_eq_int(idx, 5);

    idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface("[::1]", idx));
    fct_chk_eq_int(idx, 5);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv6_link_local) {
    // Link-local addresses contain '%' scope-id suffix in some contexts.
    // The helper should still match if the map entry matches exactly.
    // Note: string literals are stored in variables to avoid '%' in
    // fct_chk's stringified condition (printf format string).
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    const std::string link_local_bracketed = "[fe80::1%25eth0]";
    const std::string link_local_bare      = "fe80::1%25eth0";
    AmConfig::LocalSIPIP2If[link_local_bracketed] = 14;

    unsigned short idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface(link_local_bracketed, idx));
    fct_chk_eq_int(idx, 14);

    // Without brackets, the ':' triggers the add-brackets path.
    idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface(link_local_bare, idx));
    fct_chk_eq_int(idx, 14);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_empty_brackets_returns_false) {
    // Edge case: "[]" should not match anything and must not modify idx.
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If[""] = 99;

    unsigned short idx = 77;
    fct_chk(!AmConfig::lookupLocalSIPInterface("[]", idx));
    fct_chk_eq_int(idx, 77);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv4_not_confused_with_ipv6) {
    // IPv4 addresses must not trigger the bracket-addition path.
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[10.0.0.1]"] = 50;

    unsigned short idx = 0;
    // "10.0.0.1" has no ':', so it should NOT try "[10.0.0.1]".
    // The only match path is verbatim, which won't find "10.0.0.1".
    fct_chk(!AmConfig::lookupLocalSIPInterface("10.0.0.1", idx));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_guard_restores_state) {
    // Verify the RAII guard correctly restores map state.
    std::map<std::string, unsigned short> before = AmConfig::LocalSIPIP2If;
    {
      LocalSIPIP2IfGuard guard;
      AmConfig::LocalSIPIP2If.clear();
      AmConfig::LocalSIPIP2If["test_sentinel"] = 999;
      fct_chk(AmConfig::LocalSIPIP2If.count("test_sentinel") == 1);
    }
    // After guard destruction, original state must be restored.
    fct_chk(AmConfig::LocalSIPIP2If == before);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(lookup_ipv6_full_form) {
    // Full (non-compressed) IPv6 notation stored bracketed.
    LocalSIPIP2IfGuard guard;
    AmConfig::LocalSIPIP2If.clear();
    AmConfig::LocalSIPIP2If["[2001:0db8:0000:0000:0000:0000:0000:0001]"] = 66;

    unsigned short idx = 0;
    fct_chk(AmConfig::lookupLocalSIPInterface(
        "2001:0db8:0000:0000:0000:0000:0000:0001", idx));
    fct_chk_eq_int(idx, 66);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
