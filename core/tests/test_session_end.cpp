#include "fct.h"

#include "log.h"

#include "AmSession.h"
#include "AmSipDialog.h"
#include "AmSipHeaders.h"
#include "AmSipMsg.h"

#include <string>
#include <vector>

// Once an application has stopped its session, AmSession::processingCycle()
// sends a BYE on its behalf if the dialog is still up, and keeps the session
// until the dialog is Disconnected. AmSipDialog::bye() does not always leave
// a transaction behind whose outcome gets it there: before the call is
// answered it replies 487 to the pending INVITE, whose ACK the transaction
// layer absorbs, and with nothing to cancel or to reply to it sets
// Disconnected itself. Nothing wakes the session up after that, so
// processingCycle() has to end it right away; with a BYE in flight it has to
// keep waiting.
//
// The dialog below does the bookkeeping of AmBasicSipDialog::reply() and
// sendRequest() short of handing the message to the transaction layer, so
// the real bye() runs against it. The ERROR bye() logs when it finds nothing
// to cancel or to reply to is expected here.

namespace {

class TestDialog : public AmSipDialog {
public:
  std::vector<unsigned int> reply_codes;
  std::vector<std::string> requests;

  ~TestDialog() override {
    // nothing was registered with the transaction layer, so there is nothing
    // for ~AmBasicSipDialog() to terminate
    uas_trans.clear();
    uac_trans.clear();
  }

  int reply(const AmSipRequest &req, unsigned int code, const std::string &reason,
            const AmMimeBody *body = NULL, const std::string &hdrs = "", int flags = 0) override {
    if (uas_trans.find(req.cseq) == uas_trans.end()) {
      return -1;
    }
    AmSipReply reply;
    reply.code = code;
    reply.reason = reason;
    reply.hdrs = hdrs;
    reply.cseq = req.cseq;
    reply.cseq_method = req.method;
    if (body) {
      reply.body = *body;
    }
    if (onTxReply(req, reply, flags)) {
      return -1;
    }
    reply_codes.push_back(code);
    onReplyTxed(req, reply);
    return 0;
  }

  int sendRequest(const std::string &method, const AmMimeBody *body = NULL, const std::string &hdrs = "",
                  int flags = 0, int /* max_forwards */ = -1) override {
    AmSipRequest req;
    req.method = method;
    req.cseq = cseq;
    req.callid = callid;
    req.hdrs = hdrs;
    if (body) {
      req.body = *body;
    }
    if (onTxRequest(req, flags) < 0) {
      return -1;
    }
    requests.push_back(method);
    onRequestTxed(req);
    return 0;
  }
};

class TestSession : public AmSession {
public:
  // the provisional reply onInvite() answers with, 0 for none
  unsigned int provisional;

  explicit TestSession(unsigned int provisional = 180)
      : AmSession(new TestDialog()), provisional(provisional) {}

  TestDialog *dialog() { return static_cast<TestDialog *>(dlg); }

  using AmSession::processingCycle;

  void onInvite(const AmSipRequest &req) override {
    if (provisional) {
      dlg->reply(req, provisional, "Ringing");
    }
  }
};

// an INVITE the way the transaction layer hands it to the dialog
AmSipRequest invite() {
  AmSipRequest req;
  req.method = SIP_METH_INVITE;
  req.r_uri = "sip:bob@example.com";
  req.from = "<sip:alice@example.com>;tag=alice";
  req.from_tag = "alice";
  req.from_uri = "sip:alice@example.com";
  req.to = "<sip:bob@example.com>";
  req.callid = "call@example.com";
  req.cseq = 1;
  return req;
}

const char *status_str(int status) {
  return AmBasicSipDialog::getStatusStr((AmBasicSipDialog::Status)status);
}

} // namespace

FCTMF_SUITE_BGN(test_session_end) {

  // bye() answers the pending INVITE with 487, and the ACK to that never
  // reaches the session: it has to end right away
  FCT_TEST_BGN(stopped_before_answer_ends_after_487) {
    const unsigned int provisionals[] = {0, 180}; // Trying, Early
    for (int i = 0; i < 2; i++) {
      TestSession s(provisionals[i]);
      s.dialog()->onRxRequest(invite());
      int status = s.dialog()->getStatus();
      fct_xchk(status == (provisionals[i] ? AmSipDialog::Early : AmSipDialog::Trying), "dialog is %s",
               status_str(status));

      s.setStopped();
      bool keep_running = s.processingCycle();

      std::vector<unsigned int> expected;
      if (provisionals[i]) {
        expected.push_back(provisionals[i]);
      }
      expected.push_back(487);
      fct_xchk(!keep_running, "%s: the session kept waiting", status_str(status));
      fct_xchk(s.dialog()->reply_codes == expected, "%s: %d replies sent, the last one %d",
               status_str(status), (int)s.dialog()->reply_codes.size(),
               s.dialog()->reply_codes.empty() ? -1 : (int)s.dialog()->reply_codes.back());
      fct_chk_eq_int(s.dialog()->getStatus(), AmSipDialog::Disconnected);
      fct_chk(s.dialog()->requests.empty());
    }
  }
  FCT_TEST_END();

  // with neither an INVITE to cancel nor one to reply to, bye() sets
  // Disconnected itself
  FCT_TEST_BGN(stopped_with_nothing_to_end_ends_at_once) {
    const AmSipDialog::Status statuses[] = {AmSipDialog::Trying, AmSipDialog::Proceeding, AmSipDialog::Early};
    for (int i = 0; i < 3; i++) {
      TestSession s;
      s.dialog()->setStatus(statuses[i]);
      s.setStopped();
      bool keep_running = s.processingCycle();
      fct_xchk(!keep_running, "%s: the session kept waiting", status_str(statuses[i]));
      fct_chk_eq_int(s.dialog()->getStatus(), AmSipDialog::Disconnected);
      fct_chk(s.dialog()->reply_codes.empty());
      fct_chk(s.dialog()->requests.empty());
    }
  }
  FCT_TEST_END();

  // in a call, bye() sends a BYE: the session waits for its reply, and ends
  // once that reply has disconnected the dialog
  FCT_TEST_BGN(stopped_in_call_waits_for_the_bye_reply) {
    TestSession s;
    s.dialog()->setStatus(AmSipDialog::Connected);
    s.setStopped();

    fct_chk(s.processingCycle());
    fct_req(s.dialog()->requests.size() == 1);
    fct_chk(s.dialog()->requests[0] == SIP_METH_BYE);
    fct_chk_eq_int(s.dialog()->getStatus(), AmSipDialog::Disconnecting);

    AmSipReply ok;
    ok.code = 200;
    ok.reason = "OK";
    ok.cseq = s.dialog()->cseq - 1; // the BYE's
    ok.cseq_method = SIP_METH_BYE;
    s.dialog()->onRxReply(ok);

    fct_chk_eq_int(s.dialog()->getStatus(), AmSipDialog::Disconnected);
    fct_chk(!s.processingCycle());
  }
  FCT_TEST_END();

  // while its own INVITE is being cancelled, a stopped session does not call
  // bye() at all and waits for the outcome
  FCT_TEST_BGN(stopped_while_cancelling_waits) {
    TestSession s;
    s.dialog()->setStatus(AmSipDialog::Cancelling);
    s.setStopped();

    fct_chk(s.processingCycle());
    fct_chk_eq_int(s.dialog()->getStatus(), AmSipDialog::Cancelling);
    fct_chk(s.dialog()->requests.empty());
    fct_chk(s.dialog()->reply_codes.empty());
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
