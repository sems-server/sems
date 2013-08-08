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
  bool relayed_invite;

  // constructor from relayed INVITE request
  ConnectLegEvent(const AmSipRequest &_relayed_invite):
    B2BEvent(ConnectLeg),
    body(_relayed_invite.body),
    hdrs(_relayed_invite.hdrs),
    r_cseq(_relayed_invite.cseq),
    relayed_invite(true)
  { }

  // constructor from generated INVITE (for example blind call transfer)
  ConnectLegEvent(const string &_hdrs, const AmMimeBody &_body):
    B2BEvent(ConnectLeg),
    body(_body),
    hdrs(_hdrs),
    r_cseq(0),
    relayed_invite(false)
  { }
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
  bool relayed_invite;

  AmB2BMedia *media; // avoid direct access to this
  AmB2BSession::RTPRelayMode rtp_mode;
  string session_tag;
  enum Role { A, B } role; // reconnect as A or B leg

  void setMedia(AmB2BMedia *m, AmB2BSession::RTPRelayMode _mode) { media = m; if (media) media->addReference(); rtp_mode = _mode; }

  ReconnectLegEvent(const string &tag, const AmSipRequest &relayed_invite):
    ReliableB2BEvent(ReconnectLeg, NULL, new B2BEvent(B2BTerminateLeg) /* TODO: choose a better one */),
    body(relayed_invite.body),
    hdrs(relayed_invite.hdrs),
    r_cseq(relayed_invite.cseq),
    relayed_invite(true),
    media(NULL),
    rtp_mode(AmB2BSession::RTP_Direct),
    session_tag(tag),
    role(B) // we have relayed_invite (only in A leg) thus reconnect as regular B leg
  { setSender(tag); }

  ReconnectLegEvent(Role _role, const string &tag, const string &_hdrs, const AmMimeBody &_body):
    ReliableB2BEvent(ReconnectLeg, NULL, new B2BEvent(B2BTerminateLeg) /* TODO: choose a better one */),
    body(_body),
    hdrs(_hdrs),
    r_cseq(0),
    relayed_invite(false),
    media(NULL),
    rtp_mode(AmB2BSession::RTP_Direct),
    session_tag(tag),
    role(_role)
  { setSender(tag); }

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
    ReplaceLegEvent(const string &tag, const AmSipRequest &relayed_invite, AmB2BMedia *m, AmB2BSession::RTPRelayMode mode):
      ReliableB2BEvent(ReplaceLeg, NULL, new B2BEvent(B2BTerminateLeg))
    { ev = new ReconnectLegEvent(tag, relayed_invite); ev->setMedia(m, mode); setSender(tag); }

    ReplaceLegEvent(const string &tag, ReconnectLegEvent *e):
      ReliableB2BEvent(ReplaceLeg, NULL, new B2BEvent(B2BTerminateLeg)),
      ev(e)
    { setSender(tag); }

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
  bool put_remote_on_hold;
  DisconnectLegEvent(bool _put_remote_on_hold): B2BEvent(DisconnectLeg), put_remote_on_hold(_put_remote_on_hold) { }
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
 *  - the role (A/B leg) can be changed during the CallLeg life and is
 *  understood just this way
 *
 *    "A leg is the call leg created when handling initial INVITE request"
 *
 *  It is used
 *    - as identification what part of media session is affected by operation
 *    - when CANCEL is being processed only the CANCEL of initial INVITE is
 *    important so it is explicitly verified that it is handled in A leg)
 *
 *  In other words - the B leg can create new 'b-like-legs' the same way the
 *  A leg does.
 * */
class CallLeg: public AmB2BSession
{
  public:
    /** B2B call status.
     *
     * This status need not to be related directly to SIP dialog status in
     * appropriate call legs - for example the B2B call status can be
     * "Connected" though the legs have received BYE replies. */
    enum CallStatus {
      Disconnected, //< there is no other call leg we are connected to
      NoReply,      //< there is at least one call leg we are connected to but without any response
      Ringing,      //< this leg or one of legs we are connected to rings
      Connected,    //< there is exactly one call leg we are connected to, in this case AmB2BSession::other_id holds the other leg id
      Disconnecting //< we were connected and now going to be disconnected (waiting for reINVITE reply for example)
    };

    /** reason reported in onCallFailed method */
    enum CallFailureReason {
      CallRefused, //< non-ok reply received and no more B-legs exit
      CallCanceled //< call canceled
    };

    /** reason for changing call status */
    struct StatusChangeCause
    {
      enum Reason {
        SipReply,
        SipRequest,
        Canceled,
        NoAck,
        NoPrack,
        RtpTimeout,
        SessionTimeout,
        InternalError,
        Other
      } reason;

      union {
        const AmSipReply *reply;
        const AmSipRequest *request;
        const char *desc;
      } param;
      StatusChangeCause(const AmSipReply *r): reason(SipReply) { param.reply = r; }
      StatusChangeCause(const AmSipRequest *r): reason(SipRequest) { param.request = r; }
      StatusChangeCause(): reason(Other) { param.desc = NULL; }
      StatusChangeCause(const char *desc): reason(Other) { param.desc = desc; }
      StatusChangeCause(const Reason r): reason(r) { param.reply = NULL; }
    };

  private:

    CallStatus call_status; //< status of the call (replaces callee's status)

    AmSdp initial_sdp;
    bool initial_sdp_stored;

    /** information needed in A leg for a B leg */
    struct OtherLegInfo {
      /** local tag of the B leg */
      string id;

      /** once the B leg gets connected to the A leg A leg starts to use its
       * corresponding media_session created when the B leg is added to the list
       * of B legs */
      AmB2BMedia *media_session;

      void releaseMediaSession() {
	if (media_session) {
	  if (media_session->releaseReference())
	    delete media_session;
	  media_session = NULL;
	}
      }
    };

    /** List of legs which can be connected to this leg, it is valid for A leg until first
     * 2xx response which moves the A leg to Connected state and terminates all
     * other B legs.
     *
     * Please note that the A/B role may change during the call leg life. For
     * example when a B leg is parked and then 'rings back on timer' it becomes
     * A leg, i.e. it creates new B leg(s) for itself. */
    std::vector<OtherLegInfo> other_legs;

    unsigned hold_request_cseq; // CSeq of a request generated by us to hold the call
    enum { NotHeld, OnHold, HoldRequested, ResumeRequested } hold_status; // remote is put on hold by us

    // methods just for make this stuff more readable, not intended to be
    // overriden, override onB2BEvent instead!
    void onB2BReply(B2BSipReplyEvent *e);
    void onB2BConnect(ConnectLegEvent *e);
    void onB2BReconnect(ReconnectLegEvent *e);
    void onB2BReplace(ReplaceLegEvent *e);
    void onB2BReplaceInProgress(ReplaceInProgressEvent *e);
    void b2bInitial1xx(AmSipReply& reply, bool forward);
    void b2bInitial2xx(AmSipReply& reply, bool forward);
    void b2bInitialErr(AmSipReply& reply, bool forward);

    int relaySipReply(AmSipReply &reply);

    /** terminate all other B legs than the connected one (should not be used
     * directly by successors, right?) */
    void terminateNotConnectedLegs();

    /** choose given B leg from the list of other B legs */
    bool setOther(const string &id, bool use_initial_sdp);

    /** remove given leg from the list of other legs */
    void removeOtherLeg(const string &id);

    void updateCallStatus(CallStatus new_status, const StatusChangeCause &cause = StatusChangeCause());

    //////////////////////////////////////////////////////////////////////
    // callbacks (intended to be redefined in successors but should not be
    // called by them directly)

    /* handler called when call status changes */
    virtual void onCallStatusChange(const StatusChangeCause &cause) { }

    /** handler called when the second leg is connected (FIXME: this is a hack,
     * use this method in SBCCallLeg only) */
    virtual void onCallConnected(const AmSipReply& reply) { }

    /** Method called if given B leg couldn't establish the call (refused with
     * failure response)
     *
     * Redefine to implement serial fork or handle redirect. */
    virtual void onBLegRefused(const AmSipReply& reply) { }

    /** handler called when all B-legs failed or the call has been canceled. 
	The reply passed is the last final reply. */
    virtual void onCallFailed(CallFailureReason reason, const AmSipReply *reply) { }

    /** add newly created callee with prepared ConnectLegEvent */
    void addNewCallee(CallLeg *callee, ConnectLegEvent *e) { addNewCallee(callee, e, rtp_relay_mode); }

    /** add a newly created calee with prepared ConnectLegEvent and forced RTP
     * relay mode (this is a hack to work around allowed temporary changes of
     * RTP relay mode used for music on hold)
     * FIXME: throw this out once MoH will use another method than temporary RTP
     * Relay mode change */
    void addNewCallee(CallLeg *callee, ConnectLegEvent *e, AmB2BSession::RTPRelayMode mode);

    void addExistingCallee(const string &session_tag, ReconnectLegEvent *e);

    /** Clears other leg, eventually removes it from the list of other legs if
     * it is there. It neither updates call state nor sip_relay_only flag! */
    virtual void clear_other();
    virtual void terminateLeg();
    virtual void terminateOtherLeg();

  protected:

    // functions offered to successors

    virtual void setCallStatus(CallStatus new_status);
    CallStatus getCallStatus() { return call_status; }

    // @see AmSession
    virtual void onInvite(const AmSipRequest& req);
    virtual void onInvite2xx(const AmSipReply& reply);
    virtual void onCancel(const AmSipRequest& req);
    virtual void onBye(const AmSipRequest& req);
    virtual void onRemoteDisappeared(const AmSipReply& reply);
    virtual void onNoAck(unsigned int cseq);
    virtual void onNoPrack(const AmSipRequest &req, const AmSipReply &rpl);
    virtual void onRtpTimeout();
    virtual void onSessionTimeout();

    // @see AmB2BSession
    virtual void onOtherBye(const AmSipRequest& req);

    virtual void onSipRequest(const AmSipRequest& req);
    virtual void onSipReply(const AmSipRequest& req, const AmSipReply& reply, AmSipDialog::Status old_dlg_status);

    virtual void onInitialReply(B2BSipReplyEvent *e);

    /* callback method called when hold/resume request is replied */
    virtual void handleHoldReply(bool succeeded);

    /* called to create SDP of hold request
     * (note that resume request always uses established_body stored before) */
    virtual void createHoldRequest(AmSdp &sdp);

  public:
    virtual void onB2BEvent(B2BEvent* ev);

    /** does all the job around disconnecting from the other leg (updates call
     * status, disconnects RTP from the other, puts remote on hold (if
     * requested)
     *
     * The other leg is not affected by disconnect - it is neither terminated
     * nor informed about the peer disconnection. */
    virtual void disconnect(bool hold_remote);

    /** Terminate the whole B2B call (if there is no other leg only this one is
     * stopped). */
    virtual void stopCall(const StatusChangeCause &cause);


    /** Put remote party on hold (may change RTP relay mode!). Note that this
     * task is asynchronous so the remote is most probably NOT 'on hold' after
     * calling this method
     *
     * This method calls handleHoldReply(false) directly if an error occurs. */
    virtual void putOnHold();

    /** resume call if the remote party is on hold */
    virtual void resumeHeld(bool send_reinvite);

    virtual bool isOnHold() { return hold_status == OnHold; }


    /** add given call leg as our B leg */
    void addCallee(CallLeg *callee, const AmSipRequest &relayed_invite) { addNewCallee(callee, new ConnectLegEvent(relayed_invite)); }

    /** add given already existing session as our B leg */
    void addCallee(const string &session_tag, const AmSipRequest &relayed_invite);
    void addCallee(const string &session_tag, const string &hdrs)
      { addExistingCallee(session_tag, new ReconnectLegEvent(a_leg ? ReconnectLegEvent::B : ReconnectLegEvent::A, getLocalTag(), hdrs, established_body)); }

    /** add given call leg as our B leg (only stored SDP is used, we don't need
     * to have INVITE request.
     * Can be used in A leg and B leg as well.
     * Additional headers may be specified - these are used in outgoing INVITE
     * to the callee's destination. */
    void addCallee(CallLeg *callee, const string &hdrs) { addNewCallee(callee, new ConnectLegEvent(hdrs, established_body)); }
    void addCallee(CallLeg *callee, const string &hdrs, AmB2BSession::RTPRelayMode mode) { addNewCallee(callee, new ConnectLegEvent(hdrs, established_body), mode); }


    /** Replace given already existing session in a B2B call. We become new
     * A leg there regardless if we are replacing original A or B leg. */
    void replaceExistingLeg(const string &session_tag, const AmSipRequest &relayed_invite);
    void replaceExistingLeg(const string &session_tag, const string &hdrs);


  public:
    /** creates A leg */
    CallLeg(AmSipDialog* p_dlg=NULL, AmSipSubscription* p_subs=NULL);

    /** creates B leg using given session as A leg */
    CallLeg(const CallLeg* caller, AmSipDialog* p_dlg=NULL,
	    AmSipSubscription* p_subs=NULL);

    virtual ~CallLeg();
};



#endif
