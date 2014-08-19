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

#define SIP_FLAGS_NOTAG        1<<3 // don't add to-tag in reply

#define SIP_FLAGS_NOBL         1<<4 // do not use destination blacklist

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

class msg_logger;

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

  string callid;

  string local_tag;
  string ext_local_tag;
  
  string remote_tag;
  string first_branch;

  string contact_params; // params in Contact-HF

  string user;         // local user
  string domain;       // local domain

  string local_uri;    // local uri
  string remote_uri;   // remote uri

  string remote_party; // To/From
  string local_party;  // To/From

  string remote_ua; // User-Agent/Server

  string route;

  string next_hop;
  bool next_hop_1st_req;
  bool patch_ruri_next_hop;
  bool next_hop_fixed;

  int outbound_interface;

  TransMap uas_trans;
  TransMap uac_trans;

  /** Dialog usages in the sense of RFC 5057 */
  unsigned int usages;

  AmBasicSipEventHandler* hdl;

  /**
   * Message logger
   */
  msg_logger* logger;

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
  virtual bool onRxReplyStatus(const AmSipReply& reply);

  /**
   * Terminate pending UAS transactions
   */
  virtual void termUasTrans();

  /**
   * Terminate pending UAC transactions
   */
  virtual void termUacTrans();

public:

  string outbound_proxy;
  bool   force_outbound_proxy;

  bool nat_handling;

  unsigned int cseq; // Local CSeq for next request
  bool r_cseq_i;
  unsigned int r_cseq; // last remote CSeq  

  AmBasicSipDialog(AmBasicSipEventHandler* h=NULL);
  virtual ~AmBasicSipDialog();

  void setEventhandler(AmBasicSipEventHandler* h) { hdl = h; }
  
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

  const string& getCallid() const { return callid; }
  virtual void setCallid(const string& n_callid) { callid = n_callid; }

  const string& getLocalTag() const { return local_tag; }
  virtual void setLocalTag(const string& n_tag) { local_tag = n_tag; }

  const string& getRemoteTag() const { return remote_tag; }
  virtual void setRemoteTag(const string& n_tag);

  const string& get1stBranch() const { return first_branch; }
  virtual void set1stBranch(const string& n_branch)
  { first_branch = n_branch; }

  const string& getExtLocalTag() const { return ext_local_tag; }
  virtual void setExtLocalTag(const string& new_ext_tag)
  { ext_local_tag = new_ext_tag; }

  const string& getContactParams() const { return contact_params; }
  virtual void setContactParams(const string& new_contact_params)
  { contact_params = new_contact_params; }

  const string& getUser() const { return user; }
  virtual void setUser(const string& new_user)
  { user = new_user; }

  const string& getDomain() const { return domain; }
  virtual void setDomain(const string& new_domain)
  { domain = new_domain; }

  const string& getLocalUri() const { return local_uri; }
  virtual void setLocalUri(const string& new_local_uri)
  { local_uri = new_local_uri; }

  const string& getRemoteUri() const { return remote_uri; }
  virtual void setRemoteUri(const string& new_remote_uri)
  { remote_uri = new_remote_uri; }

  const string& getLocalParty() const { return local_party; }
  virtual void setLocalParty(const string& new_local_party)
  { local_party = new_local_party; }

  const string& getRemoteParty() const { return remote_party; }
  virtual void setRemoteParty(const string& new_remote_party)
  { remote_party = new_remote_party; }

  const string& getRemoteUA() const { return remote_ua; }
  virtual void setRemoteUA(const string& new_remote_ua)
  { remote_ua = new_remote_ua; }

  const string& getRouteSet() const { return route; }
  virtual void setRouteSet(const string& new_rs)
  { route = new_rs; }

  const string& getNextHop() const { return next_hop; }
  virtual void setNextHop(const string& new_nh)
  { next_hop = new_nh; }

  bool getNextHop1stReq() const { return next_hop_1st_req; }
  virtual void setNextHop1stReq(bool nh_1st_req)
  { next_hop_1st_req = nh_1st_req; }

  bool getPatchRURINextHop() const { return patch_ruri_next_hop; }
  virtual void setPatchRURINextHop(bool patch_nh)
  { patch_ruri_next_hop = patch_nh; }

  bool getNextHopFixed() const { return next_hop_fixed; }
  virtual void setNextHopFixed(bool nh_fixed)
  { next_hop_fixed = nh_fixed; }

  /**
   * Compute the Contact-HF for the next request
   */
  string getContactHdr();

  /**
   * Compute the Contact URI for the next request
   */
  string getContactUri();

  /**
   * Compute the Route-HF for the next request
   */
  string getRoute();

  /**
   * Set outbound_interface to specific value (-1 = default).
   */
  virtual void setOutboundInterface(int interface_id);

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
  virtual void initFromLocalRequest(const AmSipRequest& req);

  /**
   * Executed for requests received by the local UA.
   */
  virtual void onRxRequest(const AmSipRequest& req);

  /**
   * Executed for replies received by the local UA.
   */
  virtual void onRxReply(const AmSipReply& reply);

  /**
   * Updates remote_uri if necessary.
   *
   * Note: this method is offered for inherited classes
   *       implementing dialog functionnalities. It is
   *       not used by the basic class.
   */
  void updateDialogTarget(const AmSipReply& reply);

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
   * Terminates pending UAS/UAC transactions
   */
  virtual void finalize() {
    termUasTrans();
    termUacTrans();
  }

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
			 const string& hdrs = "",
			 msg_logger* logger = NULL);

  /* dump transaction information (DBG) */
  void dump();

  /**
   * Enable or disable message logger
   */
  void setMsgLogger(msg_logger* logger);

  /**
   * Get message logger
   */
  msg_logger* getMsgLogger() { return logger; }
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

  // called upon finishing either UAC or UAS transaction
  virtual void onTransFinished() { }


  virtual ~AmBasicSipEventHandler() {}
};

#endif
