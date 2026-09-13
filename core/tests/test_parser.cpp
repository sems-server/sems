#include "fct.h"

#include "log.h"

#include "sip/sip_parser.h"
#include "sip/parse_100rel.h"
#include "sip/parse_common.h"
#include "sip/parse_cseq.h"

#include <string.h>
#include <string>
using std::string;

// Minimal valid SIP request prefix (all mandatory headers)
// Via, To, From, Call-ID, CSeq
#define SIP_REQ_PREFIX                                                                                       \
  "INVITE sip:bob@example.com SIP/2.0\r\n"                                                                   \
  "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"                                                         \
  "To: <sip:bob@example.com>\r\n"                                                                            \
  "From: <sip:alice@example.com>;tag=1234\r\n"                                                               \
  "Call-ID: abc123@192.0.2.1\r\n"                                                                            \
  "CSeq: 1 INVITE\r\n"

static int try_parse(const char *raw, int len, sip_msg &msg, char *&err_msg) {
  msg.copy_msg_buf(raw, len);
  return parse_sip_msg(&msg, err_msg);
}

FCTMF_SUITE_BGN(test_parser) {

  FCT_TEST_BGN(content_length_trims_body) {
    // Body has 10 bytes but Content-Length says 5 => body trimmed to 5
    const char *raw = SIP_REQ_PREFIX "Content-Length: 5\r\n"
                                     "\r\n"
                                     "0123456789";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
    fct_chk(msg.body.len == 5);
    fct_chk(memcmp(msg.body.s, "01234", 5) == 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_zero) {
    const char *raw = SIP_REQ_PREFIX "Content-Length: 0\r\n"
                                     "\r\n"
                                     "spurious";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
    fct_chk(msg.body.len == 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_exact_match) {
    const char *raw = SIP_REQ_PREFIX "Content-Length: 4\r\n"
                                     "\r\n"
                                     "test";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
    fct_chk(msg.body.len == 4);
    fct_chk(memcmp(msg.body.s, "test", 4) == 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_exceeds_body) {
    // Content-Length says 100 but only 4 bytes available
    const char *raw = SIP_REQ_PREFIX "Content-Length: 100\r\n"
                                     "\r\n"
                                     "test";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == MALFORMED_SIP_MSG);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_non_numeric) {
    const char *raw = SIP_REQ_PREFIX "Content-Length: abc\r\n"
                                     "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == MALFORMED_SIP_MSG);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_overflow) {
    // Value exceeding 65535
    const char *raw = SIP_REQ_PREFIX "Content-Length: 99999\r\n"
                                     "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == MALFORMED_SIP_MSG);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_leading_whitespace) {
    const char *raw = SIP_REQ_PREFIX "Content-Length:  \t 4\r\n"
                                     "\r\n"
                                     "test";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
    fct_chk(msg.body.len == 4);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(content_length_trailing_whitespace) {
    const char *raw = SIP_REQ_PREFIX "Content-Length: 4  \r\n"
                                     "\r\n"
                                     "test";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
    fct_chk(msg.body.len == 4);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(no_content_length_uses_all) {
    // No Content-Length header => body is all remaining bytes
    const char *raw = SIP_REQ_PREFIX "\r\n"
                                     "all of this is body";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
    fct_chk(msg.body.len == 19);
  }
  FCT_TEST_END();

  // --- CSeq method validation (RFC 3261 Section 8.1.1.5) ---

  FCT_TEST_BGN(cseq_method_match) {
    // INVITE request with CSeq INVITE => OK
    const char *raw = SIP_REQ_PREFIX "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(cseq_method_mismatch_known) {
    // INVITE request but CSeq says BYE
    const char *raw = "INVITE sip:bob@example.com SIP/2.0\r\n"
                      "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
                      "To: <sip:bob@example.com>\r\n"
                      "From: <sip:alice@example.com>;tag=1234\r\n"
                      "Call-ID: abc123@192.0.2.1\r\n"
                      "CSeq: 1 BYE\r\n"
                      "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == MALFORMED_SIP_MSG);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(cseq_method_mismatch_other) {
    // FOOBAR request but CSeq says BAZQUX (both OTHER_METHOD)
    const char *raw = "FOOBAR sip:bob@example.com SIP/2.0\r\n"
                      "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
                      "To: <sip:bob@example.com>\r\n"
                      "From: <sip:alice@example.com>;tag=1234\r\n"
                      "Call-ID: abc123@192.0.2.1\r\n"
                      "CSeq: 1 BAZQUX\r\n"
                      "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == MALFORMED_SIP_MSG);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(cseq_method_match_other) {
    // FOOBAR request with CSeq FOOBAR => OK
    const char *raw = "FOOBAR sip:bob@example.com SIP/2.0\r\n"
                      "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
                      "To: <sip:bob@example.com>\r\n"
                      "From: <sip:alice@example.com>;tag=1234\r\n"
                      "Call-ID: abc123@192.0.2.1\r\n"
                      "CSeq: 1 FOOBAR\r\n"
                      "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_chk(rc == 0);
  }
  FCT_TEST_END();

  // The CSeq number has to fit in 32 bits (RFC 3261 section 8.1.1.5); a
  // larger one must not wrap into a different, valid looking number.

  FCT_TEST_BGN(cseq_max_number_with_leading_zeros) {
    // the limit is on the value, not on the number of digits
    const char *raw = "INVITE sip:bob@example.com SIP/2.0\r\n"
                      "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
                      "To: <sip:bob@example.com>\r\n"
                      "From: <sip:alice@example.com>;tag=1234\r\n"
                      "Call-ID: abc123@192.0.2.1\r\n"
                      "CSeq: 0004294967295 INVITE\r\n"
                      "\r\n";
    sip_msg msg;
    char *err_msg = NULL;
    int rc = try_parse(raw, strlen(raw), msg, err_msg);
    fct_req(rc == 0);
    fct_chk(get_cseq(&msg)->num == 4294967295U);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(cseq_overflow) {
    // 4294967296 used to wrap to 0, 4294967297 to 1, and the RFC 4475
    // section 3.1.2.4 value 2**65 to 0
    const char *numbers[] = {"4294967296", "4294967297", "99999999999999", "36893488147419103232"};
    for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
      string raw = string("INVITE sip:bob@example.com SIP/2.0\r\n"
                          "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
                          "To: <sip:bob@example.com>\r\n"
                          "From: <sip:alice@example.com>;tag=1234\r\n"
                          "Call-ID: abc123@192.0.2.1\r\n"
                          "CSeq: ") +
                   numbers[i] + " INVITE\r\n\r\n";
      sip_msg msg;
      char *err_msg = NULL;
      int rc = try_parse(raw.c_str(), raw.length(), msg, err_msg);
      fct_xchk(rc == MALFORMED_SIP_MSG && err_msg && !strcmp(err_msg, "could not parse CSeq hf"),
               "CSeq %s: rc=%d err='%s'", numbers[i], rc, err_msg ? err_msg : "");
    }
  }
  FCT_TEST_END();

  // RSeq and the two RAck numbers (RFC 3262) are read the same way.

  FCT_TEST_BGN(rseq_max_number) {
    unsigned rseq = 0;
    fct_chk(parse_rseq(&rseq, "4294967295", 10));
    fct_chk(rseq == 4294967295U);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(rseq_overflow) {
    const char *numbers[] = {"4294967296", "4294967300", "42949672950", "99999999999"};
    for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
      unsigned rseq = 12345;
      bool parsed = parse_rseq(&rseq, numbers[i], strlen(numbers[i]));
      fct_xchk(!parsed && rseq == 12345, "RSeq %s: parsed=%d rseq=%u", numbers[i], (int)parsed, rseq);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(rack_max_numbers) {
    const char *value = "4294967295 4294967295 INVITE";
    sip_rack rack;
    fct_req(parse_rack(&rack, value, strlen(value)));
    fct_chk(rack.rseq == 4294967295U);
    fct_chk(rack.cseq == 4294967295U);
    fct_chk_eq_int(rack.method, sip_request::INVITE);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(rack_overflow) {
    const char *values[] = {"4294967296 1 INVITE", "1 4294967296 INVITE"};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
      sip_rack rack;
      fct_xchk(!parse_rack(&rack, values[i], strlen(values[i])), "RAck '%s' parsed", values[i]);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(prack_rack_overflow) {
    const char *prefix = "PRACK sip:bob@example.com SIP/2.0\r\n"
                         "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK777\r\n"
                         "To: <sip:bob@example.com>;tag=5678\r\n"
                         "From: <sip:alice@example.com>;tag=1234\r\n"
                         "Call-ID: abc123@192.0.2.1\r\n"
                         "CSeq: 2 PRACK\r\n";

    string ok = string(prefix) + "RAck: 1 4294967295 INVITE\r\n\r\n";
    sip_msg ok_msg;
    char *err_msg = NULL;
    fct_chk(try_parse(ok.c_str(), ok.length(), ok_msg, err_msg) == 0);

    string overlarge = string(prefix) + "RAck: 1 4294967296 INVITE\r\n\r\n";
    sip_msg msg;
    err_msg = NULL;
    int rc = try_parse(overlarge.c_str(), overlarge.length(), msg, err_msg);
    fct_chk(rc == MALFORMED_SIP_MSG);
    fct_chk(err_msg && !strcmp(err_msg, "could not parse RAck hf"));
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
