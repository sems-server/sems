#include "AmSipEvent.h"
#include "AmSipDialog.h"

void AmSipTimeoutEvent::operator() (AmSipDialog* dlg)
{
    assert(dlg);
    dlg->uasTimeout(this);
}


#if 0
  AmSipReqTimeoutEvent *req_tout_ev = 
    dynamic_cast<AmSipReqTimeoutEvent *>(sip_ev);
  if (req_tout_ev) {
    CALL_EVENT_H(onSipReqTimeout, req_tout_ev->req);
    onSipReqTimeout(req_tout_ev->req);
    return;
  }

  AmSipRplTimeoutEvent *rpl_tout_ev = 
    dynamic_cast<AmSipRplTimeoutEvent *>(sip_ev);
  if (rpl_tout_ev) {
    CALL_EVENT_H(onSipRplTimeout, rpl_tout_ev->req, rpl_tout_ev->rpl);
    onSipRplTimeout(rpl_tout_ev->req, rpl_tout_ev->rpl);
    return;
  }
#endif


void AmSipRequestEvent::operator() (AmSipDialog* dlg)
{
    assert(dlg);
    dlg->updateStatus(req);
}

void AmSipReplyEvent::operator() (AmSipDialog* dlg)
{
    assert(dlg);
    dlg->updateStatus(reply);
}
