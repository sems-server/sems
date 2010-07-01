#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"

FCTMF_SUITE_BGN(test_headers) {

    FCT_TEST_BGN(getHeader_simple) {
        fct_chk( getHeader("P-My-Test: myval" CRLF, "P-My-Test") == "myval");
    } FCT_TEST_END();
    FCT_TEST_BGN(getHeader_multi) {
      fct_chk( getHeader("P-My-Test: myval" CRLF "P-My-Test: myval2" CRLF , "P-My-Test", true) == "myval" );
      fct_chk( getHeader("P-My-Test: myval" CRLF "P-My-Test: myval2" CRLF , "P-My-Test", false) == "myval, myval2" );
      fct_chk( getHeader("P-My-Test: myval" CRLF "P-My-Otherheader: myval2" CRLF "P-My-Test: myval2" CRLF , "P-My-Test", false) == "myval, myval2" );
    } FCT_TEST_END();

} FCTMF_SUITE_END();
