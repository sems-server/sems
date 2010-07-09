#include "fct.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "jsonArg.h"


FCTMF_SUITE_BGN(test_jsonarg) {

    FCT_TEST_BGN(json_parse_escaped) {
      string s = "{\"jsonrpc\": \"2.0\", \"id\": \"11\", \"error\": {\"message\": \"(1062, \\\"Duplicate entry '5447' for key 'PRIMARY'\\\")\", \"code\": -32603}},";
      // DBG("s='%s'\n",s.c_str());
		
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
    } FCT_TEST_END();

    FCT_TEST_BGN(json_empty_struct_parse) {
      // string s = "{\"jsonrpc\": \"2.0\", \"result\": {\"timestamp\": 62, \"analysis\": {}}, \"id\": \"11\"}";
      string s = "{\"result\": {}}";
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      fct_chk(isArgStruct(rpc_params["result"]));
    } FCT_TEST_END();

    FCT_TEST_BGN(json_empty_array_parse) {
      string s = "{\"result\": []}";
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      fct_chk(isArgArray(rpc_params["result"]));
    } FCT_TEST_END();

    FCT_TEST_BGN(json_error_array_parse) {
      string s = "{\"result\": [ :1]}";
      AmArg rpc_params;
      fct_chk(!json2arg(s.c_str(), rpc_params));
    } FCT_TEST_END();

    FCT_TEST_BGN(json_error_object_parse) {
      string s = "{\"result\": { :1}}";
      AmArg rpc_params;
      fct_chk(!json2arg(s.c_str(), rpc_params));
    } FCT_TEST_END();

    FCT_TEST_BGN(json_empty_string_key_parse) {
      string s = "{\"result\": {\"\" :1}}";
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      fct_chk(rpc_params["result"][""].asInt()==1);
    } FCT_TEST_END();

} FCTMF_SUITE_END();
