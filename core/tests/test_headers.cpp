#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "AmUtils.h"

FCTMF_SUITE_BGN(test_headers) {

    FCT_TEST_BGN(getHeader_simple) {
        fct_chk( getHeader("P-My-Test: myval" CRLF, "P-My-Test") == "myval");
    } FCT_TEST_END();

    FCT_TEST_BGN(getHeader_multi) {
      fct_chk( getHeader("P-My-Test: myval" CRLF "P-My-Test: myval2" CRLF , "P-My-Test", true) == "myval" );
      fct_chk( getHeader("P-My-Test: myval" CRLF "P-My-Test: myval2" CRLF , "P-My-Test", false) == "myval, myval2" );
      fct_chk( getHeader("P-My-Test: myval" CRLF "P-My-Otherheader: myval2" CRLF "P-My-Test: myval2" CRLF , "P-My-Test", false) == "myval, myval2" );
    } FCT_TEST_END();

    FCT_TEST_BGN(getHeader_atend) {

      fct_chk(getHeader("P-My-Test: mykey=myval;myotherkey=myval" ,
			"P-My-Test", true) == "mykey=myval;myotherkey=myval");
      fct_chk(getHeader("P-My-Test: mykey=myval;myotherkey=myval\n" ,
			"P-My-Test", true) == "mykey=myval;myotherkey=myval");
      fct_chk(getHeader("P-My-Test: mykey=myval;myotherkey=myval\r" ,
			"P-My-Test", true) == "mykey=myval;myotherkey=myval");
      fct_chk(getHeader("P-My-Test: mykey=myval;myotherkey=myval\r\n" ,
			"P-My-Test", true) == "mykey=myval;myotherkey=myval");
      fct_chk(getHeader("P-My-Test: mykey=myval;myotherkey=myval\r\nP-anotherheader:xy" ,
			"P-My-Test", true) == "mykey=myval;myotherkey=myval");

    } FCT_TEST_END();

    FCT_TEST_BGN(getHeader_keyvalue) {
      fct_chk(get_header_keyvalue("mykey=myval;myotherkey=myotherval", "mykey") == "myval");
      fct_chk(get_header_keyvalue("mykey=myval1;myotherkey=myval", "myotherkey") == "myval");
      fct_chk(get_header_keyvalue("mykey=myval;myotherkey=", "myotherkey") == "");
      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval;myotherfunkykey=myval" CRLF, "P-My-Test", true), "mykey") == "myval" );

      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval;myotherfunkykey=myval", "P-My-Test", true), "mykey") == "myval" );

      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval;myotherfunkykey=myval;andsomemore", "P-My-Test", true), "mykey") == "myval" );

      fct_chk(get_header_keyvalue(getHeader("P-App-Param: product_id=1;productid=1;bla=blub ", "P-App-Param"), "product_id") == "1");
      fct_chk(get_header_keyvalue(getHeader("P-App-Param: product_id=11;productid=1;bla=blub ", "P-App-Param"), "product_id") == "11");
      fct_chk(get_header_keyvalue(getHeader("P-App-Param: ;u=user;d=domain", "P-App-Param"), "u") == "user");


      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval; myotherfunkykey=myval;andsomemore", "P-My-Test", true), "mykey") == "myval" );
      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval; myotherfunkykey= myval;andsomemore", "P-My-Test", true), "myotherfunkykey") == "myval" );
      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval; myotherfunkykey= \"myval\";andsomemore", "P-My-Test", true), "myotherfunkykey") == "myval" );

      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval; myotherfunkykey='myval';andsomemore", "P-My-Test", true), "myotherfunkykey") == "myval" );
      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval; myotherfunkykey= '';andsomemore", "P-My-Test", true), "myotherfunkykey") == "");

      fct_chk(get_header_keyvalue(getHeader("P-My-Test: mykey=myval; myotherfunkykey= 'test \\' escaped';andsomemore", "P-My-Test", true), "test \\' escaped") == "");
      fct_chk(get_header_keyvalue("u=sayer;d=iptel.org;p=abcdef", "u") == "sayer");
      fct_chk(get_header_keyvalue("u=sayer;d=iptel.org;p=abcdef", "d") == "iptel.org");

      fct_chk(get_header_keyvalue("prefixmykey=myprefixval;mykey=myval", "mykey") == "myval");
      fct_chk(get_header_keyvalue("some=other;prefixmykey=myprefixval;mykey=myval", "mykey") == "myval");
      fct_chk(get_header_keyvalue("some=\"mykey\";prefixmykey=myprefixval;mykey=myval", "mykey") == "myval");
      fct_chk(get_header_keyvalue("some=\"mykey=valx\";prefixmykey=myprefixval;mykey=myval", "mykey") == "myval");

   } FCT_TEST_END();


    FCT_TEST_BGN(getHeader_allow) {
      fct_chk(getHeader("Subject: Performance Test\n"
			"Allow: INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH", "Allow") == "INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH");
    } FCT_TEST_END();

    FCT_TEST_BGN(getHeader_allow_substr) {
      fct_chk(getHeader("Subject: Performance Test\n"
			"Allow-Events: telephone-event,refer\n"
			"Allow: INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH", "Allow") == "INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH");
    } FCT_TEST_END();

    FCT_TEST_BGN(getHeader_allow3) {
      fct_chk(getHeader("Subject: Performance Test\n"
			"Allow  : INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH", "Allow") == "INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH");
    } FCT_TEST_END();

    FCT_TEST_BGN(getHeader_allow4) {
      fct_chk(getHeader("Subject: Performance Test\n"
			"Allow-Events : telephone-event,refer\n"
			"Allow  : INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH", "Allow") == "INVITE,ACK,OPTIONS,CANCEL,BYE,UPDATE,PRACK,INFO,SUBSCRIBE,NOTIFY,REFER,MESSAGE,PUBLISH");
    } FCT_TEST_END();

    FCT_TEST_BGN(addOptionTag) {
      string hdrs =
	"Supported: timer" CRLF
	"Session-Expires: 110;refresher=uas" CRLF
	"Supported: timer" CRLF;

      string hdrs1;
      addOptionTag(hdrs1, "Supported", "blub");
      // DBG("hdrs1 '%s'\n", hdrs1.c_str());
      fct_chk(hdrs1.find("Supported: blub") != string::npos);

      hdrs1 = hdrs;
      addOptionTag(hdrs1, "Supported", "something");
      // DBG("hdrs1 '%s'\n", hdrs1.c_str());
      fct_chk(hdrs1.find("something") != string::npos);

      hdrs1 = hdrs;
      addOptionTag(hdrs1, "Supported", "timer");
      // DBG("hdrs1 '%s'\n", hdrs1.c_str());
      fct_chk(hdrs1 == hdrs);

    } FCT_TEST_END();

    FCT_TEST_BGN(removeOptionTag) {
      string hdrs =
	"Supported: timer" CRLF;

      string hdrs1 = hdrs;
      removeOptionTag(hdrs1, "Supported", "notexisting");
      // DBG("hdrs1 = '%s'\n", hdrs1.c_str());
      fct_chk(hdrs1 == hdrs); // dont touch

      hdrs1 = hdrs;
      removeOptionTag(hdrs1, "Supported", "timer");
      // DBG("hdrs1 = '%s'\n", hdrs1.c_str());
      fct_chk(hdrs1.empty() == true); // last one

      hdrs1 = hdrs + "Supported: timer" CRLF;
      removeOptionTag(hdrs1, "Supported", "timer");
      // DBG("hdrs1 = '%s'\n", hdrs1.c_str());
      fct_chk(hdrs1.empty()== true); // last one

    } FCT_TEST_END();
} FCTMF_SUITE_END();
