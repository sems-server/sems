#ifndef __AMB2BCALL_H
#define __AMB2BCALL_H

#include "AmB2BSession.h"

// TODO: global event numbering
enum {
  ConnectMedia = B2BMsgBody + 16,
  ConnectLeg
};

struct ConnectMediaEvent: public B2BEvent
{
  string remote_party;
  string remote_uri;

  AmMimeBody body;
  string hdrs;
  
  bool relayed_invite;
  unsigned int r_cseq;

  ConnectMediaEvent(AmB2BMedia *media)
    : B2BEvent(ConnectMedia),
    remote_party(remote_party),
    remote_uri(remote_uri),
    relayed_invite(false),
    r_cseq(0)
  {}
};

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

    // methods just for make this stuff more readable, not intended to be
    // overriden, override onB2BEvent instead!
    void onB2BReply(B2BSipReplyEvent *e);
    void onB2BConnect(ConnectLegEvent *e);
    void onB2BConnectMedia(ConnectMediaEvent *e);

    int relaySipReply(AmSipReply &reply);

    /** terminate all other B legs than the connected one (should not be used
     * directly by successors, right?) */
    void terminateNotConnectedLegs();

    /** terminate given leg and remove it from list of other legs  (should not
     * be used directly by successors, right?) */
    void terminateOtherLeg(const string &id);

    void updateCallStatus(CallStatus new_status);
    virtual void onCallStatusChange() { }

  public:
    void terminateCall();

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

    virtual void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);

    //int reinviteCaller(const AmSipReply& callee_reply);

    /** Method called if given B leg couldn't establish the call (refused with
     * failure response)
     *
     * Redefine to implement serial fork or handle redirect, 
     * currently only terminates the other leg. */
    virtual void onBLegRefused(const AmSipReply& reply);

    /** handler called when the second leg is connected */
    virtual void onCallConnected(const AmSipReply& reply) { }

    /** handler called when call is stopped */
    virtual void onCallStopped() { }

    void addCallee(CallLeg *callee, const AmSipRequest &relayed_invite);

  public:
    /** creates A leg */
    CallLeg();

    /** creates B leg using given session as A leg */
    CallLeg(const CallLeg* caller);

    /** Initialize RTP relay according to the "src".
     * Previously was initializeRTPRelay which initialised the session given as parameter
     * according settings in this instance (i.e. the opposite stuff)!
     *
     * original comment: initialize RTP relay mode, if rtp_relay_enabled
     * must be called *before* callee_session is started
     */
    void initRTPRelay(const CallLeg* src);

};



#endif
