#ifndef _Am100rel_h_
#define _Am100rel_h_

#include "AmSipMsg.h"

class AmSipDialog;
class AmSipDialogEventHandler;

class Am100rel
{

public:
  /** enable the reliability of provisional replies? */
  enum State {
    REL100_DISABLED=0,
    REL100_SUPPORTED,
    REL100_REQUIRE,
    //REL100_PREFERED, //TODO
    REL100_IGNORED,
    REL100_MAX
  };
  
private:
  /* The 100rel state is per role. onRequestIn()/onReplyIn() promote
     REL100_SUPPORTED to REL100_REQUIRE when the peer asks for it, and that
     decision only binds the role it was taken for: a UAS that requires
     reliable 1xx from us says nothing about what we may require from the
     peer's own requests, and vice versa. One shared field let either
     promotion leak into the other direction. */
  // governs the replies we send (requests arriving at us)
  State uas_state;
  // governs the requests we send (replies arriving at us)
  State uac_state;

  // UAS
  unsigned rseq;          // RSeq for next request
  bool rseq_confirmed;    // latest RSeq is confirmed
  unsigned rseq_1st;      // value of first RSeq (init value)
  // UAC
  unsigned rseq_last;     // last accepted RSeq

  AmSipDialog* dlg;
  AmSipDialogEventHandler* hdl;
  
public:
  Am100rel(AmSipDialog* dlg, AmSipDialogEventHandler* hdl);

  void setState(State s) { uas_state = uac_state = s; }
  State getUasState() { return uas_state; }
  State getUacState() { return uac_state; }

  int  onRequestIn(const AmSipRequest& req);
  int  onReplyIn(const AmSipReply& reply);
  void onRequestOut(AmSipRequest& req);
  void onReplyOut(AmSipReply& reply);

  void onTimeout(const AmSipRequest& req, const AmSipReply& rpl);
};

#endif
