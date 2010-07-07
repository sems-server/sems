#include "fct.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "jsonArg.h"


FCTMF_SUITE_BGN(test_jsonarg) {

    FCT_TEST_BGN(json_parse_escaped) {
      string s = "{\"jsonrpc\": \"2.0\", \"id\": \"11\", \"error\": {\"message\": \"(1062, \\\"Duplicate entry '5447' for key 'PRIMARY'\\\")\", \"code\": -32603}},";
      DBG("s='%s'\n",s.c_str());
		
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
    } FCT_TEST_END();

} FCTMF_SUITE_END();
