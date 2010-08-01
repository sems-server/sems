#include "AmSipEvent.h"
#include "AmSipDialog.h"

void AmSipTimeoutEvent::operator() (AmSipDialog* dlg)
{
    assert(dlg);
    dlg->uasTimeout(this);
}

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
