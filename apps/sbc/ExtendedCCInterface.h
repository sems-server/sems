#ifndef __EXTENDED_CC_INTERFACE
#define __EXTENDED_CC_INTERFACE

#include "CallLeg.h"
#include "sbc_events.h"

class SBCCallLeg;
struct SBCCallProfile;
class SimpleRelayDialog;

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
     * It should initialize CC module internals (values from sbcprofile.conf can
     * be used for evaluating CC module parameters). */
    virtual bool init(SBCCallLeg *call, const map<string, string> &values) { return true; }

    virtual void onStateChange(SBCCallLeg *call, const CallLeg::StatusChangeCause &cause) { };

    /** called when the call leg is being destroyed, useful to cleanup used resources */
    virtual void onDestroyLeg(SBCCallLeg *call) { }

    /** one of existing B legs is refused,
     * handle redirect here or do serial fork or ... */
    virtual CCChainProcessing onBLegRefused(SBCCallLeg *call, const AmSipReply& reply) { return ContinueProcessing; }

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

    virtual CCChainProcessing onDtmf(SBCCallLeg *call, int event, int duration) { return ContinueProcessing;  };

    // hold related functionality (seems to work best being explicitly supported
    // with API than hacking based on another callbacks)

    virtual void holdRequested(SBCCallLeg *call) { }
    virtual void holdAccepted(SBCCallLeg *call) { }
    virtual void holdRejected(SBCCallLeg *call) { }
    virtual void resumeRequested(SBCCallLeg *call) { }
    virtual void resumeAccepted(SBCCallLeg *call) { }
    virtual void resumeRejected(SBCCallLeg *call) { }

    /** Possibility to influence messages relayed to the B2B peer leg.
      return value:
      - lower than 0 means error (returned upstream, the one
      returning error is responsible for destrying the event instance)
      - greater than 0 means "stop processing and return 0 upstream"
      - equal to 0 means "continue processing" */
    virtual int relayEvent(SBCCallLeg *call, AmEvent *e) { return 0; }

    // using extended CC modules with simple relay

    virtual bool init(SBCCallProfile &profile, SimpleRelayDialog *relay, void *&user_data) { return true; }
    virtual void initUAC(const AmSipRequest &req, void *user_data) { }
    virtual void initUAS(const AmSipRequest &req, void *user_data) { }
    virtual void finalize(void *user_data) { }
    virtual void onSipRequest(const AmSipRequest& req, void *user_data) { }
    virtual void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply,
		  AmBasicSipDialog::Status old_dlg_status,
                  void *user_data) { }
    virtual void onB2BRequest(const AmSipRequest& req, void *user_data) { }
    virtual void onB2BReply(const AmSipReply& reply, void *user_data) { }


};

#endif
