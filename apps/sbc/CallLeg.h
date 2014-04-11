/*
 * Copyright (C) 2010-2011 Stefan Sayer
 * Copyright (C) 2012-2013 FRAFOS GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __AMB2BCALL_H
#define __AMB2BCALL_H

#include "AmB2BSession.h"
#include "AmSessionContainer.h"
#include "CallLegEvents.h"
#include "SessionUpdate.h"

#include <queue>

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
	  media_session->releaseReference();
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

    bool on_hold; // remote is on hold
    AmSdp non_hold_sdp;
    enum { HoldRequested, ResumeRequested, PreserveHoldStatus } hold;

    // queue of session update operations, first element is possibly the one
    // being in progress
    std::list<SessionUpdate *> pending_updates;
    class SessionUpdateTimer pending_updates_timer;

    // generate re-INVITE with given parameters (establishing means that the
    // INVITE is establishing a connection between two legs)
    // returns the request CSeq or -1 upon error
    int reinvite(const string &hdrs, const AmMimeBody &body, bool relayed, unsigned r_cseq, bool establishing);

    // put on hold directly, returns CSeq of hold request or -1 if no request
    // was sent out (either error or on hold already)
    int putOnHoldImpl();

    // resume held call directly; returns CSeq of unhold request or -1 if no request
    // was sent out (either error or not on hold)
    int resumeHeldImpl();

    // generate 200 reply on a pending INVITE (uses fake body)
    void acceptPendingInvite(AmSipRequest *invite);

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

    void changeRtpMode(RTPRelayMode new_mode, AmB2BMedia *new_media);
    void changeOtherLegsRtpMode(RTPRelayMode new_mode);

    // offer-answer handling
    void adjustOffer(AmSdp &sdp);

     /* offer was rejected (called just for negative replies to an request
      * carying offer (not always correct?), answer with disabled streams
      * doesn't cause calling this */
     void offerRejected();

    // returns false if there is a pending INVITE so the session can not be
    // changed right now
    bool canUpdateSession() { return !(dlg->getUACInvTransPending() || dlg->getUASPendingInv()); }
    void applyPendingUpdate();
    void updateSession(SessionUpdate *op);

    virtual void onTransFinished();

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
//    virtual void handleHoldReply(bool succeeded);

    /* called to create SDP of locally generated hold request */
    virtual void createHoldRequest(AmSdp &sdp) = 0;

    /** called to alter B2B hold request (i.e. the request from other peer) */
    virtual void alterHoldRequest(AmSdp &sdp) { }

    /* called to create SDP of locally generated resume request */
    virtual void createResumeRequest(AmSdp &sdp);

    /** called to alter B2B hold request (i.e. the request from other peer) */
    virtual void alterResumeRequest(AmSdp &sdp) { }

    /* hold requested (either from B2B peer leg or locally)
     * to be overridden */
    virtual void holdRequested() { }
    virtual void holdAccepted();
    virtual void holdRejected();
    virtual void resumeRequested() { }
    virtual void resumeAccepted();
    virtual void resumeRejected() { }

    virtual void terminateOtherLeg();
    virtual void terminateLeg();

    /** change RTP mode (and AmB2BMedia if needed) but do not send reINVITE
     *
     * Changes the mode for all peer legs as well.
     *
     * WARNING: really unsafe because possibly makes the object state
     * inconsistent with sent SIP messages (sent/received SDPs).
     *
     * WARNING: might require several events exchanged between call legs, the
     * AmB2BMedia will be valid after them!
     *
     * TODO: add parameter to trigger reINVITE */
    void changeRtpMode(RTPRelayMode new_mode);

    virtual void updateLocalSdp(AmSdp &sdp);

    // return retry time for 491 reply in seconds
    virtual double get491RetryTime() { return (get_random() % 200) / 100.0; }

  public:
    virtual void onB2BEvent(B2BEvent* ev);

    /** does all the job around disconnecting from the other leg (updates call
     * status, disconnects RTP from the other, puts remote on hold (if
     * requested)
     *
     * The other leg is not affected by disconnect - it is neither terminated
     * nor informed about the peer disconnection. */
    virtual void disconnect(bool hold_remote, bool preserve_media_session = false);

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
    virtual void resumeHeld(/*bool send_reinvite*/);

    virtual bool isOnHold() { return on_hold; }


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
    void addCallee(CallLeg *callee, const string &hdrs);
//    void addCallee(CallLeg *callee, const string &hdrs, AmB2BSession::RTPRelayMode mode) { addNewCallee(callee, new ConnectLegEvent(hdrs, established_body), mode); }


    /** Replace given already existing session in a B2B call. We become new
     * A leg there regardless if we are replacing original A or B leg. */
    void replaceExistingLeg(const string &session_tag, const AmSipRequest &relayed_invite);
    void replaceExistingLeg(const string &session_tag, const string &hdrs);

    /** generate debug information into log with overall call leg status */
    void debug();

    const char* getCallStatusStr();

    // AmMediaSession interface from AmMediaProcessor
    int readStreams(unsigned long long ts, unsigned char *buffer);
    int writeStreams(unsigned long long ts, unsigned char *buffer);

  public:
    /** creates A leg */
    CallLeg(AmSipDialog* p_dlg=NULL, AmSipSubscription* p_subs=NULL);

    /** creates B leg using given session as A leg */
    CallLeg(const CallLeg* caller, AmSipDialog* p_dlg=NULL,
	    AmSipSubscription* p_subs=NULL);

    virtual ~CallLeg();

    // OA callbacks
    virtual int onSdpCompleted(const AmSdp& local, const AmSdp& remote);
    virtual bool getSdpOffer(AmSdp& offer) { return false; }
    virtual bool getSdpAnswer(const AmSdp& offer, AmSdp& answer) { return false; }
    virtual void onEarlySessionStart() { }
    virtual void onSessionStart() { }

    // classes doing session update are allowed to access our internals:
    friend class Reinvite;
    friend class PutOnHold;
    friend class ResumeHeld;
};



#endif
