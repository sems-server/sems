#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "AmUtils.h"
#include "plug-in/uac_auth/UACAuth.h"

FCTMF_SUITE_BGN(test_auth) {

    FCT_TEST_BGN(nonce_gen) {

      string secret = "1234secret";
      string nonce = UACAuth::calcNonce(secret);
      //      DBG("nonce '%s'\n", nonce.c_str());    
      fct_chk( UACAuth::checkNonce(nonce, secret));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_secret) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce(secret);
      fct_chk( !UACAuth::checkNonce(nonce, secret+"asd"));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_nonce) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce(secret);
      nonce[0]=0;
      nonce[1]=0;
      fct_chk( !UACAuth::checkNonce(nonce, secret));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_nonce) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce(secret);
      nonce+="hallo";
      fct_chk( !UACAuth::checkNonce(nonce, secret));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_nonce2) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce(secret);
      nonce[nonce.size()-1]=nonce[nonce.size()-2];
      fct_chk( !UACAuth::checkNonce(nonce, secret));
    } FCT_TEST_END();

} FCTMF_SUITE_END();
