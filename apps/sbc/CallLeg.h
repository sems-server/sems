#ifndef __AMB2BCALL_H
#define __AMB2BCALL_H

#include "AmB2BSession.h"
#include "AmSessionContainer.h"

// TODO: global event numbering
enum {
  ConnectLeg = B2BMsgBody + 16,
  ReconnectLeg,
  ReplaceLeg,
  ReplaceInProgress,
  DisconnectLeg
};

#define LAST_B2B_CALL_LEG_EVENT_ID DisconnectLeg

struct ConnectLegEvent: public B2BEvent
{
  AmMimeBody body;
  string hdrs;
  
  unsigned int r_cseq;

  ConnectLegEvent(const AmSipRequest &relayed_invite)
    : B2BEvent(ConnectLeg),
    body(relayed_invite.body),
    hdrs(relayed_invite.hdrs),
    r_cseq(relayed_invite.cseq)
  {}
};

/** B2B event which sends another event back if it was or was not processed.
 * (note that the back events need to be created in advance because we can not
 * use overriden virtual methods in destructor (which is the only place which
 * will be called for sure) */
struct ReliableB2BEvent: public B2BEvent
{
  private:
    bool processed;

    B2BEvent *unprocessed_reply; //< reply to be sent back if the original event was not processed
    B2BEvent *processed_reply; //< event sent back if the original event was processed
    string sender; // sender will be filled when sending the event out

  public:

    ReliableB2BEvent(int ev_id, B2BEvent *_processed, B2BEvent *_unprocessed):
      B2BEvent(ev_id), processed(false), processed_reply(_processed), unprocessed_reply(_unprocessed) { }
    void markAsProcessed() { processed = true; }
    void setSender(const string &tag) { sender = tag; }
    virtual ~ReliableB2BEvent();
};

struct ReconnectLegEvent: public ReliableB2BEvent
{
  AmMimeBody body;
  string hdrs;

  unsigned int r_cseq;

  AmB2BMedia *media; // avoid direct access to this
  string session_tag;

  ReconnectLegEvent(const string &tag, const AmSipRequest &relayed_invite, AmB2BMedia *m)
    : ReliableB2BEvent(ReconnectLeg, NULL, new B2BEvent(B2BTerminateLeg) /* TODO: choose a better one */),
    body(relayed_invite.body),
    hdrs(relayed_invite.hdrs),
    r_cseq(relayed_invite.cseq),
    media(m),
    session_tag(tag)
  { if (media) media->addReference(); setSender(tag); }

  virtual ~ReconnectLegEvent() { if (media && media->releaseReference()) delete media; }
};


/** Call leg receiving ReplaceLegEvent should replace itself with call leg from
 * the event parameters. (it terminates itself and forwards ReconnectLegEvent to
 * the call leg identified by other_id) */
struct ReplaceLegEvent: public ReliableB2BEvent
{
  private:
    ReconnectLegEvent *ev;

  public:
    ReplaceLegEvent(const string &tag, const AmSipRequest &relayed_invite, AmB2BMedia *m)
      : ReliableB2BEvent(ReplaceLeg, NULL, new B2BEvent(B2BTerminateLeg)) { ev = new ReconnectLegEvent(tag, relayed_invite, m); setSender(tag); }
    ReconnectLegEvent *getReconnectEvent() { ReconnectLegEvent *e = ev; ev = NULL; return e; }
    virtual ~ReplaceLegEvent() { if (ev) delete ev; }
};

struct ReplaceInProgressEvent: public B2BEvent
{
  string dst_session; // session to be connected to

  ReplaceInProgressEvent(const string &_dst_session):
      B2BEvent(ReplaceInProgress), dst_session(_dst_session) { }
};

struct DisconnectLegEvent: public B2BEvent
{
  DisconnectLegEvent(): B2BEvent(DisconnectLeg) { }
};

/** composed AmB2BCalleeSession & AmB2BCallerSession
 * represents indepenedently A or B leg of a call,
 * old clases left for compatibility
 *
 * Notes:
 *
 *  - we use the relayEvent implementation from AmB2BSession - it can happen
 *  that we have no peer (i.e. we are a standalone call leg, for example parked
 *  one) => do not create other call leg automatically
 *
 *  - we use onSystemEvent implementation from AmB2BSession - the other leg
 *  receives and handles the same shutdown event, right?
 *
 * */
class CallLeg: public AmB2BSession
{
  public:
    enum CallStatus {
      Disconnected, //< there is no other call leg we are connected to
      NoReply,      //< there is at least one call leg we are connected to but without any response 
      Ringing,      //< this leg or one of legs we are connected to rings
      Connected     //< there is exactly one call leg we are connected to, in this case AmB2BSession::other_id holds the other leg id
    };

  private:

    CallStatus call_status; //< status of the call (replaces callee's status)

    AmSdp initial_sdp;
    bool initial_sdp_stored;

    /** information needed in A leg for a B leg */
    struct BLegInfo {
      /** local tag of the B leg */
      string id;

      /** once the B leg gets connected to the A leg A leg starts to use its
       * corresponding media_session created when the B leg is added to the list
       * of B legs */
      AmB2BMedia *media_session;

      void releaseMediaSession() { if (media_session) { if (media_session->releaseReference()) delete media_session; media_session = NULL; } }
    };

    /** List of legs which can be connected to this leg, it is valid for A leg until first
     * 2xx response which moves the A leg to Connected state and terminates all
     * other B legs.
     *
     * Please note that the A/B role may change during the call leg life. For
     * example when a B leg is parked and then 'rings back on timer' it becomes
     * A leg, i.e. it creates new B leg(s) for itself. */
    std::vector<BLegInfo> b_legs;

    // CSeq of a request generated by us to hold the call
    unsigned hold_request_cseq;

    // methods just for make this stuff more readable, not intended to be
    // overriden, override onB2BEvent instead!
    void onB2BReply(B2BSipReplyEvent *e);
    void onB2BConnect(ConnectLegEvent *e);
    void onB2BReconnect(ReconnectLegEvent *e);
    void onB2BReplace(ReplaceLegEvent *e);
    void onB2BReplaceInProgress(ReplaceInProgressEvent *e);
    void onB2BDisconnect(DisconnectLegEvent* ev);

    int relaySipReply(AmSipReply &reply);

    /** terminate all other B legs than the connected one (should not be used
     * directly by successors, right?) */
    void terminateNotConnectedLegs();

    /** terminate given leg and remove it from list of other legs  (should not
     * be used directly by successors, right?) */
    void terminateOtherLeg(const string &id);

    void removeBLeg(const string &id);

    void terminateCall();

    void updateCallStatus(CallStatus new_status);

    //////////////////////////////////////////////////////////////////////
    // callbacks (intended to be redefined in successors but should not be
    // called by them directly)

    /* handler called when call status changes */
    virtual void onCallStatusChange() { }

    /** handler called when the second leg is connected */
    virtual void onCallConnected(const AmSipReply& reply) { }

    /** handler called when call is stopped */
    virtual void onCallStopped() { }

    /** Method called if given B leg couldn't establish the call (refused with
     * failure response)
     *
     * Redefine to implement serial fork or handle redirect. */
    virtual void onBLegRefused(const AmSipReply& reply) { }

  protected:

    // functions offered to successors

    /** add given call leg as our B leg */
    void addCallee(CallLeg *callee, const AmSipRequest &relayed_invite);

    /** add given already existing session as our B leg */
    void addCallee(const string &session_tag, const AmSipRequest &relayed_invite);

    /** Replace given already existing session in a B2B call. We become new
     * A leg there regardless if we are replacing original A or B leg. */
    void replaceExistingLeg(const string &session_tag, const AmSipRequest &relayed_invite);

    CallStatus getCallStatus() { return call_status; }

    virtual void clear_other();

  public:
    // @see AmB2BSession
    virtual void terminateLeg();
    virtual void terminateOtherLeg();
    virtual void onB2BEvent(B2BEvent* ev);

    // @see AmSession
    virtual void onInvite(const AmSipRequest& req);
    virtual void onInvite2xx(const AmSipReply& reply);
    virtual void onCancel(const AmSipRequest& req);
    virtual void onBye(const AmSipRequest& req);
    virtual void onRemoteDisappeared(const AmSipReply& reply);

    virtual void onSipRequest(const AmSipRequest& req);
    virtual void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);

    //int reinviteCaller(const AmSipReply& callee_reply);

  public:
    /** creates A leg */
    CallLeg();

    /** creates B leg using given session as A leg */
    CallLeg(const CallLeg* caller);

};



#endif
