#include "fct.h"

#include "log.h"

#include "Am100rel.h"
#include "AmSipDialog.h"
#include "AmSipHeaders.h"
#include "AmUtils.h"

// Am100rel promotes REL100_SUPPORTED to REL100_REQUIRE when the peer asks for
// reliable provisional replies. That decision only binds the role it was taken
// for:
//
//   onRequestIn()  - the peer's INVITE wants OUR 1xx to be reliable (UAS)
//   onReplyIn()    - the peer's 1xx to OUR INVITE is reliable (UAC)
//
// Both wrote the same `reliable_1xx` field, so either promotion leaked into
// the other direction for the rest of the dialog: a reliable 1xx from a peer
// we called made us start requiring 100rel on the replies and requests we
// send, and a peer that advertised 100rel on its own INVITE made us require it
// of them. A dialog configured as "supported" ended up behaving as "require",
// rejecting in-dialog re-INVITEs with 421 Extension Required and stamping
// Require: 100rel / RSeq onto our own messages.

namespace {

// Drives Am100rel the way AmSipDialog's reply/request paths do, without a
// transaction layer behind it.
class ProbeDialog : public AmSipDialog {
public:
  ProbeDialog() : AmSipDialog() {}

  int replyIn(const AmSipReply &r) { return rel100.onReplyIn(r); }
  int requestIn(const AmSipRequest &r) { return rel100.onRequestIn(r); }
  void replyOut(AmSipReply &r) { rel100.onReplyOut(r); }
  void requestOut(AmSipRequest &r) { rel100.onRequestOut(r); }
};

// a reliable 183 from the peer, to an INVITE we sent
AmSipReply peer_reliable_183() {
  AmSipReply reply;
  reply.code = 183;
  reply.reason = "Session Progress";
  reply.cseq = 1;
  reply.cseq_method = SIP_METH_INVITE;
  reply.rseq = 1;
  reply.hdrs = SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
  return reply;
}

AmSipRequest peer_invite(const char *hdrs) {
  AmSipRequest req;
  req.method = SIP_METH_INVITE;
  req.cseq = 1;
  req.hdrs = hdrs;
  return req;
}

// a provisional reply of ours, before Am100rel gets to it
AmSipReply our_1xx(int code) {
  AmSipReply reply;
  reply.code = code;
  reply.cseq = 1;
  reply.cseq_method = SIP_METH_INVITE;
  return reply;
}

bool advertises(const string &hdrs, const char *hf) {
  return key_in_list(getHeader(hdrs, hf), SIP_EXT_100REL);
}

} // namespace

FCTMF_SUITE_BGN(test_rel100_state) {

  // The peer requiring reliable 1xx on an INVITE we sent says nothing about
  // what we may require of the peer's own requests.
  FCT_TEST_BGN(uac_promotion_does_not_reach_the_replies_we_send) {
    ProbeDialog dlg;
    dlg.setStatus(AmSipDialog::Early);

    dlg.replyIn(peer_reliable_183());

    AmSipReply out = our_1xx(180);
    dlg.replyOut(out);

    fct_chk(advertises(out.hdrs, SIP_HDR_SUPPORTED));
    fct_chk(!advertises(out.hdrs, SIP_HDR_REQUIRE));
    fct_chk(getHeader(out.hdrs, SIP_HDR_RSEQ).empty());
  }
  FCT_TEST_END();

  // The other direction: a peer advertising 100rel on its own INVITE must not
  // make us require it of them on ours.
  FCT_TEST_BGN(uas_promotion_does_not_reach_the_requests_we_send) {
    ProbeDialog dlg;

    fct_chk(dlg.requestIn(peer_invite(SIP_HDR_COLSP(SIP_HDR_SUPPORTED) SIP_EXT_100REL CRLF)) == 1);

    AmSipRequest out;
    out.method = SIP_METH_INVITE;
    dlg.requestOut(out);

    fct_chk(advertises(out.hdrs, SIP_HDR_SUPPORTED));
    fct_chk(!advertises(out.hdrs, SIP_HDR_REQUIRE));
  }
  FCT_TEST_END();

  // The promotion still has to work inside its own role: a peer that requires
  // 100rel on its INVITE gets reliable provisional replies from us.
  FCT_TEST_BGN(uas_promotion_still_applies_to_the_replies_we_send) {
    ProbeDialog dlg;

    dlg.requestIn(peer_invite(SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF));

    AmSipReply out = our_1xx(183);
    dlg.replyOut(out);

    fct_chk(advertises(out.hdrs, SIP_HDR_REQUIRE));
    fct_chk(!getHeader(out.hdrs, SIP_HDR_RSEQ).empty());
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
