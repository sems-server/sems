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

/** \brief relay a message body to other leg in B2B session */
struct B2BMsgBodyEvent : public B2BEvent {
  string content_type;
  string body;

  B2BMsgBodyEvent(const string& content_type,
		  const string& body)
    : B2BEvent(B2BMsgBody),
    content_type(content_type), body(body) { }
  ~B2BMsgBodyEvent() { }
};

/** \brief trigger connecting the callee leg in B2B session */
struct B2BConnectEvent: public B2BEvent
{
  string remote_party;
  string remote_uri;

  string content_type;
  string body;
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
  enum B2BMode {
    B2BMode_Transparent,  // relay message bodies
    B2BMode_SDPFilter     // reconstruct SDP
  };

 protected:
  /** local tag of the other leg */
  string other_id;

  /** Tell if the session should
   *  process SIP request itself
   * or only relay them (B2B mode).
   */
  bool sip_relay_only;

  B2BMode b2b_mode;
  bool a_leg;

  /** 
   * Requests which have been relayed 
   * from the other leg and sent as SIP
   */
  TransMap relayed_req;

  /** Requests received for relaying */
  std::map<int,AmSipRequest> recvd_req;

  /** content-type of established session */
  string established_content_type;
  /** body of established session */
  string established_body;
  /** hash of body (from o-line) */
  uint32_t body_hash;
  /** save current session description (SDP) */
  void saveSessionDescription(const string& content_type, const string& body);
  /** @return whether session has changed */
  bool updateSessionDescription(const string& content_type, const string& body);

  /** reset relation with other leg */
  void clear_other();

  /** Relay one event to the other side. */
  virtual void relayEvent(AmEvent* ev);

  /** send a relayed SIP Request */
  void relaySip(const AmSipRequest& req);

  /** send a relayed SIP Reply */
  void relaySip(const AmSipRequest& orig, const AmSipReply& reply);

  /** Terminate our leg and forget the other. */
  void terminateLeg();

  /** Terminate the other leg and forget it.*/
  virtual void terminateOtherLeg();

  /** @see AmSession */
  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipReply& reply,
		  int old_dlg_status, const string& trans_method);
  void onInvite2xx(const AmSipReply& reply);

  void onSessionTimeout();

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

  /** filter body ( b2b_mode == SDPFilter */
  virtual int filterBody(string& content_type, string& body, bool is_a2b);

  /** filter SDP body ( b2b_mode == SDPFilter */
  virtual int filterBody(AmSdp& sdp, bool is_a2b);

  AmB2BSession();
  AmB2BSession(const string& other_local_tag);

  virtual ~AmB2BSession();

 public:
  void set_sip_relay_only(bool r);
  B2BMode getB2BMode() const;
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
  void relayEvent(AmEvent* ev);

  /** Tell if the session should
   *  relay early media SDPs to
   *  caller leg
   */
  bool sip_relay_early_media_sdp;

 public:
  AmB2BCallerSession();
  virtual ~AmB2BCallerSession();
    
  CalleeStatus getCalleeStatus() { return callee_status; }

  virtual AmB2BCalleeSession* newCalleeSession();

  void connectCallee(const string& remote_party,
		     const string& remote_uri,
		     bool relayed_invite = false);

  const AmSipRequest& getOriginalRequest() { return invite_req; }

  // @see AmSession
  void onSessionStart(const AmSipRequest& req);

  // @see AmB2BSession
  void terminateLeg();
  void terminateOtherLeg();
  void onB2BEvent(B2BEvent* ev);

  AmSipRequest* getInviteReq() { return &invite_req; }

  void set_sip_relay_early_media_sdp(bool r);
};

/** \brief Callee leg of a B2B session */
class AmB2BCalleeSession: public AmB2BSession
{
 public:
  /*  AmB2BCalleeSession(const string& other_local_tag); */
  AmB2BCalleeSession(const AmB2BCallerSession* caller);

  virtual ~AmB2BCalleeSession();

  void onB2BEvent(B2BEvent* ev);
};

#endif
