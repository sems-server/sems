#include "SessionUpdate.h"
#include "CallLeg.h"

void PutOnHold::apply(CallLeg *call)
{
  setCSeq(call->putOnHoldImpl());
}

///////////////////////////////////////////////////////////////////////////////////////

void ResumeHeld::apply(CallLeg *call)
{
  setCSeq(call->resumeHeldImpl());
}

///////////////////////////////////////////////////////////////////////////////////////

void Reinvite::apply(CallLeg *call)
{
  setCSeq(call->reinvite(hdrs, body, relayed_invite, r_cseq, establishing));
}

///////////////////////////////////////////////////////////////////////////////////////

void SessionUpdateTimer::fire()
{
  DBG("session update timer fired");
  has_started = false;
  AmSessionContainer::instance()->postEvent(ltag, new ApplyPendingUpdatesEvent());
}

void SessionUpdateTimer::start(const std::string &_ltag, double delay)
{
  has_started = true;
  ltag = _ltag; // not nice here, needed to find a place where the local tag is set finally
  AmAppTimer::instance()->setTimer(this, delay);
}

