
#include "log.h"

#include <string>
#include <vector>

#include <map>

#include <string.h>

#include "AmConfig.h"

#include "fct.h"      /* FCTX is installed! */
  
#include "AmSipMsg.h"
#include "AmSipHeaders.h"

#include "fct.h"

FCT_BGN() {
  init_logging();
  log_stderr=true;
  log_level=3;

  FCTMF_SUITE_CALL(test_sdp);
  FCTMF_SUITE_CALL(test_auth);
  FCTMF_SUITE_CALL(test_headers);
  FCTMF_SUITE_CALL(test_uriparser);
  FCTMF_SUITE_CALL(test_jsonarg);
  FCTMF_SUITE_CALL(test_replaces);
} FCT_END();


// FCT_BGN() {
// FCT_FIXTURE_SUITE_BGN(example_suite) {
 
// FCT_SETUP_BGN() {
// } FCT_SETUP_END();
 
// FCT_TEARDOWN_BGN() {
// } FCT_TEARDOWN_END();

// FCT_TEST_BGN(test_object_basic) {
// } FCT_TEST_END();
 
// } FCT_FIXTURE_SUITE_END();
 
// } FCT_END();
