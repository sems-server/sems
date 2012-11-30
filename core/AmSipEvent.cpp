#include "AmSipEvent.h"
#include "AmSipDialog.h"

void AmSipTimeoutEvent::operator() (AmBasicSipDialog* dlg)
{
    assert(dlg);
    AmSipDialog* sip_dlg = dynamic_cast<AmSipDialog*>(dlg);
    if(!sip_dlg){
      ERROR("Wrong dialog class\n");
      return;
    }
    sip_dlg->uasTimeout(this);
}

void AmSipRequestEvent::operator() (AmBasicSipDialog* dlg)
{
    assert(dlg);
    dlg->onRxRequest(req);
}

void AmSipReplyEvent::operator() (AmBasicSipDialog* dlg)
{
    assert(dlg);
    dlg->onRxReply(reply);
}
