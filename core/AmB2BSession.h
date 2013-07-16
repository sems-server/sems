/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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
/** @file AmB2BSession.h */
#ifndef AmB2BSession_h
#define AmB2BSession_h

#include "AmSession.h"
#include "AmSipDialog.h"
#include "sip/hash.h"
#include "AmB2BMedia.h"
#include "AmSipSubscription.h"

#define MAX_RELAY_STREAMS 3 // voice, video, rtt

enum { B2BTerminateLeg,
       B2BConnectLeg,
       B2BSipRequest,
       B2BSipReply,
       B2BMsgBody };

/** \brief base class for event in B2B session */
struct B2BEvent: public AmEvent
{
  B2BEvent(int ev_id) 
    : AmEvent(ev_id)
  {}
};

/** \brief base class for SIP event in B2B session */
struct B2BSipEvent: public B2BEvent
{
  bool forward;

  B2BSipEvent(int ev_id, bool forward)
    : B2BEvent(ev_id),
       forward(forward)
  {}
};

/** \brief SIP request in B2B session */
struct B2BSipRequestEvent: public B2BSipEvent
{
  AmSipRequest req;

  B2BSipRequestEvent(const AmSipRequest& req, bool forward)
    : B2BSipEvent(B2BSipRequest,forward),
       req(req)
  { }
};

/** \brief SIP reply in B2B session */
struct B2BSipReplyEvent: public B2BSipEvent
{
  AmSipReply reply;
  string trans_method;

 B2BSipReplyEvent(const AmSipReply& reply, bool forward,
		  string trans_method)
   : B2BSipEvent(B2BSipReply,forward),
    reply(reply), trans_method(trans_method)
  { }
};

/** \brief trigger connecting the callee leg in B2B session */
struct B2BConnectEvent: public B2BEvent
{
  string remote_party;
  string remote_uri;

  AmMimeBody body;
  string hdrs;
  
  bool relayed_invite;
  unsigned int r_cseq;

  B2BConnectEvent(const string& remote_party,
		  const string& remote_uri)
    : B2BEvent(B2BConnectLeg),
    remote_party(remote_party),
    remote_uri(remote_uri),
    relayed_invite(false),
    r_cseq(0)
  {}
};

/**
 * \brief Base class for Sessions in B2BUA mode.
 * 
 * It has two legs as independent sessions:
 * Callee- and caller-leg.
 */
class AmB2BSession: public AmSession
{
 public:

  enum RTPRelayMode {
    /* audio will go directly between caller and callee
     * SDP bodies of relayed requests are filtered */  
    RTP_Direct,

    /* audio will be realyed through us
     * SDP bodies of relayed requests are filtered 
     * and connection addresses are replaced by us
     */
    RTP_Relay,

    /*
     * similar to RTP_Relay, but additionally transcoding 
     * might be used depending on payload IDs 
     */
    RTP_Transcoding
  };

private:
  /** local tag of the other leg */
  string other_id;

 protected:
  /** Tell if the session should
   *  process SIP request itself
   * or only relay them (B2B mode).
   */
  bool sip_relay_only;

  bool a_leg;

  /** 
   * Requests which have been relayed 
   * from the other leg and sent as SIP
   */
  TransMap relayed_req;

  /** Requests received for relaying */
  std::map<int,AmSipRequest> recvd_req;

  /** CSeq of the INVITE that established this call */
  unsigned int est_invite_cseq;
  unsigned int est_invite_other_cseq;

  /** SUBSCRIBE/NOTIFY handling */
  AmSipSubscription* subs;

  /** body of established session */
  AmMimeBody established_body;
  /** hash of body (from o-line) */
  uint32_t body_hash;
  /** save current session description (SDP) */
  virtual bool saveSessionDescription(const AmMimeBody& body);
  /** @return whether session description (SDP) has changed */
  virtual bool updateSessionDescription(const AmMimeBody& body);

  /** reset relation with other leg */
  virtual void clear_other();

  /** Relay one event to the other side. @return 0 on success */
  virtual int relayEvent(AmEvent* ev);

  /** send a relayed SIP Request */
  int relaySip(const AmSipRequest& req);

  /** send a relayed SIP Reply */
  int relaySip(const AmSipRequest& orig, const AmSipReply& reply);

  void relayError(const string &method, unsigned cseq, bool forward, int sip_code, const char *reason);
  void relayError(const string &method, unsigned cseq, bool forward, int err_code);

  /** Terminate our leg and forget the other. */
  virtual void terminateLeg();

  /** Terminate the other leg and forget it.*/
  virtual void terminateOtherLeg();


  /** @see AmSession */
  virtual void updateUACTransCSeq(unsigned int old_cseq, unsigned int new_cseq);

  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipRequest& req, const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);

  void onRequestSent(const AmSipRequest& req);
  void onReplySent(const AmSipRequest& req, const AmSipReply& reply);

  void onInvite2xx(const AmSipReply& reply);

  int onSdpCompleted(const AmSdp& local_sdp, const AmSdp& remote_sdp);

  void onRemoteDisappeared(const AmSipReply& reply);

  void onRtpTimeout();
  void onSessionTimeout();
  void onNoAck(unsigned int cseq);

  /** send re-INVITE with established session description 
   *  @return 0 on success
   */
  int sendEstablishedReInvite();

  /** do session refresh */
  bool refresh(int flags = 0);

  /** @see AmEventQueue */
  void process(AmEvent* event);

  /** B2BEvent handler */
  virtual void onB2BEvent(B2BEvent* ev);

  /** handle BYE on other leg */
  virtual void onOtherBye(const AmSipRequest& req);

  /** 
   * Reply received from other leg has been replied 
   * @return true if reply was processed (should be absorbed)
   * @return false if reply was not processed
   */
  virtual bool onOtherReply(const AmSipReply& reply);

  AmB2BSession(const string& other_local_tag = "", AmSipDialog* p_dlg=NULL,
	       AmSipSubscription* p_subs=NULL);

  virtual ~AmB2BSession();

  /** flag to enable RTP relay mode */
  RTPRelayMode rtp_relay_mode;
  /** force symmetric RTP */
  bool rtp_relay_force_symmetric_rtp;
  /** transparent seqno for RTP relay */
  bool rtp_relay_transparent_seqno;
  /** transparent SSRC for RTP relay */
  bool rtp_relay_transparent_ssrc;
  /** If true, transcoded audio is injected into 
      the inband DTMF detector */
  bool enable_dtmf_transcoding;
  /** Low fidelity payloads for which inband DTMF 
      transcoding should be used */
  vector<SdpPayload> lowfi_payloads;

  /** clear our and the other side's RTP streams from RTPReceiver */
  void clearRtpReceiverRelay();
  /** update remote connection in relay_streams */
  void updateRelayStreams(const AmMimeBody& body,
			  AmSdp& parser_sdp);

  /** replace connection with our address */
  bool updateLocalBody(const AmMimeBody& body, AmMimeBody& r_body);

  /** Called when SDP relayed from other leg should be sent to the remote party.
   * Default implementation updates connection address and ports. */
  virtual bool updateLocalSdp(AmSdp &sdp);

  /** Passes remote SDP to AmB2BMediaSession, might be redefined to provide
   * another functionality than just simply passing SDP */
  virtual bool updateRemoteSdp(AmSdp &sdp);

 public:

  virtual void setOtherId(const string& n_other_id) {
    other_id = n_other_id;
  }
  virtual const string& getOtherId() const { return other_id; }

  void set_sip_relay_only(bool r);

  /** set RTP relay mode (possibly initiaze by given INVITE) */
  virtual void setRtpRelayMode(RTPRelayMode mode);

  /** link RTP streams of other_session to our streams */
  RTPRelayMode getRtpRelayMode() const { return rtp_relay_mode; }
  bool getRtpRelayForceSymmetricRtp() const { return rtp_relay_force_symmetric_rtp; }
  bool getEnableDtmfTranscoding() const { return enable_dtmf_transcoding; }
  void getLowFiPLs(vector<SdpPayload>& lowfi_payloads) const;

  virtual void setRtpInterface(int relay_interface);
  virtual void setRtpRelayForceSymmetricRtp(bool force_symmetric);
  void setRtpRelayTransparentSeqno(bool transparent);
  void setRtpRelayTransparentSSRC(bool transparent);

  void setEnableDtmfTranscoding(bool enable);
  void setLowFiPLs(const vector<SdpPayload>& lowfi_payloads);
  
  bool getRtpRelayTransparentSeqno() { return rtp_relay_transparent_seqno; }
  bool getRtpRelayTransparentSSRC() { return rtp_relay_transparent_ssrc; }

  /* -------------- media processing -------------- */

  private:
    AmB2BMedia *media_session;

  public:
    virtual void setMediaSession(AmB2BMedia *new_session);
    AmB2BMedia *getMediaSession() { return media_session; }
};

class AmB2BCalleeSession;

/** \brief Caller leg of a B2B session */
class AmB2BCallerSession: public AmB2BSession
{
 public:
  enum CalleeStatus {
    None=0,
    NoReply,
    Ringing,
    Connected
  };

 private:
  // Callee Status
  CalleeStatus callee_status;
  
  int  reinviteCaller(const AmSipReply& callee_reply);

 protected:
  AmSipRequest invite_req;
  virtual void createCalleeSession();
  int relayEvent(AmEvent* ev);

  /** Tell if the session should
   *  relay early media SDPs to
   *  caller leg
   */
  bool sip_relay_early_media_sdp;

 public:
  AmB2BCallerSession();
  virtual ~AmB2BCallerSession();
    
  CalleeStatus getCalleeStatus() { return callee_status; }
  void setCalleeStatus(CalleeStatus c) { callee_status = c; }

  virtual AmB2BCalleeSession* newCalleeSession();

  void connectCallee(const string& remote_party,
		     const string& remote_uri,
		     bool relayed_invite = false);

  const AmSipRequest& getOriginalRequest() { return invite_req; }

  // @see AmSession
  void onInvite(const AmSipRequest& req);
  void onInvite2xx(const AmSipReply& reply);
  void onCancel(const AmSipRequest& req);
  void onBye(const AmSipRequest& req);

  void onRemoteDisappeared(const AmSipReply& reply);

  void onSystemEvent(AmSystemEvent* ev);

  // @see AmB2BSession
  void terminateLeg();
  void terminateOtherLeg();
  virtual void onB2BEvent(B2BEvent* ev);

  AmSipRequest* getInviteReq() { return &invite_req; }

  void set_sip_relay_early_media_sdp(bool r);

  /** initialize RTP relay mode, if rtp_relay_enabled
      must be called *before* callee_session is started
   */
  void initializeRTPRelay(AmB2BCalleeSession* callee_session);
};

/** \brief Callee leg of a B2B session */
class AmB2BCalleeSession: public AmB2BSession
{
 public:
  AmB2BCalleeSession(const string& other_local_tag);
  AmB2BCalleeSession(const AmB2BCallerSession* caller);

  virtual ~AmB2BCalleeSession();

  virtual void onB2BEvent(B2BEvent* ev);
};

#endif
