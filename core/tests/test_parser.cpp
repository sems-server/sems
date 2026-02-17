#include "fct.h"

#include "log.h"

#include "sip/sip_parser.h"
#include "sip/parse_common.h"

#include <string.h>
#include <string>
using std::string;

// Minimal valid SIP request prefix (all mandatory headers)
// Via, To, From, Call-ID, CSeq
#define SIP_REQ_PREFIX \
    "INVITE sip:bob@example.com SIP/2.0\r\n" \
    "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n" \
    "To: <sip:bob@example.com>\r\n" \
    "From: <sip:alice@example.com>;tag=1234\r\n" \
    "Call-ID: abc123@192.0.2.1\r\n" \
    "CSeq: 1 INVITE\r\n"

static int try_parse(const char* raw, int len, sip_msg& msg, char*& err_msg)
{
    msg.copy_msg_buf(raw, len);
    return parse_sip_msg(&msg, err_msg);
}

FCTMF_SUITE_BGN(test_parser) {

    FCT_TEST_BGN(content_length_trims_body) {
        // Body has 10 bytes but Content-Length says 5 => body trimmed to 5
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: 5\r\n"
            "\r\n"
            "0123456789";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.body.len == 5);
        fct_chk(memcmp(msg.body.s, "01234", 5) == 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_zero) {
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: 0\r\n"
            "\r\n"
            "spurious";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.body.len == 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_exact_match) {
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: 4\r\n"
            "\r\n"
            "test";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.body.len == 4);
        fct_chk(memcmp(msg.body.s, "test", 4) == 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_exceeds_body) {
        // Content-Length says 100 but only 4 bytes available
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: 100\r\n"
            "\r\n"
            "test";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == MALFORMED_SIP_MSG);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_non_numeric) {
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: abc\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == MALFORMED_SIP_MSG);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_overflow) {
        // Value exceeding 65535
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: 99999\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == MALFORMED_SIP_MSG);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_leading_whitespace) {
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length:  \t 4\r\n"
            "\r\n"
            "test";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.body.len == 4);
    } FCT_TEST_END();

    FCT_TEST_BGN(content_length_trailing_whitespace) {
        const char* raw =
            SIP_REQ_PREFIX
            "Content-Length: 4  \r\n"
            "\r\n"
            "test";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.body.len == 4);
    } FCT_TEST_END();

    FCT_TEST_BGN(no_content_length_uses_all) {
        // No Content-Length header => body is all remaining bytes
        const char* raw =
            SIP_REQ_PREFIX
            "\r\n"
            "all of this is body";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.body.len == 19);
    } FCT_TEST_END();

} FCTMF_SUITE_END();
