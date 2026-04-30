#include "fct.h"

#include "AmConfig.h"
#include "AmBasicSipDialog.h"
#include "AmSipMsg.h"
#include "AmSipHeaders.h"
#include "sip/defs.h"

// RAII guard: restores AmConfig identity fields after each test so that test
// order does not matter and no state leaks between suites.
struct UAConfigGuard {
  string saved_sig;
  bool   saved_send;

  UAConfigGuard()
    : saved_sig(AmConfig::Signature),
      saved_send(AmConfig::SendUserAgent)
  {}

  ~UAConfigGuard()
  {
    AmConfig::Signature    = saved_sig;
    AmConfig::SendUserAgent = saved_send;
  }
};

FCTMF_SUITE_BGN(test_ua_header) {

  // -----------------------------------------------------------------
  // Default behaviour: SendUserAgent=false => headers suppressed
  // -----------------------------------------------------------------

  FCT_TEST_BGN(default_no_ua_sent_empty_hdrs) {
    // With factory defaults (SendUserAgent=false, Signature="") and no
    // pre-existing header, applyIdentityHeader must leave hdrs empty.
    UAConfigGuard g;
    AmConfig::SendUserAgent = false;
    AmConfig::Signature     = "";
    string hdrs;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(hdrs.empty());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(default_strips_forwarded_ua) {
    // Even when the upstream UAC sent its own User-Agent, the default must
    // strip it so no identity leaks through the B2BUA.
    UAConfigGuard g;
    AmConfig::SendUserAgent = false;
    AmConfig::Signature     = "";
    string hdrs = "User-Agent: SomePhone/1.0" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT).empty());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(default_strips_forwarded_ua_with_signature_configured) {
    // A configured signature must NOT be injected when SendUserAgent=false,
    // and any forwarded UA must still be stripped.
    UAConfigGuard g;
    AmConfig::SendUserAgent = false;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs = "User-Agent: SomePhone/1.0" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT).empty());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(default_no_injection_without_signature) {
    // SendUserAgent=false and no Signature: nothing is added.
    UAConfigGuard g;
    AmConfig::SendUserAgent = false;
    AmConfig::Signature     = "";
    string hdrs;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT).empty());
  }
  FCT_TEST_END();

  // -----------------------------------------------------------------
  // send_user_agent=yes + signature configured
  // -----------------------------------------------------------------

  FCT_TEST_BGN(send_ua_injects_signature_when_absent) {
    // SendUserAgent=true + Signature set: SEMS must add User-Agent when the
    // header is not already present (covers SBC relay with no upstream UA).
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT) == "SEMS/2.x");
  }
  FCT_TEST_END();

  FCT_TEST_BGN(send_ua_preserves_existing_ua) {
    // When the upstream UAC already provided User-Agent, SEMS must not
    // overwrite it — B2BUA transparency is preserved.
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs = "User-Agent: SomePhone/1.0" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT) == "SomePhone/1.0");
    // Must not add a second User-Agent line.
    string ua_second = getHeader(hdrs, SIP_HDR_USER_AGENT, false);
    fct_chk(ua_second.find("SEMS") == string::npos);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(send_ua_true_no_signature_noop) {
    // SendUserAgent=true but no Signature string: nothing should be added or
    // removed (no-op, not a crash).
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "";
    string hdrs;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(hdrs.empty());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(send_ua_true_no_signature_preserves_forwarded) {
    // SendUserAgent=true, no Signature, upstream UA present: must not strip it.
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "";
    string hdrs = "User-Agent: SomePhone/1.0" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT) == "SomePhone/1.0");
  }
  FCT_TEST_END();

  // -----------------------------------------------------------------
  // Server header (replies) — symmetric behaviour
  // -----------------------------------------------------------------

  FCT_TEST_BGN(default_strips_server_header) {
    UAConfigGuard g;
    AmConfig::SendUserAgent = false;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs = "Server: Kamailio/5.x" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_SERVER);
    fct_chk(getHeader(hdrs, SIP_HDR_SERVER).empty());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(send_ua_injects_server_when_absent) {
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_SERVER);
    fct_chk(getHeader(hdrs, SIP_HDR_SERVER) == "SEMS/2.x");
  }
  FCT_TEST_END();

  FCT_TEST_BGN(send_ua_preserves_existing_server) {
    // A Server header already in the reply (e.g. from the B-leg) must not be
    // overwritten when SendUserAgent=true.
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs = "Server: Kamailio/5.x" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_SERVER);
    fct_chk(getHeader(hdrs, SIP_HDR_SERVER) == "Kamailio/5.x");
  }
  FCT_TEST_END();

  // -----------------------------------------------------------------
  // Regression: VERBATIM-relayed REGISTER/INVITE with no upstream UA
  // This is the core bug from issue #539: auth-retry REGISTER generated by
  // UACAuth sent with SIP_FLAGS_VERBATIM had no User-Agent even when
  // signature was configured, because injection was gated on !VERBATIM.
  // Now injection is gated on SendUserAgent and header absence only.
  // -----------------------------------------------------------------

  FCT_TEST_BGN(regression_539_auth_retry_gets_signature) {
    // Simulate the auth-retry REGISTER: hdrs contains auth credentials but
    // no User-Agent (the upstream phone did not send one).
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "SEMS/2.x";
    // Typical saved headers from UACAuth::onSipReply after adding auth creds.
    string hdrs =
        "Authorization: Digest username=\"alice\",realm=\"example.com\","
        "nonce=\"abc123\",uri=\"sip:example.com\",response=\"deadbeef\"" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT) == "SEMS/2.x");
  }
  FCT_TEST_END();

  FCT_TEST_BGN(regression_539_relay_with_phone_ua_preserved) {
    // The upstream phone sent User-Agent; the B2BUA should forward it as-is
    // and not overwrite with the SEMS signature.
    UAConfigGuard g;
    AmConfig::SendUserAgent = true;
    AmConfig::Signature     = "SEMS/2.x";
    string hdrs = "User-Agent: Grandstream/1.2.3" CRLF;
    AmBasicSipDialog::applyIdentityHeader(hdrs, SIP_HDR_USER_AGENT);
    fct_chk(getHeader(hdrs, SIP_HDR_USER_AGENT) == "Grandstream/1.2.3");
  }
  FCT_TEST_END();

}
FCTMF_SUITE_END();
