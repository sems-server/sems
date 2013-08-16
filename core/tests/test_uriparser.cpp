#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "AmUtils.h"
#include "AmUriParser.h"

FCTMF_SUITE_BGN(test_uriparser) {

    FCT_TEST_BGN(uriparser_simple) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("sip:u@d", 0, end) );
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_angle) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("<sip:u@d>", 0, end) );
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_angle_param) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("<sip:u@d>;tag=123", 0, end) );
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
      fct_chk( p.params["tag"]=="123");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_uri_param) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("<sip:u@d;tag=123>", 0, end) );
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
      fct_chk( p.uri_param=="tag=123");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_params_nobracket) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("sip:u@d;tag=123", 0, end) );
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
      fct_chk( p.params["tag"]=="123");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_params_dname) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("hu <sip:u@d;tag=123>", 0, end) );
      // DBG("DN:: '%s'\n", p.display_name.c_str());
      fct_chk( p.display_name=="hu");
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_params_dname2) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("  hu bar <sip:u@d;tag=123>", 0, end) );
      // DBG("DN:: '%s'\n", p.display_name.c_str());

      fct_chk( p.display_name=="hu bar");
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_params_dname3) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("  \"hu bar\" <sip:u@d;tag=123>", 0, end) );
      fct_chk( p.display_name=="hu bar");
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_params_dname4) {
      AmUriParser p;
      size_t end;
      fct_chk( p.parse_contact("  \"hu bar\\\\ \" <sip:u@d;tag=123>", 0, end) );
      // fct_chk( p.parse_contact("  \"hu bar\\\\\" <sip:u@d;tag=123>", 0, end) );
      fct_chk( p.display_name=="hu bar\\\\ ");
      fct_chk( p.uri_user=="u");
      fct_chk( p.uri_host=="d");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_params_dname4) {
      AmUriParser p;
      size_t end;
      fct_chk(p.parse_contact("\"Mr. Watson\" <mailto:watson@bell-telephone.com> ;q=0.1", 0, end));
      fct_chk( p.display_name=="Mr. Watson");
      fct_chk( p.uri_user=="watson");
      fct_chk( p.uri_host=="bell-telephone.com");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_headers) {
      AmUriParser p;
      size_t end;
      fct_chk(p.parse_contact("\"Mr. Watson\" <mailto:watson@bell-telephone.com?Replaces:%20lkancskjd%3Bto-tag=3123141ab%3Bfrom-tag=kjhkjcsd> ;q=0.1", 0, end));
      fct_chk( p.display_name=="Mr. Watson");
      fct_chk( p.uri_user=="watson");
      fct_chk( p.uri_host=="bell-telephone.com");
      fct_chk( p.uri_headers=="Replaces:\%20lkancskjd%3Bto-tag=3123141ab%3Bfrom-tag=kjhkjcsd");
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_headers_str) {
      AmUriParser p;
      string orig_str = "\"Mr. Watson\" <sip:watson@bell-telephone.com?Replaces:%20lkancskjd%3Bto-tag=3123141ab%3Bfrom-tag=kjhkjcsd>;q=0.1";
      fct_chk(p.parse_nameaddr(orig_str));
      fct_chk( p.display_name=="Mr. Watson");
      fct_chk( p.uri_user=="watson");
      fct_chk( p.uri_host=="bell-telephone.com");
      fct_chk( p.uri_headers=="Replaces:\%20lkancskjd%3Bto-tag=3123141ab%3Bfrom-tag=kjhkjcsd");
      string a_str = p.nameaddr_str();
      // DBG(" >>%s<< => >>%s<<\n", orig_str.c_str(), a_str.c_str());
      fct_chk(orig_str == a_str);
    } FCT_TEST_END();

    FCT_TEST_BGN(uriparser_url_escape) {
      string src = "Replaces: CSADFSD;from-tag=31241231abc;to-tag=235123";
      string dst = "Replaces%3A%20CSADFSD%3Bfrom-tag%3D31241231abc%3Bto-tag%3D235123";
      fct_chk ( URL_decode(dst)==src  );
      fct_chk ( URL_encode(src)==dst  );
      fct_chk ( URL_decode(URL_encode(src))==src  );

    } FCT_TEST_END();

} FCTMF_SUITE_END();
