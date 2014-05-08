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
/** @file AmSipDialog.h */
#ifndef AmSipDialog_h
#define AmSipDialog_h

#include "AmBasicSipDialog.h"
#include "AmSdp.h"
#include "AmOfferAnswer.h"
#include "Am100rel.h"

class AmSipTimeoutEvent;
class AmSipDialogEventHandler;

/**
 * \brief implements the dialog state machine
 */
class AmSipDialog
  : public AmBasicSipDialog
{
protected:
  // Number of open UAS INVITE transactions
  unsigned int pending_invites;

  AmSdp   sdp_local;
  AmSdp   sdp_remote;

  bool early_session_started;
  bool session_started;

  // Current offer/answer transaction
  AmOfferAnswer oa;
  bool offeranswer_enabled;

  // Reliable provisional reply support
  Am100rel rel100;

  int onTxReply(const AmSipRequest& req, AmSipReply& reply, int& flags);
  int onTxRequest(AmSipRequest& req, int& flags);

  void onReplyTxed(const AmSipRequest& req, const AmSipReply& reply);
  void onRequestTxed(const AmSipRequest& req);

  bool onRxReqSanity(const AmSipRequest& req);
  bool onRxReqStatus(const AmSipRequest& req);

  bool onRxReplyStatus(const AmSipReply& reply);


 public:

  AmSipDialog(AmSipDialogEventHandler* h=NULL);
  ~AmSipDialog();

  /** @return whether UAC INVITE transaction is pending */
  bool   getUACInvTransPending();

  /** @return a pending UAS INVITE transaction or NULL */
  AmSipRequest* getUASPendingInv();

  /** 
   * Calls onSdpCompleted on the session event handler
   * and executes onSessionStart/onEarlySessionStart when required.
   */
  int onSdpCompleted();

  bool getSdpOffer(AmSdp& offer);
  bool getSdpAnswer(const AmSdp& offer, AmSdp& answer);

  AmOfferAnswer::OAState getOAState();
  void setOAState(AmOfferAnswer::OAState n_st);
  void setOAEnabled(bool oa_enabled);
  const AmSdp& getLocalSdp() { return oa.getLocalSdp(); }
  const AmSdp& getRemoteSdp() { return oa.getRemoteSdp(); }

  void setRel100State(Am100rel::State rel100_state);

  void uasTimeout(AmSipTimeoutEvent* to_ev);

  /** @return 0 on success */
  int send_200_ack(unsigned int inv_cseq,
		   const AmMimeBody* body = NULL,
		   const string& hdrs = "",
		   int flags = 0);
    
  /** @return 0 on success */
  int bye(const string& hdrs = "", int flags = 0);

  /** @return 0 on success */
  int cancel();

  /** @return 0 on success */
  int prack(const AmSipReply &reply1xx,
	    const AmMimeBody* body,
            const string &hdrs);

  /** @return 0 on success */
  int update(const AmMimeBody* body, 
            const string &hdrs);

  /** @return 0 on success */
  int reinvite(const string& hdrs,
	       const AmMimeBody* body,
	       int flags = 0);

  /** @return 0 on success */
  int invite(const string& hdrs,  
	     const AmMimeBody* body);

  /** @return 0 on success */
  int refer(const string& refer_to,
	    int expires = -1,
	    const string& referred_by = "");

  /** @return 0 on success */
  int info(const string& hdrs,  
	   const AmMimeBody* body);

  /** @return 0 on success */
  int transfer(const string& target);
  int drop();
};

/**
 * \brief base class for SIP request/reply event handler 
 */
class AmSipDialogEventHandler 
  : public AmBasicSipEventHandler
{
public:
  /** Hook called when a provisional reply is received with 100rel active */
  virtual void onInvite1xxRel(const AmSipReply &)=0;

  /** Hook called when a local INVITE request has been replied with 2xx */
  virtual void onInvite2xx(const AmSipReply& reply)=0;

  /** Hook called when an answer for a locally sent PRACK is received */
  virtual void onPrack2xx(const AmSipReply &)=0;

  /** Hook called when a UAS INVITE transaction did not receive the ACK */
  virtual void onNoAck(unsigned int ack_cseq)=0;

  /** Hook called when a UAS INVITE transaction did not receive the PRACK */
  virtual void onNoPrack(const AmSipRequest &req, const AmSipReply &rpl)=0;

  /** Hook called when an SDP offer is required */
  virtual bool getSdpOffer(AmSdp& offer)=0;

  /** Hook called when an SDP offer is required */
  virtual bool getSdpAnswer(const AmSdp& offer, AmSdp& answer)=0;

  /** Hook called when an SDP OA transaction has been completed */
  virtual int onSdpCompleted(const AmSdp& local, const AmSdp& remote)=0;

  /** Hook called when an early session starts 
   *  (SDP OA completed + dialog in early state) */
  virtual void onEarlySessionStart()=0;

  /** Hook called when the session creation is completed 
   *  (INV trans replied with 200) */
  virtual void onSessionStart()=0;

  virtual ~AmSipDialogEventHandler() {};    
};

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
