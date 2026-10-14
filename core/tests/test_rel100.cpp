#include "fct.h"

#include "log.h"

#include "Am100rel.h"
#include "AmSession.h"
#include "AmSipDialog.h"
#include "AmSipHeaders.h"

// A caller may build the SIP dialog itself and hand it to AmSession instead of
// letting AmSession create one. apps/sbc does exactly that, with no event
// handler at all:
//
//   return new SBCCallLeg(call_profile, new AmSipDialog());
//
// AmSipDialog's constructor passes its handler argument straight on to
// Am100rel, so that dialog's Am100rel::hdl was NULL. AmSession's constructor
// only called dlg->setEventhandler(this), which does not reach Am100rel, so
// the handler stayed NULL for the dialog's whole life. Every reliable-1xx
// callback in Am100rel sits behind an `if (hdl)` guard, so they were all
// skipped: onInvite1xxRel() never fired and no PRACK was ever sent for a
// reliable provisional reply on such a leg.

namespace {

// Lets a test hand a reply to Am100rel the way AmSipDialog::onRxReplyStatus()
// does, without a transaction layer behind it.
class ProbeDialog : public AmSipDialog {
public:
  explicit ProbeDialog(AmSipDialogEventHandler *h = NULL) : AmSipDialog(h) {}

  int feedReply(const AmSipReply &reply) { return rel100.onReplyIn(reply); }
};

class Rel100Session : public AmSession {
public:
  int invite_1xx_rel_calls;

  explicit Rel100Session(AmSipDialog *d) : AmSession(d), invite_1xx_rel_calls(0) {}

  void onInvite1xxRel(const AmSipReply &) override { invite_1xx_rel_calls++; }
};

// 183 Session Progress carrying a reliable RSeq, as a UAS that requires
// 100rel sends it
AmSipReply reliable_183() {
  AmSipReply reply;
  reply.code = 183;
  reply.reason = "Session Progress";
  reply.cseq = 1;
  reply.cseq_method = SIP_METH_INVITE;
  reply.rseq = 1;
  reply.hdrs = SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
  return reply;
}

} // namespace

FCTMF_SUITE_BGN(test_rel100) {

  // The dialog comes from the caller with no handler at all, as in apps/sbc.
  FCT_TEST_BGN(rel100_handler_is_set_for_a_handlerless_dialog) {
    ProbeDialog *dlg = new ProbeDialog();
    Rel100Session sess(dlg); // adopts the dialog, owns it from here on

    dlg->setRel100State(Am100rel::REL100_REQUIRE);
    dlg->setStatus(AmSipDialog::Early);
    dlg->feedReply(reliable_183());

    fct_chk(sess.invite_1xx_rel_calls == 1);
  }
  FCT_TEST_END();

  // A dialog built pointing at one handler and then adopted by a session has
  // to dispatch to the session that owns it now, not to the old handler.
  FCT_TEST_BGN(rel100_handler_follows_the_session_that_owns_the_dialog) {
    Rel100Session stale(NULL);
    ProbeDialog *dlg = new ProbeDialog(&stale);
    Rel100Session sess(dlg); // adopts the dialog, owns it from here on

    dlg->setRel100State(Am100rel::REL100_REQUIRE);
    dlg->setStatus(AmSipDialog::Early);
    dlg->feedReply(reliable_183());

    fct_chk(sess.invite_1xx_rel_calls == 1);
    fct_chk(stale.invite_1xx_rel_calls == 0);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
