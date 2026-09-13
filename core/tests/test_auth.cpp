#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "AmUtils.h"
#include "md5.h"
#include "plug-in/uac_auth/UACAuth.h"

#include <string>

namespace {

string md5_hex(const string &s) {
  static const char hex[] = "0123456789abcdef";
  MD5_CTX ctx;
  unsigned char digest[16];
  MD5Init(&ctx);
  MD5Update(&ctx, (const unsigned char *)s.data(), s.length());
  MD5Final(digest, &ctx);
  string res;
  for (int i = 0; i < 16; i++) {
    res += hex[digest[i] >> 4];
    res += hex[digest[i] & 0x0f];
  }
  return res;
}

// the request-digest a client computes (RFC 2617 section 3.2.2.1) for the
// request auth_request() builds, over the nonce-count exactly as it sends it
string client_response(const string &pwd, const string &nonce, const string &qop, const string &nc,
                       const string &body = "") {
  string ha1 = md5_hex("alice:example.com:" + pwd);
  string ha2 = md5_hex("INVITE:sip:example.com" + (qop == "auth-int" ? ":" + md5_hex(body) : string()));
  if (qop.empty()) {
    return md5_hex(ha1 + ":" + nonce + ":" + ha2);
  }
  return md5_hex(ha1 + ":" + nonce + ":" + nc + ":0a4f113b:" + qop + ":" + ha2);
}

// INVITE carrying credentials for user alice in realm example.com; qop and nc
// are left out when empty
AmSipRequest auth_request(const string &nonce, const string &qop, const string &nc, const string &response) {
  AmSipRequest req;
  req.method = SIP_METH_INVITE;
  req.hdrs = SIP_HDR_COLSP(SIP_HDR_AUTHORIZATION) "Digest username=\"alice\", realm=\"example.com\", ";
  req.hdrs += "nonce=\"" + nonce + "\", uri=\"sip:example.com\", ";
  if (!qop.empty()) {
    req.hdrs += "qop=" + qop + ", ";
  }
  if (!nc.empty()) {
    req.hdrs += "nc=" + nc + ", ";
  }
  if (!qop.empty()) {
    req.hdrs += "cnonce=\"0a4f113b\", ";
  }
  req.hdrs += "response=\"" + response + "\", algorithm=MD5" CRLF;
  return req;
}

// @return the status code checkAuthentication() answers req with, expecting
// password "secret"
int check_auth(const AmSipRequest &req) {
  AmArg ret;
  UACAuth::checkAuthentication(&req, "example.com", "alice", "secret", ret);
  return ret.size() ? ret.get(0).asInt() : -1;
}

} // namespace

FCTMF_SUITE_BGN(test_auth) {

  FCT_TEST_BGN(nonce_gen) {

    string secret = "1234secret";
    string nonce = UACAuth::calcNonce();
    //      DBG("nonce '%s'\n", nonce.c_str());
    fct_chk(UACAuth::checkNonce(nonce));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(nonce_wrong_secret) {
    string secret = "1234secret";
    UACAuth::setServerSecret(secret);
    string nonce = UACAuth::calcNonce();

    UACAuth::setServerSecret(secret + "asd");
    fct_chk(!UACAuth::checkNonce(nonce));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(nonce_wrong_nonce) {
    string secret = "1234secret";
    string nonce = UACAuth::calcNonce();
    nonce[0] = 0;
    nonce[1] = 0;
    fct_chk(!UACAuth::checkNonce(nonce));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(nonce_wrong_nonce) {
    string secret = "1234secret";
    string nonce = UACAuth::calcNonce();
    nonce += "hallo";
    fct_chk(!UACAuth::checkNonce(nonce));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(nonce_wrong_nonce2) {
    string secret = "1234secret";
    string nonce = UACAuth::calcNonce();
    // Invert last char
    char array[] = "fedcba9876543210";
    unsigned char last_char = nonce[nonce.size() - 1];
    if (('a' <= last_char) && (last_char <= 'f')) {
      last_char = last_char - 'a' + 10;
    } else {
      last_char = last_char - '0';
    }
    nonce[nonce.size() - 1] = array[last_char];
    fct_chk(!UACAuth::checkNonce(nonce));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(t_cmp_len) {
    string s1 = "1234secret";
    string s2 = "1234s3ecret";
    fct_chk(!UACAuth::tc_isequal(s1, s2));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(t_cmp_eq) {
    string s1 = "1234secret";
    string s2 = "1234secret";
    fct_chk(UACAuth::tc_isequal(s1, s2));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(t_cmp_empty) { fct_chk(UACAuth::tc_isequal("", "")); }
  FCT_TEST_END();

  FCT_TEST_BGN(t_cmp_uneq) { fct_chk(!UACAuth::tc_isequal("1234secret", "2134secret")); }
  FCT_TEST_END();

  FCT_TEST_BGN(t_cmp_uneq_chr) { fct_chk(!UACAuth::tc_isequal("1234secret", "2134secret", 10)); }
  FCT_TEST_END();

  FCT_TEST_BGN(t_cmp_eq_charptr) { fct_chk(UACAuth::tc_isequal("1234secret", "1234secret", 10)); }
  FCT_TEST_END();

  // checks the client side digest helper the following tests rely on
  FCT_TEST_BGN(client_response_rfc2617_example) {
    string ha1 = md5_hex("Mufasa:testrealm@host.com:Circle Of Life");
    string ha2 = md5_hex("GET:/dir/index.html");
    fct_chk(ha1 == "939e7578ed9e3c518a452acee763bce9");
    fct_chk(ha2 == "39aff3a2bab6126f332b942af96d3366");
    fct_chk(md5_hex(ha1 + ":dcd98b7102dd2f0e8b11d0f600bfb0c093:00000001:0a4f113b:auth:" + ha2) ==
            "6629fae49393a05397450978507c4ef1");
  }
  FCT_TEST_END();

  // the nonce-count is 8 hex digits, and its digits a-f must not break
  // authentication once a client has reused a nonce often enough
  FCT_TEST_BGN(uas_accepts_hex_nonce_counts) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    const char *counts[] = {"00000001", "00000009", "0000000a", "0000000f",
                            "00000010", "000000ff", "7fffffff", "ffffffff"};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
      string nc = counts[i];
      string response = client_response("secret", nonce, "auth", nc);
      int code = check_auth(auth_request(nonce, "auth", nc, response));
      fct_xchk(code == 200, "qop=auth, nc=%s: got %d", nc.c_str(), code);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(uas_accepts_hex_nonce_count_with_auth_int) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    // the request has no body, so H(entity-body) is that of an empty body
    string response = client_response("secret", nonce, "auth-int", "0000000b");
    fct_chk_eq_int(check_auth(auth_request(nonce, "auth-int", "0000000b", response)), 200);
  }
  FCT_TEST_END();

  // the nonce-count is digested as sent, so a client's upper-case hex digits
  // work as well
  FCT_TEST_BGN(uas_accepts_nonce_count_as_sent) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    string response = client_response("secret", nonce, "auth", "0000000A");
    fct_chk_eq_int(check_auth(auth_request(nonce, "auth", "0000000A", response)), 200);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(uas_rejects_nonce_count_other_than_digested) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    string response = client_response("secret", nonce, "auth", "0000000a");
    fct_chk_eq_int(check_auth(auth_request(nonce, "auth", "0000000b", response)), 401);
    fct_chk_eq_int(check_auth(auth_request(nonce, "auth", "00000010", response)), 401);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(uas_rejects_missing_nonce_count) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    string response = client_response("secret", nonce, "auth", "");
    fct_chk_eq_int(check_auth(auth_request(nonce, "auth", "", response)), 401);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(uas_rejects_wrong_password_with_hex_nonce_count) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    string response = client_response("wrong", nonce, "auth", "0000000a");
    fct_chk_eq_int(check_auth(auth_request(nonce, "auth", "0000000a", response)), 401);
  }
  FCT_TEST_END();

  // without qop (RFC 2069 compatibility) there is no nonce-count at all
  FCT_TEST_BGN(uas_accepts_digest_without_qop) {
    UACAuth::setServerSecret("test_auth nonce-count secret");
    string nonce = UACAuth::calcNonce();
    string response = client_response("secret", nonce, "", "");
    fct_chk_eq_int(check_auth(auth_request(nonce, "", "", response)), 200);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
