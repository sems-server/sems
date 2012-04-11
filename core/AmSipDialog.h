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

#include "AmSipMsg.h"
#include "AmSdp.h"
#include "AmOfferAnswer.h"
#include "Am100rel.h"

#include <string>
#include <vector>
#include <map>
using std::string;

#define MAX_SER_KEY_LEN 30
#define CONTACT_USER_PREFIX "sems"

// flags which may be used when sending request/reply
#define SIP_FLAGS_NONE         0 // none
#define SIP_FLAGS_VERBATIM     1 // send request verbatim, 
                                 // i.e. modify as little as possible

#define SIP_FLAGS_NOAUTH       1<<1 // don't add authentication header

class AmSipTimeoutEvent;
class AmSipDialogEventHandler;

/** \brief SIP transaction representation */
struct AmSipTransaction
{
  string       method;
  unsigned int cseq;
  trans_ticket tt;

  AmSipTransaction(const string& method, unsigned int cseq, const trans_ticket& tt)
  : method(method),
    cseq(cseq),
    tt(tt)
  {}

  AmSipTransaction()
  {}
};

typedef std::map<int,AmSipTransaction> TransMap;

/**
 * \brief implements the dialog state machine
 */
class AmSipDialog
  : public AmObject
{
 public:
  enum Status {	
    Disconnected=0,
    Trying,
    Proceeding,
    Cancelling,
    Early,
    Connected,
    Disconnecting,
    __max_Status
  };

private:
  Status status;

  TransMap uas_trans;
  TransMap uac_trans;
    
  // Number of open UAS INVITE transactions
  unsigned int pending_invites;

  // In case a CANCEL should have been sent
  // while in 'Trying' state
  bool         cancel_pending;

  AmSdp   sdp_local;
  AmSdp   sdp_remote;

  bool early_session_started;
  bool session_started;

  AmSipDialogEventHandler* hdl;
  
  int onTxReply(AmSipReply& reply);
  int onTxRequest(AmSipRequest& req);

  string getRoute();

  /** @return 0 on success */
  int sendRequest(const string& method, 
		  const AmMimeBody* body,
		  const string& hdrs,
		  int flags,
		  unsigned int req_cseq);

  // Current offer/answer transaction
  AmOfferAnswer oa;
  bool offeranswer_enabled;

  // Reliable provisional reply support
  Am100rel rel100;

 public:
  string user;         // local user
  string domain;       // local domain

  string local_uri;    // local uri
  string remote_uri;   // remote uri

  string contact_uri;  // pre-calculated contact uri

  string callid;
  string remote_tag;
  string local_tag;

  string first_branch;

  string remote_party; // To/From
  string local_party;  // To/From

  string route;
  string outbound_proxy;
  bool   force_outbound_proxy;

  string next_hop;

  int outbound_interface;

  unsigned int cseq; // Local CSeq for next request
  bool r_cseq_i;
  unsigned int r_cseq; // last remote CSeq  

  AmSipDialog(AmSipDialogEventHandler* h);
  ~AmSipDialog();

  /** @return UAC transaction coresponding to cseq or NULL */
  AmSipTransaction* getUACTrans(unsigned int t_cseq);

  /** @return whether UAC transaction is pending */
  bool   getUACTransPending();

  /** @return whether UAC INVITE transaction is pending */
  bool   getUACInvTransPending();

  /** @return UAS transaction coresponding to cseq or NULL */
  AmSipTransaction* getUASTrans(unsigned int t_cseq);

  /** @return a pending UAS INVITE transaction or NULL */
  AmSipTransaction* getPendingUASInv();

  Status getStatus() { return status; }
  const char* getStatusStr();

  void   setStatus(Status new_status);

  string getContactHdr();

  AmSipDialogEventHandler* getDialogEventHandler() { return hdl; }

  /** 
   * Computes, set and return the outbound interface
   * based on remote_uri, next_hop_ip, outbound_proxy, route.
   */
  int getOutboundIf();

  /**
   * Resets outbound_interface to it default value (-1).
   */
  void resetOutboundIf();


  /** update Status from locally originated request (e.g. INVITE) */
  void initFromLocalRequest(const AmSipRequest& req);

  void onRxRequest(const AmSipRequest& req);
  void onRxReply(const AmSipReply& reply);

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

  void setRel100State(Am100rel::State rel100_state);

  void uasTimeout(AmSipTimeoutEvent* to_ev);

  /** @return 0 on success (deprecated) */
  int reply(const AmSipRequest& req,
	    unsigned int  code, 
	    const string& reason,
	    const AmMimeBody* body = NULL,
	    const string& hdrs = "",
	    int flags = 0);

  /** @return 0 on success */
  int reply(const AmSipTransaction& t,
	    unsigned int  code, 
	    const string& reason,
	    const AmMimeBody* body = NULL,
	    const string& hdrs = "",
	    int flags = 0);

  /** @return 0 on success */
  int sendRequest(const string& method, 
		  const AmMimeBody* body = NULL,
		  const string& hdrs = "",
		  int flags = 0);

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
	    int expires = -1);

  /** @return 0 on success */
  int transfer(const string& target);
  int drop();

  /**
   * This method should only be used to send responses
   * to requests which are not referenced by any dialog.
   *
   * WARNING: If the request has already been referenced 
   * (see uas_trans, pending_invites), this method cannot 
   * mark the request as replied, thus leaving it
   * in the pending state forever.
   */
  static int reply_error(const AmSipRequest& req,
			 unsigned int  code,
			 const string& reason,
			 const string& hdrs = "");
};

/**
 * \brief base class for SIP request/reply event handler 
 */
class AmSipDialogEventHandler 
{
 public:
  /** 
   * Hook called when a request has been received
   */
  virtual void onSipRequest(const AmSipRequest& req)=0;

  /** Hook called when a reply has been received */
  virtual void onSipReply(const AmSipReply& reply, 
			  AmSipDialog::Status old_dlg_status)=0;

  /** Hook called before a request is sent */
  virtual void onSendRequest(AmSipRequest& req, int flags)=0;
    
  /** Hook called before a reply is sent */
  virtual void onSendReply(AmSipReply& reply, int flags)=0;

  /** Hook called when the remote side is unreachable - 408/481 reply received */
  virtual void onRemoteDisappeared(const AmSipReply &)=0;

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

  /** Hook called when an early session starts (SDP OA completed + dialog in early state) */
  virtual void onEarlySessionStart()=0;

  /** Hook called when the session creation is completed (INV trans replied with 200) */
  virtual void onSessionStart()=0;

  enum FailureCause {
    FAIL_REL100_421,
#define FAIL_REL100_421  AmSipDialogEventHandler::FAIL_REL100_421
    FAIL_REL100_420,
#define FAIL_REL100_420  AmSipDialogEventHandler::FAIL_REL100_420
  };

  virtual void onFailure(FailureCause cause, const AmSipRequest*, 
      const AmSipReply*)=0;

  virtual ~AmSipDialogEventHandler() {};
    
};

const char* dlgStatusStr(AmSipDialog::Status st);


#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
