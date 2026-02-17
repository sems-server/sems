#include "fct.h"

#include "log.h"

#include "sip/parse_extensions.h"

#include <string>
using std::string;

FCTMF_SUITE_BGN(test_extensions) {

    FCT_TEST_BGN(register_and_check_supported) {
        register_supported_extension("100rel");
        fct_chk(is_extension_supported("100rel") == true);
    } FCT_TEST_END();

    FCT_TEST_BGN(check_unsupported) {
        fct_chk(is_extension_supported("unknown-ext") == false);
    } FCT_TEST_END();

    FCT_TEST_BGN(get_unsupported_all_known) {
        register_supported_extension("replaces");
        register_supported_extension("timer");
        const char* val = "100rel, replaces, timer";
        string result = get_unsupported_extensions(val, strlen(val));
        fct_chk(result.empty());
    } FCT_TEST_END();

    FCT_TEST_BGN(get_unsupported_all_unknown) {
        const char* val = "foo, bar";
        string result = get_unsupported_extensions(val, strlen(val));
        fct_chk(result.find("foo") != string::npos);
        fct_chk(result.find("bar") != string::npos);
    } FCT_TEST_END();

    FCT_TEST_BGN(get_unsupported_mixed) {
        // 100rel was registered above
        const char* val = "100rel, exotic-ext";
        string result = get_unsupported_extensions(val, strlen(val));
        fct_chk(result.find("100rel") == string::npos);
        fct_chk(result.find("exotic-ext") != string::npos);
    } FCT_TEST_END();

    FCT_TEST_BGN(get_unsupported_single_unsupported) {
        const char* val = "never-heard-of";
        string result = get_unsupported_extensions(val, strlen(val));
        fct_chk(result == "never-heard-of");
    } FCT_TEST_END();

    FCT_TEST_BGN(get_unsupported_whitespace_handling) {
        const char* val = "  100rel ,  unknown-x  ";
        string result = get_unsupported_extensions(val, strlen(val));
        fct_chk(result.find("100rel") == string::npos);
        fct_chk(result.find("unknown-x") != string::npos);
    } FCT_TEST_END();

    FCT_TEST_BGN(get_unsupported_empty_value) {
        const char* val = "";
        string result = get_unsupported_extensions(val, 0);
        fct_chk(result.empty());
    } FCT_TEST_END();

} FCTMF_SUITE_END();
