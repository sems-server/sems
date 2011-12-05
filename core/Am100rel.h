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
  State reliable_1xx;

  unsigned rseq;          // RSeq for next request
  bool rseq_confirmed;    // latest RSeq is confirmed
  unsigned rseq_1st;      // value of first RSeq (init value)

  AmSipDialog* dlg;
  AmSipDialogEventHandler* hdl;
  
public:
  Am100rel(AmSipDialog* dlg, AmSipDialogEventHandler* hdl);

  void setState(State s) { reliable_1xx = s; }
  State getState() { return reliable_1xx; }

  int  onRequestIn(const AmSipRequest& req);
  int  onReplyIn(const AmSipReply& reply);
  void onRequestOut(AmSipRequest& req);
  void onReplyOut(AmSipReply& reply);

  void onTimeout(const AmSipRequest& req, const AmSipReply& rpl);
};

#endif
