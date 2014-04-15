#include "fct.h"

#include "log.h"

#include "AmSipHeaders.h"
#include "AmSipMsg.h"
#include "AmUtils.h"
#include "AmUriParser.h"

#include "../../apps/sbc/ReplacesMapper.h"
#include "../../apps/sbc/SBCCallRegistry.h"


FCTMF_SUITE_BGN(test_replaces) {

    FCT_TEST_BGN(registry_simple) {
      SBCCallRegistryEntry e = SBCCallRegistryEntry("callid2", "ltag2", "rtag2");
      SBCCallRegistry::addCall("ltag", e);
      fct_chk(SBCCallRegistry::lookupCall("ltag",e));
      fct_chk(!SBCCallRegistry::lookupCall("ltag3",e));
      SBCCallRegistry::removeCall("ltag");
    } FCT_TEST_END();

    FCT_TEST_BGN(replaces_fixup_invite) {
      SBCCallRegistryEntry e = SBCCallRegistryEntry("C2", "C2f", "C2t");
      SBCCallRegistry::addCall("Ct", e);
      SBCCallRegistryEntry e2 = SBCCallRegistryEntry("C", "Ct", "Cf");
      SBCCallRegistry::addCall("C2f", e2);

      AmSipRequest r;
      r.hdrs="Replaces: C;from-tag=Cf;to-tag=Ct\r\n";
      fixReplaces(r.hdrs, true);
      DBG("r.hdrs='%s'\n", r.hdrs.c_str());
      fct_chk(r.hdrs=="Replaces: C2;from-tag=C2f;to-tag=C2t\r\n");

      SBCCallRegistry::removeCall("Ct");
      SBCCallRegistry::removeCall("C2f");
    } FCT_TEST_END();

    FCT_TEST_BGN(replaces_fixup_refer) {
      SBCCallRegistryEntry e = SBCCallRegistryEntry("C2", "C2f", "C2t");
      SBCCallRegistry::addCall("Ct", e);
      SBCCallRegistryEntry e2 = SBCCallRegistryEntry("C", "Ct", "Cf");
      SBCCallRegistry::addCall("C2f", e2);

      AmSipRequest r;
      string orig_str = "Refer-To: \"Mr. Watson\" <sip:watson@bell-telephone.com?Replaces=C%3Bto-tag%3DCt%3Bfrom-tag%3DCf>;q=0.1";
      string new_str = "Refer-To: \"Mr. Watson\" <sip:watson@bell-telephone.com?Replaces=C2%3Bfrom-tag%3DC2f%3Bto-tag%3DC2t>;q=0.1\r\n";

      r.hdrs=orig_str+"\r\n";
      fixReplaces(r.hdrs, false);
      DBG("r.hdrs='%s'\n", r.hdrs.c_str());
      DBG("new  s='%s'\n", new_str.c_str());

      fct_chk(r.hdrs==new_str);

      SBCCallRegistry::removeCall("Ct");
      SBCCallRegistry::removeCall("C2f");
    } FCT_TEST_END();

    FCT_TEST_BGN(replaces_fixup_refer2) {
      SBCCallRegistryEntry e = SBCCallRegistryEntry("C2", "C2f", "C2t");
      SBCCallRegistry::addCall("Ct", e);
      SBCCallRegistryEntry e2 = SBCCallRegistryEntry("C", "Ct", "Cf");
      SBCCallRegistry::addCall("C2f", e2);

      AmSipRequest r;
      string orig_str = "Refer-To: \"Mr. Watson\" <sip:watson@bell-telephone.com?Require=replaces;Replaces=C%3Bto-tag%3DCt%3Bfrom-tag%3DCf>;q=0.1\r\n";
      string new_str  = "Refer-To: \"Mr. Watson\" <sip:watson@bell-telephone.com?Require=replaces;Replaces=C2%3Bfrom-tag%3DC2f%3Bto-tag%3DC2t>;q=0.1\r\n";

      r.hdrs=orig_str;
      fixReplaces(r.hdrs, false);
      DBG("r.hdrs='%s'\n", r.hdrs.c_str());
      DBG("new  s='%s'\n", new_str.c_str());

      fct_chk(r.hdrs==new_str);

      SBCCallRegistry::removeCall("Ct");
      SBCCallRegistry::removeCall("C2f");
    } FCT_TEST_END();

    FCT_TEST_BGN(replaces_fixup_refer3) {
      SBCCallRegistryEntry e = SBCCallRegistryEntry("C2", "C2f", "C2t");
      SBCCallRegistry::addCall("Ct", e);
      SBCCallRegistryEntry e2 = SBCCallRegistryEntry("C", "Ct", "Cf");
      SBCCallRegistry::addCall("C2f", e2);

      AmSipRequest r;
      string orig_str = "Refer-To: \"Mr. Watson\" <sip:watson@bell-telephone.com?Require=replaces;Replaces=C%3Bto-tag%3DCt%3Bfrom-tag%3DCf;Bla=Blub>;q=0.1\r\n";
      string new_str  = "Refer-To: \"Mr. Watson\" <sip:watson@bell-telephone.com?Require=replaces;Replaces=C2%3Bfrom-tag%3DC2f%3Bto-tag%3DC2t;Bla=Blub>;q=0.1\r\n";

      r.hdrs=orig_str;
      fixReplaces(r.hdrs, false);
      DBG("r.hdrs='%s'\n", r.hdrs.c_str());
      DBG("new  s='%s'\n", new_str.c_str());

      fct_chk(r.hdrs==new_str);

      SBCCallRegistry::removeCall("Ct");
      SBCCallRegistry::removeCall("C2f");
    } FCT_TEST_END();

} FCTMF_SUITE_END();
