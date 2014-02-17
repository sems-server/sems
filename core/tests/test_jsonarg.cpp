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

    FCT_TEST_BGN(json_number_e_parse) {
      string s = "{\"result\": 0E1}";
      // DBG("s.c_str() %s\n", s.c_str() );
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      fct_chk(isArgInt(rpc_params["result"]));
      fct_chk(rpc_params["result"].asInt() == 0);
    } FCT_TEST_END();

    FCT_TEST_BGN(json_number_e_pow) {
      string s = "{\"result\": 1E1}";
      // DBG("s.c_str() %s\n", s.c_str() );
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      fct_chk(isArgInt(rpc_params["result"]) && rpc_params["result"].asInt() == 10);
    } FCT_TEST_END();

    FCT_TEST_BGN(json_number_e_pow2) {
      string s = "{\"result\": 5e0}";
      // DBG("s.c_str() %s\n", s.c_str() );
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      fct_chk(isArgInt(rpc_params["result"]) && rpc_params["result"].asInt() == 5);
    } FCT_TEST_END();

    FCT_TEST_BGN(json_number_e_wrong) {
      string s = "{\"result\": 1E}";
      // DBG("s.c_str() %s\n", s.c_str() );
      AmArg rpc_params;
      fct_chk(!json2arg(s.c_str(), rpc_params));
    } FCT_TEST_END();

    FCT_TEST_BGN(json_number_e_powneg1) {
      string s = "{\"result\": 1E-1}";
      // DBG("s.c_str() %s\n", s.c_str() );
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      // DBG("res: %s, type %d\n",  AmArg::print(rpc_params["result"]).c_str(), rpc_params["result"].getType());

      fct_chk(isArgDouble(rpc_params["result"]));
      fct_chk(isArgDouble(rpc_params["result"]) && rpc_params["result"].asDouble() == 0.1);
    } FCT_TEST_END();

    FCT_TEST_BGN(json_number_float_parse) {
      string s = "{\"result\": 1.21}";
      // DBG("s.c_str() %s\n", s.c_str() );
      AmArg rpc_params;
      fct_chk(json2arg(s.c_str(), rpc_params));
      // DBG("res: %s, type %d\n",  AmArg::print(rpc_params["result"]).c_str(), rpc_params["result"].getType());

      fct_chk(isArgDouble(rpc_params["result"]));
      fct_chk(isArgDouble(rpc_params["result"]) && rpc_params["result"].asDouble() == 1.21);
    } FCT_TEST_END();

    FCT_TEST_BGN(json_tofro_equality) {
      AmArg a1;
      a1["test"]=1;
      a1["test2"].push("asdf");
      a1["test2"].push(1);

      string s = arg2json(a1);
      AmArg a2;
      bool back_conversion_result = json2arg(s, a2);
      fct_chk(back_conversion_result);
      // fct_chk(a1==a2);
      // DBG("a1 = '%s', a2 = '%s', \n", AmArg::print(a1).c_str(), AmArg::print(a2).c_str());
    } FCT_TEST_END();

} FCTMF_SUITE_END();
