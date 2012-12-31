/*
 * Copyright (C) 2012 Frafos GmbH
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
#ifndef _AmBasicSipDialog_h_
#define _AmBasicSipDialog_h_

#include "AmSipMsg.h"

#include <string>
#include <vector>
#include <map>
using std::string;

// flags which may be used when sending request/reply
#define SIP_FLAGS_VERBATIM     1 // send request verbatim, 
                                 // i.e. modify as little as possible

#define SIP_FLAGS_NOAUTH       1<<1 // don't add authentication header
#define SIP_FLAGS_NOCONTACT    1<<2 // don't add contact

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

typedef std::map<int,AmSipRequest> TransMap;

class AmBasicSipEventHandler;

class AmBasicSipDialog
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
  static const char* status2str[__max_Status];

protected:
  Status status;

  TransMap uas_trans;
  TransMap uac_trans;

  /** Dialog usages in the sense of RFC 5057 */
  unsigned int usages;

  AmBasicSipEventHandler* hdl;

  /**
   * Executed for replies sent by a local UA,
   * right before the reply is passed to the transaction layer.
   */
  virtual int onTxReply(const AmSipRequest& req, AmSipReply& reply, int& flags);

  /**
   * Executed for requests sent by a local UA,
   * right before the request is passed to the transaction layer.
   */
  virtual int onTxRequest(AmSipRequest& req, int& flags);

  /**
   * Executed for replies sent by a local UA,
   * after the reply has been successfuly sent.
   */
  virtual void onReplyTxed(const AmSipRequest& req, const AmSipReply& reply);

  /**
   * Executed for requests sent by a local UA,
   * after the request has been successfuly sent.
   */
  virtual void onRequestTxed(const AmSipRequest& req);

  /**
   * Basic sanity check on received requests
   *
   * Note: At this point in the processing, 
   *       the request has not been inserted yet
   *       into the uas_trans container.
   *       Thus, reply_error() should be used 
   *       instead of reply() method.
   *       
   * @return true to continue processing, false otherwise
   */
  virtual bool onRxReqSanity(const AmSipRequest& req);
  
  /**
   * Executed from onRxRequest() to allow inherited classes
   * to extend the basic behavior.
   *
   * @return true to continue processing, false otherwise
   */
  virtual bool onRxReqStatus(const AmSipRequest& req) { return true; }

  /**
   * Executed from onRxReply() to allow inherited classes
   * to extend the basic behavior (deletes the transaction on final reply).
   *
   * @return true to continue processing, false otherwise
   */
  virtual bool onRxReplyStatus(const AmSipReply& reply, 
			       TransMap::iterator t_uac_it);

public:

  string user;         // local user
  string domain;       // local domain

  string local_uri;    // local uri
  string remote_uri;   // remote uri


  string callid;
  string remote_tag;
  string local_tag;
  string ext_local_tag;

  string first_branch;

  string remote_party; // To/From
  string local_party;  // To/From

  string route;
  string outbound_proxy;
  bool   force_outbound_proxy;

  string next_hop;
  bool next_hop_1st_req;

  int  outbound_interface;
  bool nat_handling;

  unsigned int cseq; // Local CSeq for next request
  bool r_cseq_i;
  unsigned int r_cseq; // last remote CSeq  

  AmBasicSipDialog(AmBasicSipEventHandler* h=0);
  virtual ~AmBasicSipDialog();
  
  /** @return UAC request coresponding to cseq or NULL */
  AmSipRequest* getUACTrans(unsigned int t_cseq);

  /** @return UAS request coresponding to cseq or NULL */
  AmSipRequest* getUASTrans(unsigned int t_cseq);

  /** @return the method of the corresponding uac request */
  string getUACTransMethod(unsigned int t_cseq);

  /** @return whether UAC transaction is pending */
  bool   getUACTransPending();

  /**
   * Getter/Setter basic dialog status
   */
  Status       getStatus() const { return status; }
  virtual void setStatus(Status new_status);

  virtual const char* getStatusStr();
  static const char* getStatusStr(Status st);
  
  unsigned int getUsages() { return usages; }
  void incUsages() { usages++; }
  void decUsages() { usages--; }

  /**
   * Compute the Contact-HF for the next request
   */
  string getContactHdr();

  /**
   * Compute the Route-HF for the next request
   */
  string getRoute();

  /**
   * Set outbound_interface to specific value (-1 = default).
   */
  void setOutboundInterface(int interface_id);

  /** 
   * Compute, set and return the outbound interface
   * based on remote_uri, next_hop_ip, outbound_proxy, route.
   */
  int getOutboundIf();

  /**
   * Reset outbound_interface to default value (-1).
   */
  void resetOutboundIf();

  /**
   * Set outbound_interface to specific value (-1 = default).
   */
  //void setOutboundInterface(int interface_id);

  /** Initialize dialog from locally originated UAC request */
  void initFromLocalRequest(const AmSipRequest& req);

  /**
   * Executed for requests received by the local UA.
   */
  void onRxRequest(const AmSipRequest& req);

  /**
   * Executed for replies received by the local UA.
   */
  void onRxReply(const AmSipReply& reply);

  /**
   * Updates remote_uri if necessary.
   *
   * Note: this method is offered for inherited classes
   *       implementing dialog functionnalities. It is
   *       not used by the basic class.
   */
  void updateDialogTarget(const AmSipReply& reply);

  void updateRouteSet(const string& new_rs);
  void updateRemoteTag(const string& new_rt);


  /** @return 0 on success */
  virtual int reply(const AmSipRequest& req,
		    unsigned int  code, 
		    const string& reason,
		    const AmMimeBody* body = NULL,
		    const string& hdrs = "",
		    int flags = 0);

  /** @return 0 on success */
  virtual int sendRequest(const string& method, 
			  const AmMimeBody* body = NULL,
			  const string& hdrs = "",
			  int flags = 0);

  /**
   * This method should only be used to send responses
   * to requests which are not referenced by any dialog.
   *
   * WARNING: If the request has already been referenced 
   * (see uas_trans), this method cannot mark the request 
   * as replied, thus leaving it in the pending state forever.
   */
  static int reply_error(const AmSipRequest& req,
			 unsigned int  code,
			 const string& reason,
			 const string& hdrs = "");

  /* dump transaction information (DBG) */
  void dump();
};

/**
 * \brief base class for SIP request/reply event handler
 */
class AmBasicSipEventHandler
{
 public:

  /** Hook called when a request has been received */
  virtual void onSipRequest(const AmSipRequest& req) {}

  /** Hook called when a reply has been received */
  virtual void onSipReply(const AmSipRequest& req,
			  const AmSipReply& reply,
			  AmBasicSipDialog::Status old_status) {}

  /** Hook called before a request is sent */
  virtual void onSendRequest(AmSipRequest& req, int& flags) {}
    
  /** Hook called before a reply is sent */
  virtual void onSendReply(const AmSipRequest& req, 
			   AmSipReply& reply, int& flags) {}

  /** Hook called after a request has been sent */
  virtual void onRequestSent(const AmSipRequest& req) {}

  /** Hook called after a reply has been sent */
  virtual void onReplySent(const AmSipRequest& req, const AmSipReply& reply) {}
    
  /**
   * Hook called when the all dialog usages should be terminated
   * after a reply received from the far end, or a locally generated
   * timeout (408).
   */
  virtual void onRemoteDisappeared(const AmSipReply& reply) {}

  /** 
   * Hook called when the all dialog usages should be terminated 
   * before a local reply is sent.
   */
  virtual void onLocalTerminate(const AmSipReply& reply) {}

  /** 
   * Hook called when either a received request or 
   * reply has been rejected by the local SIP UA layer.
   */
  virtual void onFailure() {}

  virtual ~AmBasicSipEventHandler() {}
};

#endif
