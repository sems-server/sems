#include "fct.h"

#include "log.h"

#include "sip/sip_parser.h"
#include "sip/parse_common.h"
#include "sip/parse_header.h"
#include "sip/parse_via.h"
#include "sip/parse_cseq.h"
#include "sip/parse_from_to.h"

#include <string.h>
#include <string>
using std::string;

/*
 * RFC 3261 SIP Parser MUST-requirement compliance tests.
 *
 * These tests are derived directly from the RFC text, not from
 * reading the parser implementation. They exercise parse_sip_msg()
 * and verify the parsed sip_msg output fields.
 */

static int try_parse(const char* raw, int len, sip_msg& msg, char*& err_msg)
{
    msg.copy_msg_buf(raw, len);
    return parse_sip_msg(&msg, err_msg);
}

// Helper: build a full SIP request with custom components
static string make_request(const char* method, const char* ruri,
                           const char* extra_hdrs, const char* body = NULL)
{
    string msg;
    msg += string(method) + " " + ruri + " SIP/2.0\r\n";
    msg += "Via: SIP/2.0/UDP 192.0.2.1:5060;branch=z9hG4bK776asdhds\r\n";
    msg += "To: Bob <sip:bob@biloxi.com>\r\n";
    msg += "From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n";
    msg += "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n";
    msg += string("CSeq: 314159 ") + method + "\r\n";
    if(extra_hdrs && *extra_hdrs)
        msg += extra_hdrs;
    msg += "\r\n";
    if(body)
        msg += body;
    return msg;
}

// Helper: build a full SIP response
static string make_response(int code, const char* reason,
                            const char* extra_hdrs = NULL)
{
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "SIP/2.0 %d %s\r\n", code, reason);
    string msg(status_line);
    msg += "Via: SIP/2.0/UDP 192.0.2.1:5060;branch=z9hG4bK776asdhds\r\n";
    msg += "To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n";
    msg += "From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n";
    msg += "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n";
    msg += "CSeq: 314159 INVITE\r\n";
    if(extra_hdrs && *extra_hdrs)
        msg += extra_hdrs;
    msg += "\r\n";
    return msg;
}

FCTMF_SUITE_BGN(test_rfc3261_parser) {

    // =====================================================================
    // Section 7.1: Request-Line parsing
    // "The Request-Line contains a method name, a Request-URI, and the
    //  protocol version separated by a single SP character."
    // =====================================================================

    FCT_TEST_BGN(rfc3261_7_1_request_line_parsed) {
        // A well-formed request-line MUST be parsed into method + R-URI
        string raw = make_request("INVITE", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.type == SIP_REQUEST);
        fct_chk(msg.u.request->method == sip_request::INVITE);
        fct_chk_eq_int(msg.u.request->ruri_str.len, 18);
        fct_chk(memcmp(msg.u.request->ruri_str.s, "sip:bob@biloxi.com", 18) == 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.2: Status-Line parsing
    // "The Status-Line consists of the protocol version followed by a
    //  numeric Status-Code and its associated textual phrase."
    // =====================================================================

    FCT_TEST_BGN(rfc3261_7_2_status_line_parsed) {
        // Status-Line MUST be parsed into code + reason
        string raw = make_response(200, "OK");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.type == SIP_REPLY);
        fct_chk_eq_int(msg.u.reply->code, 200);
        fct_chk(msg.u.reply->reason.len == 2);
        fct_chk(memcmp(msg.u.reply->reason.s, "OK", 2) == 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 8.1.1: Mandatory request header fields
    // "A valid SIP request [...] MUST contain To, From, CSeq, Call-ID,
    //  Max-Forwards, and Via"
    // (Max-Forwards is not enforced by the parser but checked at upper layer)
    // =====================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_missing_via_rejected) {
        // Missing Via MUST cause parse failure
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_missing_from_rejected) {
        // Missing From MUST cause parse failure
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_missing_to_rejected) {
        // Missing To MUST cause parse failure
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_missing_callid_rejected) {
        // Missing Call-ID MUST cause parse failure
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_missing_cseq_rejected) {
        // Missing CSeq MUST cause parse failure
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 8.1.1.3: From header MUST contain a tag
    // "The From header field MUST contain a new 'tag' parameter"
    // =====================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_3_from_tag_required) {
        // From without tag MUST be rejected
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_3_from_tag_present_ok) {
        // From with tag MUST parse successfully
        string raw = make_request("OPTIONS", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.from != NULL);
        fct_chk(msg.from->p != NULL);
        sip_from_to* from = dynamic_cast<sip_from_to*>(msg.from->p);
        fct_chk(from != NULL);
        fct_chk(from->tag.len > 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 8.1.1.5: CSeq method MUST match the request method
    // "The method part of CSeq MUST match the method of the request."
    // =====================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_5_cseq_method_must_match) {
        // INVITE request with CSeq method = REGISTER MUST be rejected
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 REGISTER\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 8.2.2.1: SIP version check
    // "If the SIP version [...] is not supported, the server MUST respond
    //  with a 505 (Version Not Supported)"
    // The parser MUST signal version mismatch distinctly from other errors.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_8_2_2_1_bad_version_request) {
        // Request with SIP/3.0 MUST return MALFORMED_SIP_VERSION
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/3.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == MALFORMED_SIP_VERSION);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_2_2_1_bad_version_minor) {
        // Request with SIP/2.1 MUST return MALFORMED_SIP_VERSION
        const char* raw =
            "OPTIONS sip:bob@biloxi.com SIP/2.1\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 OPTIONS\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == MALFORMED_SIP_VERSION);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_2_2_1_good_version_20) {
        // Request with SIP/2.0 MUST parse successfully
        string raw = make_request("INVITE", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.3.1 / 20.35: Via header parsing
    // "The branch parameter [...] MUST be unique across space and time."
    // "Implementations MUST use the 'z9hG4bK' magic cookie."
    // The parser MUST extract the Via branch parameter.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_via_branch_extracted) {
        // The parser MUST extract the branch parameter from the top Via
        string raw = make_request("INVITE", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via1 != NULL);
        fct_chk(msg.via_p1 != NULL);
        fct_chk(msg.via_p1->branch.len > 0);
        // Branch starts with magic cookie
        fct_chk(msg.via_p1->branch.len >= MAGIC_BRANCH_LEN);
        fct_chk(memcmp(msg.via_p1->branch.s, MAGIC_BRANCH_COOKIE,
                        MAGIC_BRANCH_LEN) == 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_via_transport_extracted) {
        // The parser MUST extract the transport from the Via
        string raw = make_request("INVITE", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk_eq_int(msg.via_p1->trans.type, sip_transport::UDP);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_via_host_port_extracted) {
        // The parser MUST extract host and port from the Via sent-by
        string raw = make_request("INVITE", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk(msg.via_p1->host.len > 0);
        fct_chk(memcmp(msg.via_p1->host.s, "192.0.2.1", 9) == 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.3.1: Multiple header fields of same type
    // "Multiple header field rows with the same field name MAY be present."
    // The parser MUST collect all Via headers.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_multiple_vias_collected) {
        // Two Via headers MUST both be collected
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds\r\n"
            "Via: SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int((int)msg.vias.size(), 2);
        // via1 MUST point to the first (topmost) Via
        fct_chk(msg.via1 == *msg.vias.begin());
    } FCT_TEST_END();

    // =====================================================================
    // Section 20.10: Contact header parsing
    // The parser MUST collect Contact headers when present.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_contact_collected) {
        string raw = make_request("INVITE", "sip:bob@biloxi.com",
            "Contact: <sip:alice@pc33.atlanta.com>\r\n");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(!msg.contacts.empty());
    } FCT_TEST_END();

    // =====================================================================
    // Section 20.30 / 20.34: Record-Route and Route headers
    // The parser MUST collect Record-Route and Route headers.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_record_route_collected) {
        string raw = make_request("INVITE", "sip:bob@biloxi.com",
            "Record-Route: <sip:proxy1.atlanta.com;lr>\r\n"
            "Record-Route: <sip:proxy2.biloxi.com;lr>\r\n");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int((int)msg.record_route.size(), 2);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.3.1: Compact header form
    // "Compact form [...] MAY be used [...] The parser MUST accept both."
    // Compact forms: v=Via, t=To, f=From, i=Call-ID, m=Contact, l=Content-Length
    // =====================================================================

    FCT_TEST_BGN(rfc3261_compact_header_forms) {
        // Message using compact header forms MUST parse correctly
        const char* raw =
            "OPTIONS sip:bob@biloxi.com SIP/2.0\r\n"
            "v: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "t: <sip:bob@biloxi.com>\r\n"
            "f: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "i: callid123@host\r\n"
            "CSeq: 1 OPTIONS\r\n"
            "m: <sip:alice@pc33.atlanta.com>\r\n"
            "l: 0\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via1 != NULL);
        fct_chk(msg.to != NULL);
        fct_chk(msg.from != NULL);
        fct_chk(msg.callid != NULL);
        fct_chk(!msg.contacts.empty());
        fct_chk(msg.content_length != NULL);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.1: SIP method recognition
    // The parser MUST correctly identify all standard methods.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_method_recognition) {
        // All six RFC 3261 methods MUST be recognized
        struct { const char* name; int expected; } methods[] = {
            { "INVITE",   sip_request::INVITE },
            { "ACK",      sip_request::ACK },
            { "OPTIONS",  sip_request::OPTIONS },
            { "BYE",      sip_request::BYE },
            { "CANCEL",   sip_request::CANCEL },
            { "REGISTER", sip_request::REGISTER },
        };
        for(int i = 0; i < 6; i++) {
            string raw = make_request(methods[i].name, "sip:bob@biloxi.com", "");
            sip_msg msg;
            char* err_msg = NULL;
            int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
            fct_chk(rc == 0);
            fct_chk_eq_int(msg.u.request->method, methods[i].expected);
        }
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.1: Extension methods
    // "If a [...] method is not recognized, the element [...] MUST NOT
    //  reject the request." - Parser MUST accept unknown methods.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_unknown_method_accepted) {
        // Unknown extension methods MUST be parsed as OTHER_METHOD
        string raw = make_request("SUBSCRIBE", "sip:bob@biloxi.com", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.type == SIP_REQUEST);
        fct_chk_eq_int(msg.u.request->method, sip_request::OTHER_METHOD);
        fct_chk(msg.u.request->method_str.len == 9);
        fct_chk(memcmp(msg.u.request->method_str.s, "SUBSCRIBE", 9) == 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.2: Response status code parsing
    // The parser MUST correctly extract 1xx-6xx status codes.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_response_codes_parsed) {
        // Various response classes MUST be parsed correctly
        int codes[] = { 100, 180, 200, 302, 400, 500, 603 };
        const char* reasons[] = { "Trying", "Ringing", "OK", "Moved",
                                  "Bad Request", "Server Error", "Decline" };
        for(int i = 0; i < 7; i++) {
            string raw = make_response(codes[i], reasons[i]);
            sip_msg msg;
            char* err_msg = NULL;
            int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
            fct_chk(rc == 0);
            fct_chk(msg.type == SIP_REPLY);
            fct_chk_eq_int(msg.u.reply->code, codes[i]);
        }
    } FCT_TEST_END();

    // =====================================================================
    // Section 20.16: Content-Type header
    // "The Content-Type header field [...] MUST be present if the body
    //  is not empty."
    // The parser MUST extract Content-Type when present.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_content_type_extracted) {
        string raw = make_request("INVITE", "sip:bob@biloxi.com",
            "Content-Type: application/sdp\r\n"
            "Content-Length: 3\r\n",
            "v=0");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.content_type != NULL);
        fct_chk(msg.content_type->value.len > 0);
    } FCT_TEST_END();

    // =====================================================================
    // Section 7.5: Framing - CRLF keep-alives
    // "Implementations MUST accept [...] CR-LF-CR-LF" between messages.
    // The parser MUST handle both CRLF and bare LF line endings.
    // =====================================================================

    FCT_TEST_BGN(rfc3261_7_5_bare_lf_line_endings) {
        // Messages with bare LF (no CR) MUST still be parsed
        const char* raw =
            "OPTIONS sip:bob@biloxi.com SIP/2.0\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\n"
            "To: <sip:bob@biloxi.com>\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\n"
            "Call-ID: abc@host\n"
            "CSeq: 1 OPTIONS\n"
            "\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.type == SIP_REQUEST);
    } FCT_TEST_END();

} FCTMF_SUITE_END();
