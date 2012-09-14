#ifndef __EXTENDED_CC_INTERFACE
#define __EXTENDED_CC_INTERFACE

#include "CallLeg.h"
#include "sbc_events.h"

class SBCCallLeg;
class SBCCallProfile;

struct InitialInviteHandlerParams
{
  string remote_party;
  string remote_uri;
  string from;
  const AmSipRequest *original_invite;
  AmSipRequest *modified_invite;

  InitialInviteHandlerParams(const string &to, const string &ruri, const string &_from,
      const AmSipRequest *original, AmSipRequest *modified):
      remote_party(to), remote_uri(ruri), from(_from),
      original_invite(original), modified_invite(modified) { }
};

enum CCChainProcessing { ContinueProcessing, StopProcessing };

class ExtendedCCInterface
{
  protected:
    ~ExtendedCCInterface() { }

  public:
    // call state changes

    /** First method called from extended CC module interface.
     * It should initialize CC module internals (initial INVITE can be used for
     * evaluating CC module parameters. */
    virtual void init(SBCCallLeg *call, const AmSipRequest &invite) { }

    virtual void onStateChange(SBCCallLeg *call) { };

    virtual void onTerminateLeg(SBCCallLeg *call) { }

    /** called when the call leg is being destroyed, useful to cleanup used resources */
    virtual void onDestroyLeg(SBCCallLeg *call) { }

    // dialog state changes
    // TODO

    // SIP messages

    /** called from A-leg onInvite
     *
     * can be used for forking or handling INVITE with Replaces header */
    virtual CCChainProcessing onInitialInvite(SBCCallLeg *call, InitialInviteHandlerParams &params) { return ContinueProcessing; }

    /** called from A/B leg when in-dialog request comes in */
    virtual CCChainProcessing onInDialogRequest(SBCCallLeg *call, const AmSipRequest &req) { return ContinueProcessing; }

    virtual CCChainProcessing onInDialogReply(SBCCallLeg *call, const AmSipReply &reply) { return ContinueProcessing; }

    /** called before any other processing for the event is done */
    virtual CCChainProcessing onEvent(SBCCallLeg *call, AmEvent *e) { return ContinueProcessing; }
};

#endif
