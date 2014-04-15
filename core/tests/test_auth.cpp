#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "AmUtils.h"
#include "plug-in/uac_auth/UACAuth.h"

FCTMF_SUITE_BGN(test_auth) {

    FCT_TEST_BGN(nonce_gen) {

      string secret = "1234secret";
      string nonce = UACAuth::calcNonce();
      //      DBG("nonce '%s'\n", nonce.c_str());    
      fct_chk( UACAuth::checkNonce(nonce));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_secret) {
      string secret = "1234secret";
      UACAuth::setServerSecret(secret);
      string nonce = UACAuth::calcNonce();

      UACAuth::setServerSecret(secret+"asd");
      fct_chk( !UACAuth::checkNonce(nonce));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_nonce) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce();
      nonce[0]=0;
      nonce[1]=0;
      fct_chk( !UACAuth::checkNonce(nonce));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_nonce) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce();
      nonce+="hallo";
      fct_chk( !UACAuth::checkNonce(nonce));
    } FCT_TEST_END();

    FCT_TEST_BGN(nonce_wrong_nonce2) {
      string secret = "1234secret";
      string nonce = UACAuth::calcNonce();
      nonce[nonce.size()-1]=nonce[nonce.size()-2];
      fct_chk( !UACAuth::checkNonce(nonce));
    } FCT_TEST_END();

    FCT_TEST_BGN(t_cmp_len) {
      string s1 = "1234secret";
      string s2 = "1234s3ecret";
      fct_chk( !UACAuth::tc_isequal(s1,s2) );
    } FCT_TEST_END();

    FCT_TEST_BGN(t_cmp_eq) {
      string s1 = "1234secret";
      string s2 = "1234secret";
      fct_chk( UACAuth::tc_isequal(s1,s2) );
    } FCT_TEST_END();


    FCT_TEST_BGN(t_cmp_empty) {
      fct_chk( UACAuth::tc_isequal("","") );
    } FCT_TEST_END();

    FCT_TEST_BGN(t_cmp_uneq) {
      fct_chk( !UACAuth::tc_isequal("1234secret","2134secret") );
    } FCT_TEST_END();

    FCT_TEST_BGN(t_cmp_uneq_chr) {
      fct_chk( !UACAuth::tc_isequal("1234secret","2134secret", 10) );
    } FCT_TEST_END();

    FCT_TEST_BGN(t_cmp_eq_charptr) {
      fct_chk( UACAuth::tc_isequal("1234secret","1234secret", 10) );
    } FCT_TEST_END();


} FCTMF_SUITE_END();
