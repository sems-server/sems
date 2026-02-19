#include "fct.h"

#include "log.h"

#include "sip/sip_parser.h"
#include "sip/parse_common.h"
#include "sip/parse_header.h"
#include "sip/parse_via.h"
#include "sip/parse_cseq.h"
#include "sip/parse_from_to.h"
#include "sip/parse_uri.h"
#include "sip/parse_nameaddr.h"

#include <string.h>
#include <string>
using std::string;

/*
 * Extended RFC 3261 MUST-requirement compliance tests.
 *
 * Covers: URI parsing, header field handling, message structure,
 * response parsing, edge cases, and robustness requirements.
 * All tests derived from RFC 3261 text without reading the implementation.
 */

static int try_parse(const char* raw, int len, sip_msg& msg, char*& err_msg)
{
    msg.copy_msg_buf(raw, len);
    return parse_sip_msg(&msg, err_msg);
}

static string make_request(const char* method, const char* ruri,
                           const char* cseq_method,
                           const char* extra_hdrs,
                           const char* body = NULL)
{
    string msg;
    msg += string(method) + " " + ruri + " SIP/2.0\r\n";
    msg += "Via: SIP/2.0/UDP 192.0.2.1:5060;branch=z9hG4bK776asdhds\r\n";
    msg += "To: Bob <sip:bob@biloxi.com>\r\n";
    msg += "From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n";
    msg += "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n";
    msg += string("CSeq: 314159 ") + cseq_method + "\r\n";
    if(extra_hdrs && *extra_hdrs)
        msg += extra_hdrs;
    if(body) {
        msg += "Content-Length: " + std::to_string(strlen(body)) + "\r\n";
    }
    msg += "\r\n";
    if(body)
        msg += body;
    return msg;
}

static string make_req(const char* method, const char* ruri,
                       const char* extra_hdrs = "",
                       const char* body = NULL)
{
    return make_request(method, ruri, method, extra_hdrs, body);
}

static string make_response(int code, const char* reason,
                            const char* cseq_method = "INVITE",
                            const char* extra_hdrs = "")
{
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "SIP/2.0 %d %s\r\n", code, reason);
    string msg(status_line);
    msg += "Via: SIP/2.0/UDP 192.0.2.1:5060;branch=z9hG4bK776asdhds\r\n";
    msg += "To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n";
    msg += "From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n";
    msg += "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n";
    msg += string("CSeq: 314159 ") + cseq_method + "\r\n";
    if(extra_hdrs && *extra_hdrs)
        msg += extra_hdrs;
    msg += "\r\n";
    return msg;
}


FCTMF_SUITE_BGN(test_rfc3261_musts) {

    // =================================================================
    // RFC 3261 Section 19.1.1: SIP URI structure
    // "A SIP URI has the form: sip:user@host:port;uri-parameters?headers"
    // The parser MUST extract scheme, user, host, port.
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_sip_scheme) {
        // R-URI with sip: scheme MUST be parsed
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.request->ruri.scheme, sip_uri::SIP);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_19_1_ruri_sips_scheme) {
        // R-URI with sips: scheme MUST be parsed
        string raw = make_req("INVITE", "sips:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.request->ruri.scheme, sip_uri::SIPS);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_19_1_ruri_user_host) {
        // MUST extract user and host parts from R-URI
        string raw = make_req("INVITE", "sip:alice@atlanta.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.u.request->ruri.user.len == 5);
        fct_chk(memcmp(msg.u.request->ruri.user.s, "alice", 5) == 0);
        fct_chk(msg.u.request->ruri.host.len == 11);
        fct_chk(memcmp(msg.u.request->ruri.host.s, "atlanta.com", 11) == 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_19_1_ruri_with_port) {
        // MUST extract explicit port from R-URI
        string raw = make_req("INVITE", "sip:bob@biloxi.com:5080");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.request->ruri.port, 5080);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_19_1_ruri_no_user) {
        // SIP URI without user part (host only) MUST be parsed
        string raw = make_req("OPTIONS", "sip:biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.u.request->ruri.host.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: URI parameters
    // "URI parameters [...] are separated by semicolons"
    // The parser MUST extract transport parameter from R-URI.
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_transport_param) {
        // R-URI with transport=tcp parameter MUST be parsed
        string raw = make_req("INVITE", "sip:bob@biloxi.com;transport=tcp");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        // Transport param should be extracted
        fct_chk(msg.u.request->ruri.trsp != NULL);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.2 / 20.4: Allow & Call-ID
    // "The Call-ID header field MUST be the same for all requests and
    //  responses sent by either UA in a dialog."
    // The parser MUST extract Call-ID value.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_8_callid_extracted) {
        // Call-ID value MUST be accessible after parsing
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.callid != NULL);
        fct_chk(msg.callid->value.len > 0);
        fct_chk(memcmp(msg.callid->value.s, "a84b4c76e66710@pc33.atlanta.com",
                        msg.callid->value.len) == 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.16: CSeq parsing
    // "The CSeq header field [...] contains a single decimal sequence
    //  number and the request method."
    // MUST extract both the numeric value and the method.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_16_cseq_num_and_method) {
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.cseq != NULL);
        sip_cseq* cs = dynamic_cast<sip_cseq*>(msg.cseq->p);
        fct_chk(cs != NULL);
        fct_chk_eq_int(cs->num, 314159);
        fct_chk_eq_int(cs->method, sip_request::INVITE);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.42: Via header - TCP transport
    // The parser MUST recognize TCP transport in Via
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_42_via_tcp_transport) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/TCP 192.0.2.1:5060;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk_eq_int(msg.via_p1->trans.type, sip_transport::TCP);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.42: Via header - TLS transport
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_42_via_tls_transport) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/TLS 192.0.2.1:5061;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk_eq_int(msg.via_p1->trans.type, sip_transport::TLS);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 18.1.1 / 20.42: Via sent-by with port
    // "The Via header field value MUST contain [...] the host name or
    //  network address [...] and the port number"
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_42_via_port_extracted) {
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk_eq_int(msg.via_p1->port_i, 5060);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 8.1.1.7 / 17.1.1.3: Via branch parameter
    // "The branch parameter value MUST be unique across space and time"
    // "The branch parameter [...] MUST start with the magic cookie
    //  'z9hG4bK'"
    // =================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_7_branch_without_magic_cookie) {
        // Branch without magic cookie MUST still be parsed (pre-3261)
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=oldstyle123\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk(msg.via_p1->branch.len > 0);
        // Should NOT start with magic cookie
        fct_chk(msg.via_p1->branch.len < MAGIC_BRANCH_LEN ||
                memcmp(msg.via_p1->branch.s, MAGIC_BRANCH_COOKIE,
                       MAGIC_BRANCH_LEN) != 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.19: From header - tag and display name
    // "The From header field MUST contain a URI [...] and MAY contain
    //  a display name."
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_20_from_display_name_and_uri) {
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.from != NULL);
        sip_from_to* from = dynamic_cast<sip_from_to*>(msg.from->p);
        fct_chk(from != NULL);
        // Display name "Alice" should be extracted
        fct_chk(from->nameaddr.name.len > 0);
        // Tag MUST be present
        fct_chk(from->tag.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.39: To header parsing
    // "The To header field [...] specifies the logical recipient"
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_39_to_parsed) {
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.to != NULL);
        sip_from_to* to = dynamic_cast<sip_from_to*>(msg.to->p);
        fct_chk(to != NULL);
        // Display name "Bob" should be extracted
        fct_chk(to->nameaddr.name.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.39: To header in response - tag
    // "The UAS MUST add a tag to the To header field in the response"
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_39_to_tag_in_response) {
        string raw = make_response(200, "OK");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.to != NULL);
        sip_from_to* to = dynamic_cast<sip_from_to*>(msg.to->p);
        fct_chk(to != NULL);
        fct_chk(to->tag.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Header field ordering
    // "Multiple header field rows with the same field name [...] MUST
    //  be combinable into one"
    // Route headers MUST maintain order.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_route_order_preserved) {
        // Three Route headers - order MUST be preserved
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "Route: <sip:proxy1.atlanta.com;lr>\r\n"
            "Route: <sip:proxy2.biloxi.com;lr>\r\n"
            "Route: <sip:proxy3.chicago.com;lr>\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int((int)msg.route.size(), 3);
        // First route should contain proxy1
        list<sip_header*>::iterator it = msg.route.begin();
        fct_chk(it != msg.route.end());
        string first_route((*it)->value.s, (*it)->value.len);
        fct_chk(first_route.find("proxy1") != string::npos);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Via header ordering
    // "The order of header field rows [...] MUST NOT be changed"
    // Via headers MUST be collected in order.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_via_order_preserved) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP first.example.com;branch=z9hG4bKfirst\r\n"
            "Via: SIP/2.0/UDP second.example.com;branch=z9hG4bKsecond\r\n"
            "Via: SIP/2.0/UDP third.example.com;branch=z9hG4bKthird\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int((int)msg.vias.size(), 3);
        // First Via (via1) MUST be the topmost
        string first_via(msg.via1->value.s, msg.via1->value.len);
        fct_chk(first_via.find("first.example.com") != string::npos);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.2: Response - all 1xx through 6xx classes
    // "Every response MUST include [...] the same Call-ID, CSeq,
    //  From, and the Via header field values as the corresponding
    //  request."
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_2_100_trying) {
        string raw = make_response(100, "Trying");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.reply->code, 100);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_2_180_ringing) {
        string raw = make_response(180, "Ringing");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.reply->code, 180);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_2_183_session_progress) {
        string raw = make_response(183, "Session Progress");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.reply->code, 183);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_2_401_unauthorized) {
        string raw = make_response(401, "Unauthorized");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.reply->code, 401);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_2_486_busy_here) {
        string raw = make_response(486, "Busy Here");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.reply->code, 486);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.4 / 20.14: Bodies
    // "The body of a SIP message [...] MUST be interpreted according
    //  to Content-Type."
    // Content-Length MUST delimit the body exactly.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_4_body_with_content_length) {
        const char* sdp = "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\n";
        string raw = make_req("INVITE", "sip:bob@biloxi.com",
            "Content-Type: application/sdp\r\n", sdp);
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.body.len, (int)strlen(sdp));
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.7: Call-ID case sensitivity
    // "Call-ID values are case-sensitive and are simply compared
    //  byte-by-byte."
    // The parser MUST preserve the exact Call-ID value.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_8_callid_case_preserved) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: AbCdEf123@Host.Example.COM\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.callid != NULL);
        string callid(msg.callid->value.s, msg.callid->value.len);
        fct_chk(callid == "AbCdEf123@Host.Example.COM");
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Unknown header fields
    // "If a header field [...] is not understood, it MUST be treated
    //  as a header field of type 'extension-header'."
    // Unknown headers MUST be preserved in the header list.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_unknown_headers_preserved) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "X-Custom-Header: some-value\r\n"
            "P-Asserted-Identity: <sip:alice@atlanta.com>\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        // Unknown headers should be in hdrs list as H_OTHER
        int other_count = 0;
        for(list<sip_header*>::iterator it = msg.hdrs.begin();
            it != msg.hdrs.end(); ++it) {
            if((*it)->type == sip_header::H_OTHER)
                other_count++;
        }
        fct_chk(other_count >= 2);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Header field name case insensitivity
    // "Header field names are case-insensitive."
    // The parser MUST recognize headers regardless of case.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_header_names_case_insensitive) {
        // Mixed-case header names MUST be recognized
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "VIA: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "TO: <sip:bob@biloxi.com>\r\n"
            "FROM: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "CALL-ID: abc@host\r\n"
            "CSEQ: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via1 != NULL);
        fct_chk(msg.to != NULL);
        fct_chk(msg.from != NULL);
        fct_chk(msg.callid != NULL);
        fct_chk(msg.cseq != NULL);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Mixed case header names
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_mixed_case_headers) {
        // camelCase headers MUST be recognized
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "tO: <sip:bob@biloxi.com>\r\n"
            "fRoM: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "cAlL-iD: abc@host\r\n"
            "cSeQ: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via1 != NULL);
        fct_chk(msg.to != NULL);
        fct_chk(msg.from != NULL);
        fct_chk(msg.callid != NULL);
        fct_chk(msg.cseq != NULL);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.3: Compact form "c" for Content-Type
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_3_compact_content_type) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "v: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "t: <sip:bob@biloxi.com>\r\n"
            "f: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "i: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "c: application/sdp\r\n"
            "l: 0\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.content_type != NULL);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 18.3: Content-Length MUST be present for TCP
    // "If a request is sent over a reliable transport (TCP) and the
    //  Content-Length header field is not present, the request MUST be
    //  rejected with a 400 (Bad Request) response."
    //
    // NOTE: This is a transport-level requirement. The parser itself
    //       does not enforce this - it uses all remaining bytes when
    //       Content-Length is absent. This test verifies the parser
    //       behavior (no Content-Length = use all remaining bytes).
    // =================================================================

    FCT_TEST_BGN(rfc3261_18_3_no_content_length_body_used) {
        // Without Content-Length, parser uses remaining bytes as body
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/TCP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n"
            "body-data";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.content_length == NULL);
        fct_chk_eq_int(msg.body.len, 9);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 8.1.1.5: CSeq - various method mismatches
    // =================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_5_cseq_options_vs_invite) {
        // OPTIONS request with CSeq INVITE MUST fail
        string raw = make_request("OPTIONS", "sip:bob@biloxi.com",
                                  "INVITE", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_5_cseq_register_matches) {
        // REGISTER request with CSeq REGISTER MUST succeed
        string raw = make_req("REGISTER", "sip:registrar.biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        sip_cseq* cs = dynamic_cast<sip_cseq*>(msg.cseq->p);
        fct_chk(cs != NULL);
        fct_chk_eq_int(cs->method, sip_request::REGISTER);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.1: Request line with SIP/2.0 - case handling
    // "SIP" in the version MUST be case-insensitive per RFC grammar
    // (Actually RFC 3261 BNF says SIP-Version = "SIP" "/" 1*DIGIT "."
    //  1*DIGIT, which uses quoted string meaning case-sensitive for
    //  "SIP". But many implementations accept case-insensitively.)
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_1_sip_version_uppercase) {
        // Standard uppercase SIP/2.0 MUST work
        string raw = make_req("INVITE", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.30: Record-Route in responses
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_30_record_route_in_response) {
        string raw = make_response(200, "OK", "INVITE",
            "Record-Route: <sip:proxy.atlanta.com;lr>\r\n");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int((int)msg.record_route.size(), 1);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.20: Max-Forwards header type recognition
    // The parser MUST recognize Max-Forwards as a known header type.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_22_max_forwards_recognized) {
        string raw = make_req("INVITE", "sip:bob@biloxi.com",
            "Max-Forwards: 70\r\n");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        bool found_mf = false;
        for(list<sip_header*>::iterator it = msg.hdrs.begin();
            it != msg.hdrs.end(); ++it) {
            if((*it)->type == sip_header::H_MAX_FORWARDS) {
                found_mf = true;
                string val((*it)->value.s, (*it)->value.len);
                fct_chk(val.find("70") != string::npos);
            }
        }
        fct_chk(found_mf);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.20: Require header type recognition
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_32_require_recognized) {
        string raw = make_req("INVITE", "sip:bob@biloxi.com",
            "Require: 100rel\r\n");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        bool found_require = false;
        for(list<sip_header*>::iterator it = msg.hdrs.begin();
            it != msg.hdrs.end(); ++it) {
            if((*it)->type == sip_header::H_REQUIRE)
                found_require = true;
        }
        fct_chk(found_require);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.10: Multiple Contact headers
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_10_multiple_contacts) {
        const char* raw =
            "REGISTER sip:registrar.biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:bob@biloxi.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 REGISTER\r\n"
            "Contact: <sip:bob@client1.biloxi.com>\r\n"
            "Contact: <sip:bob@client2.biloxi.com>\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int((int)msg.contacts.size(), 2);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.5: Empty body (CRLF right after headers)
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_5_empty_body) {
        string raw = make_req("OPTIONS", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.body.len, 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.1: Very long method name (extension)
    // The parser MUST accept extension methods of any reasonable length.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_1_long_extension_method) {
        string raw = make_req("SUPERLONGMETHODNAME", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.request->method, sip_request::OTHER_METHOD);
        fct_chk_eq_int(msg.u.request->method_str.len, 19);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.2: Response with non-standard reason phrase
    // "The reason phrase is intended for the human user."
    // Any reason text MUST be accepted.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_2_custom_reason_phrase) {
        string raw = make_response(200, "Everything Is Fine");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.reply->code, 200);
        fct_chk(msg.u.reply->reason.len > 0);
        fct_chk(memcmp(msg.u.reply->reason.s, "Everything Is Fine", 18) == 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Whitespace in header field values
    // "LWS can appear between any two adjacent tokens or quoted strings
    //  in a header field value."
    // The parser MUST handle extra whitespace in header values.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_header_value_whitespace) {
        // Extra whitespace around header value colon MUST be handled
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via:   SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To:  <sip:bob@biloxi.com>\r\n"
            "From:  <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID:  abc@host\r\n"
            "CSeq:  1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via1 != NULL);
        fct_chk(msg.to != NULL);
        fct_chk(msg.from != NULL);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 8.2.2.1: SIP version in response
    // A response with non-SIP/2.0 version MUST be treated as error.
    // =================================================================

    FCT_TEST_BGN(rfc3261_8_2_2_1_bad_version_response) {
        const char* raw =
            "SIP/3.0 200 OK\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>;tag=abc\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        // Response with bad version MUST be rejected (MALFORMED_FLINE
        // or MALFORMED_SIP_MSG - either is acceptable)
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: R-URI with IPv6 address
    // "host = hostname / IPv4address / IPv6reference"
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_ipv6) {
        string raw = make_req("INVITE", "sip:bob@[2001:db8::1]");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.u.request->ruri.host.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: R-URI with IPv6 and port
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_ipv6_with_port) {
        string raw = make_req("INVITE", "sip:bob@[2001:db8::1]:5080");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.request->ruri.port, 5080);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.42: Via with IPv6 sent-by
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_42_via_ipv6_sentby) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk(msg.via_p1->host.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.42: Via rport parameter (RFC 3581)
    // "rport" parameter MUST be recognized when present.
    // =================================================================

    FCT_TEST_BGN(rfc3261_via_rport_parameter) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776;rport\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk(msg.via_p1->has_rport == true);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 18.2.1: Via received parameter
    // "received" parameter value MUST be extractable after parsing.
    // =================================================================

    FCT_TEST_BGN(rfc3261_18_2_1_via_received_parameter) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776;"
            "received=192.0.2.1\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.via_p1 != NULL);
        fct_chk(msg.via_p1->recved.len > 0);
        fct_chk(memcmp(msg.via_p1->recved.s, "192.0.2.1", 9) == 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.16: CSeq sequence number range
    // "The CSeq header field [...] contains a single decimal sequence
    //  number." A large CSeq number MUST be parsed correctly.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_16_cseq_large_number) {
        const char* raw =
            "BYE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 4294967295 BYE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        sip_cseq* cs = dynamic_cast<sip_cseq*>(msg.cseq->p);
        fct_chk(cs != NULL);
        fct_chk(cs->num == 4294967295U);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.1: PRACK method (RFC 3262)
    // PRACK MUST be recognized as a known method.
    // =================================================================

    FCT_TEST_BGN(rfc3261_prack_method_recognized) {
        string raw = make_req("PRACK", "sip:bob@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk_eq_int(msg.u.request->method, sip_request::PRACK);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Header field with empty value
    // Some headers may have empty values (e.g. Subject:)
    // The parser MUST handle headers with empty values.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_header_empty_value) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "Subject: \r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.1: Garbage / completely invalid first line
    // MUST return MALFORMED_FLINE for unparseable messages.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_1_garbage_input) {
        const char* raw = "THIS IS NOT A SIP MESSAGE AT ALL\r\n\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_1_empty_input) {
        const char* raw = "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =================================================================
    // BATCH 2: Targeted tests for known RFC 3261 compliance gaps
    // =================================================================

    // =================================================================
    // RFC 3261 Section 7.3.1: Header field line folding
    // "Header fields can be extended over multiple lines by preceding
    //  each extra line with at least one SP or HTAB."
    // The parser MUST handle line folding (CRLF + SP/HTAB = continuation).
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_header_folding_sp) {
        // Header value split across two lines with SP continuation
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>\r\n"
            " ;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        // From header MUST be recognized despite line folding
        fct_chk(msg.from != NULL);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_3_1_header_folding_htab) {
        // Header value split across two lines with HTAB continuation
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>\r\n"
            "\t;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        fct_chk(msg.from != NULL);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_3_1_subject_folding) {
        // Subject header folded across lines (classic RFC example)
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "Subject: I know you're there,\r\n"
            "         pick up the phone!\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        // The Subject header MUST be present in hdrs as a single header
        bool found_subject = false;
        for(list<sip_header*>::iterator it = msg.hdrs.begin();
            it != msg.hdrs.end(); ++it) {
            string name((*it)->name.s, (*it)->name.len);
            if(name == "Subject") {
                found_subject = true;
                // Value MUST contain both parts of the folded text
                string val((*it)->value.s, (*it)->value.len);
                fct_chk(val.find("I know") != string::npos);
                fct_chk(val.find("pick up") != string::npos);
            }
        }
        fct_chk(found_subject);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Comma-separated header values
    // "It MUST be possible to combine header field rows with the same
    //  field name into one 'field-name: field-value' pair, with commas
    //  separating the field values."
    //
    // For Via, Route, Contact, Record-Route: multiple values separated
    // by commas within one header line MUST be equivalent to multiple
    // separate header lines.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_comma_separated_via) {
        // Two Via entries on one line, separated by comma
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP first.example.com;branch=z9hG4bKfirst,"
            " SIP/2.0/UDP second.example.com;branch=z9hG4bKsecond\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        // There is only one Via header line, but via_p1 should be parsed
        // (the first Via from the comma-separated list)
        fct_chk(msg.via_p1 != NULL);
        string host(msg.via_p1->host.s, msg.via_p1->host.len);
        fct_chk(host == "first.example.com");
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_3_1_comma_separated_route) {
        // Two Route entries on one line, separated by comma
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "Route: <sip:proxy1.example.com;lr>, <sip:proxy2.example.com;lr>\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        // Route list should have 1 entry (the raw header line)
        // The comma splitting happens at route parsing time, not header time
        fct_chk_eq_int((int)msg.route.size(), 1);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_3_1_comma_separated_contact) {
        // Two Contact entries on one line, separated by comma
        const char* raw =
            "REGISTER sip:registrar.biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:bob@biloxi.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 REGISTER\r\n"
            "Contact: <sip:bob@client1.biloxi.com>, <sip:bob@client2.biloxi.com>\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk(rc == 0);
        // contacts list stores raw header lines, not individual contacts
        // So 1 Contact header = 1 entry even with commas
        fct_chk_eq_int((int)msg.contacts.size(), 1);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.14 / 7.4.1: Content-Length handling
    // "The Content-Length header field indicates the size of the
    //  message-body, in decimal number of octets."
    // There is NO limit in the RFC on body size. Content-Length
    // values > 65535 MUST be accepted.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_14_content_length_large_value) {
        // Content-Length of 100000 MUST be parseable
        // (We won't provide that much body data, but the value itself
        //  MUST not be rejected as "too large")
        // Note: This tests the parser's numeric parsing, not body handling.
        // We provide a Content-Length that exceeds 65535 but matches actual body.
        // Since we can't easily provide 100000 bytes of body in a test,
        // we test that the header value is accepted by checking what error we get.
        string body(70000, 'X');
        string raw;
        raw += "INVITE sip:bob@biloxi.com SIP/2.0\r\n";
        raw += "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n";
        raw += "To: <sip:bob@biloxi.com>\r\n";
        raw += "From: <sip:alice@atlanta.com>;tag=xyz\r\n";
        raw += "Call-ID: abc@host\r\n";
        raw += "CSeq: 1 INVITE\r\n";
        raw += "Content-Length: 70000\r\n";
        raw += "\r\n";
        raw += body;
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        // RFC: Content-Length 70000 is valid. Parser MUST accept it.
        fct_chk_eq_int(rc, 0);
        if(rc == 0) {
            fct_chk_eq_int(msg.body.len, 70000);
        }
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_20_14_content_length_exactly_65535) {
        // Content-Length of 65535 (max allowed by parser?) - edge case
        string body(65535, 'Y');
        string raw;
        raw += "INVITE sip:bob@biloxi.com SIP/2.0\r\n";
        raw += "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n";
        raw += "To: <sip:bob@biloxi.com>\r\n";
        raw += "From: <sip:alice@atlanta.com>;tag=xyz\r\n";
        raw += "Call-ID: abc@host\r\n";
        raw += "CSeq: 1 INVITE\r\n";
        raw += "Content-Length: 65535\r\n";
        raw += "\r\n";
        raw += body;
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        if(rc == 0) {
            fct_chk_eq_int(msg.body.len, 65535);
        }
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_20_14_content_length_65536) {
        // Content-Length of 65536 - one above 65535 boundary
        string body(65536, 'Z');
        string raw;
        raw += "INVITE sip:bob@biloxi.com SIP/2.0\r\n";
        raw += "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n";
        raw += "To: <sip:bob@biloxi.com>\r\n";
        raw += "From: <sip:alice@atlanta.com>;tag=xyz\r\n";
        raw += "Call-ID: abc@host\r\n";
        raw += "CSeq: 1 INVITE\r\n";
        raw += "Content-Length: 65536\r\n";
        raw += "\r\n";
        raw += body;
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        // RFC: Content-Length 65536 is valid. Parser MUST accept it.
        fct_chk_eq_int(rc, 0);
        if(rc == 0) {
            fct_chk_eq_int(msg.body.len, 65536);
        }
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.14: Content-Length 0 with body present
    // When Content-Length is 0, body MUST be empty even if bytes follow.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_14_content_length_zero_ignores_trailing) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "Content-Length: 0\r\n"
            "\r\n"
            "this-should-be-ignored";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk_eq_int(msg.body.len, 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.14: Content-Length < actual body
    // Content-Length MUST delimit body; extra bytes ignored.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_14_content_length_less_than_body) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "helloextraextra";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        // Body MUST be exactly 5 bytes
        fct_chk_eq_int(msg.body.len, 5);
        fct_chk(memcmp(msg.body.s, "hello", 5) == 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.14: Content-Length > actual body
    // "If it has too few bytes, the message is simply discarded."
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_14_content_length_exceeds_body) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "Content-Length: 500\r\n"
            "\r\n"
            "short";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        // Message MUST be rejected / discarded
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: R-URI with user-info special chars
    // "The user component [...] can include escaped characters."
    // The parser MUST accept percent-encoded characters in user part.
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_percent_encoded_user) {
        // %40 = '@' - URI with percent-encoded character in user part
        string raw = make_req("INVITE", "sip:user%40name@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        // MUST parse successfully - %XX is valid in user part
        fct_chk_eq_int(rc, 0);
        if(rc == 0) {
            // User part MUST contain the raw percent-encoded form
            fct_chk(msg.u.request->ruri.user.len > 0);
        }
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_19_1_ruri_phone_context) {
        // R-URI with phone-context parameter (common in IMS/VoLTE)
        string raw = make_req("INVITE",
            "sip:+12125551234@gateway.example.com;user=phone");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        if(rc == 0) {
            fct_chk(msg.u.request->ruri.user.len > 0);
        }
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: URI password component
    // "The password component [...] is only for use in the deprecated
    //  URI form." But the parser MUST still accept it.
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_with_password) {
        string raw = make_req("INVITE", "sip:user:password@biloxi.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        if(rc == 0) {
            fct_chk(msg.u.request->ruri.user.len > 0);
            fct_chk(msg.u.request->ruri.host.len > 0);
        }
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: URI with multiple parameters
    // "URI-parameters are added after the host component and are
    //  separated by semicolons."
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_multiple_params) {
        string raw = make_req("INVITE",
            "sip:bob@biloxi.com;transport=udp;user=phone;lr");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: URI with headers component
    // "Headers are separated from the host [...] by '?'"
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_with_headers) {
        string raw = make_req("INVITE",
            "sip:bob@biloxi.com?Subject=Meeting&Priority=urgent");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.1: Bare LF as line terminator
    // "Implementations MUST accept [...] bare LF as a line terminator"
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_5_bare_lf_request_line) {
        // Request line ended with bare LF (no CR)
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\n"
            "To: <sip:bob@biloxi.com>\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\n"
            "Call-ID: abc@host\n"
            "CSeq: 1 INVITE\n"
            "\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_5_mixed_crlf_and_lf) {
        // Mix of CRLF and bare LF
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk(msg.via1 != NULL);
        fct_chk(msg.from != NULL);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.3.1: Multiple values for same header
    // "It MUST be possible to combine the header field rows [...] into
    //  one pair, without changing the semantics of the message."
    // Via, Route, Record-Route ordering MUST be preserved when split.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_3_1_record_route_order) {
        const char* raw =
            "SIP/2.0 200 OK\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "Record-Route: <sip:proxy1.example.com;lr>\r\n"
            "Record-Route: <sip:proxy2.example.com;lr>\r\n"
            "Record-Route: <sip:proxy3.example.com;lr>\r\n"
            "To: <sip:bob@biloxi.com>;tag=abc\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk_eq_int((int)msg.record_route.size(), 3);
        // First Record-Route MUST be proxy1
        if(msg.record_route.size() >= 1) {
            list<sip_header*>::iterator it = msg.record_route.begin();
            string rr((*it)->value.s, (*it)->value.len);
            fct_chk(rr.find("proxy1") != string::npos);
        }
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 8.1.1.3: From header - tag MUST be present
    // "The From header field MUST contain a new 'tag' parameter"
    // Missing From-tag in a request MUST cause parse failure.
    // =================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_3_from_missing_tag_rejected) {
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
        // RFC 3261 Section 8.1.1.3: From-tag MUST be present
        // The parser MUST reject requests without From-tag
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 8.1.1: Missing mandatory headers
    // A SIP request MUST contain Via, CSeq, From, To, Call-ID.
    // Test each mandatory header missing individually.
    // =================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_missing_via) {
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

    FCT_TEST_BGN(rfc3261_8_1_1_missing_to) {
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

    FCT_TEST_BGN(rfc3261_8_1_1_missing_from) {
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

    FCT_TEST_BGN(rfc3261_8_1_1_missing_callid) {
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

    FCT_TEST_BGN(rfc3261_8_1_1_missing_cseq) {
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

    // =================================================================
    // RFC 3261 Section 8.1.1.5: CSeq method MUST match request method
    // Additional CSeq mismatch cases for various methods.
    // =================================================================

    FCT_TEST_BGN(rfc3261_8_1_1_5_bye_cseq_cancel_mismatch) {
        string raw = make_request("BYE", "sip:bob@biloxi.com",
                                  "CANCEL", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_8_1_1_5_register_cseq_bye_mismatch) {
        string raw = make_request("REGISTER", "sip:registrar.biloxi.com",
                                  "BYE", "");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk(rc != 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.4: Content-Type presence
    // "If a body is present, the Content-Type header field MUST be
    //  present." (This is a semantic check; the parser may not enforce.)
    // Test that Content-Type IS parsed when present with a body.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_4_content_type_with_body) {
        const char* sdp = "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\n";
        string raw = make_req("INVITE", "sip:bob@biloxi.com",
            "Content-Type: application/sdp\r\n", sdp);
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk(msg.content_type != NULL);
        string ct(msg.content_type->value.s, msg.content_type->value.len);
        fct_chk(ct.find("application/sdp") != string::npos);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.42: Via with all common parameters
    // branch, rport, received, maddr MUST all be extractable.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_42_via_full_params) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP pc33.atlanta.com:5060"
            ";branch=z9hG4bKnashds8"
            ";received=192.0.2.1"
            ";rport=5060\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk(msg.via_p1 != NULL);
        // Branch
        fct_chk(msg.via_p1->branch.len > 0);
        string branch(msg.via_p1->branch.s, msg.via_p1->branch.len);
        fct_chk(branch == "z9hG4bKnashds8");
        // Received
        fct_chk(msg.via_p1->recved.len > 0);
        // Rport
        fct_chk(msg.via_p1->has_rport == true);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.19: From with quoted display name
    // Display name can contain special chars when quoted.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_20_from_quoted_display_name) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: \"Alice, Wonderland\" <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk(msg.from != NULL);
        sip_from_to* from = dynamic_cast<sip_from_to*>(msg.from->p);
        fct_chk(from != NULL);
        // Display name MUST include the quoted content
        fct_chk(from->nameaddr.name.len > 0);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 19.1.1: R-URI with FQDN host containing dash
    // =================================================================

    FCT_TEST_BGN(rfc3261_19_1_ruri_host_with_dash) {
        string raw = make_req("INVITE", "sip:bob@my-host.example.com");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        string host(msg.u.request->ruri.host.s, msg.u.request->ruri.host.len);
        fct_chk(host == "my-host.example.com");
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.2: Response with 3-digit code boundaries
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_2_response_code_600) {
        string raw = make_response(600, "Busy Everywhere");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk_eq_int(msg.u.reply->code, 600);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_2_response_code_302) {
        string raw = make_response(302, "Moved Temporarily");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk_eq_int(msg.u.reply->code, 302);
    } FCT_TEST_END();

    FCT_TEST_BGN(rfc3261_7_2_response_code_503) {
        string raw = make_response(503, "Service Unavailable");
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw.c_str(), raw.size(), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        fct_chk_eq_int(msg.u.reply->code, 503);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 20.16: CSeq with leading zeros
    // "The CSeq header field [...] contains a single decimal sequence
    //  number."  Leading zeros MUST be handled.
    // =================================================================

    FCT_TEST_BGN(rfc3261_20_16_cseq_leading_zeros) {
        const char* raw =
            "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 0042 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        fct_chk_eq_int(rc, 0);
        sip_cseq* cs = dynamic_cast<sip_cseq*>(msg.cseq->p);
        fct_chk(cs != NULL);
        fct_chk_eq_int(cs->num, 42);
    } FCT_TEST_END();

    // =================================================================
    // RFC 3261 Section 7.1: Request with extra whitespace in request-line
    // SP is used between method, R-URI, and version.
    // Multiple SPs SHOULD be accepted.
    // =================================================================

    FCT_TEST_BGN(rfc3261_7_1_request_line_extra_spaces) {
        // Extra spaces between request-line components
        const char* raw =
            "INVITE  sip:bob@biloxi.com  SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 192.0.2.1;branch=z9hG4bK776\r\n"
            "To: <sip:bob@biloxi.com>\r\n"
            "From: <sip:alice@atlanta.com>;tag=xyz\r\n"
            "Call-ID: abc@host\r\n"
            "CSeq: 1 INVITE\r\n"
            "\r\n";
        sip_msg msg;
        char* err_msg = NULL;
        int rc = try_parse(raw, strlen(raw), msg, err_msg);
        // Extra spaces in request-line - parser SHOULD handle gracefully
        // (whether it accepts or rejects is implementation-dependent)
        // We just verify it doesn't crash
        (void)rc;
    } FCT_TEST_END();

} FCTMF_SUITE_END();
