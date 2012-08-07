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
      fct_chk( p.parse_contact("  \"hu bar\\\\\" <sip:u@d;tag=123>", 0, end) );
      fct_chk( p.display_name=="hu bar\\");
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


} FCTMF_SUITE_END();
