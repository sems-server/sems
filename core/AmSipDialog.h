/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
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

#include <string>
#include <vector>
#include <map>
using std::string;

#define MAX_SER_KEY_LEN 30
#define CONTACT_USER_PREFIX "sems"

// flags which may be used when sending request/reply
#define SIP_FLAGS_VERBATIM     1 // send request verbatim, 
                                 // i.e. modify as little as possible

/** \brief SIP transaction representation */
struct AmSipTransaction
{
    string       method;
    unsigned int cseq;
    trans_ticket tt;

  // last reply code
  // (sent or received)
  //int reply_code;

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
 * \brief base class for SIP request/reply event handler 
 */
class AmSipDialogEventHandler 
{
public:
    
    /** Hook called when a request has been received */
    virtual void onSipRequest(const AmSipRequest& req)=0;

    /** Hook called when a reply has been received */
    virtual void onSipReply(const AmSipReply& reply, int old_dlg_status)=0;

    /** Hook called before a request is sent */
    virtual void onSendRequest(const string& method,
			       const string& content_type,
			       const string& body,
			       string& hdrs,
			       int flags,
			       unsigned int cseq)=0;
    
    /** Hook called before a reply is sent */
    virtual void onSendReply(const AmSipRequest& req,
			     unsigned int  code,
			     const string& reason,
			     const string& content_type,
			     const string& body,
			     string& hdrs,
			     int flags)=0;
    
    /** Hook called when a local INVITE request has been replied with 2xx */
    virtual void onInvite2xx(const AmSipReply& reply)=0;

    virtual ~AmSipDialogEventHandler() {};
};

/**
 * \brief implements the dialog state machine
 */
class AmSipDialog
{
  int status;

  TransMap uas_trans;
  TransMap uac_trans;
    
  unsigned int pending_invites;

  AmSipDialogEventHandler* hdl;

  int updateStatusReply(const AmSipRequest& req, 
			unsigned int code);

 public:
  enum Status {
	
    Disconnected=0,
    Pending,
    Connected,
    Disconnecting
  };

  static const char* status2str[4];

  string user;         // local user
  string domain;       // local domain

  string local_uri;    // local uri
  string remote_uri;   // remote uri

  string contact_uri;  // pre-calculated contact uri

  string callid;
  string remote_tag;
  string local_tag;

  string remote_party; // To/From
  string local_party;  // To/From

  string route;
  string outbound_proxy;
  bool   force_outbound_proxy;

  unsigned int cseq; // Local CSeq for next request
  unsigned int r_cseq; // last remote CSeq  

  AmSipDialog(AmSipDialogEventHandler* h=0);
  ~AmSipDialog();

  bool   getUACTransPending() { return !uac_trans.empty(); }
  int    getStatus() { return status; }
  string getContactHdr();

  /** update Status from locally originated request (e.g. INVITE) */
  void updateStatusFromLocalRequest(const AmSipRequest& req);

  void updateStatus(const AmSipRequest& req);
  void updateStatus(const AmSipReply& reply);
    
  int reply(const AmSipRequest& req,
	    unsigned int  code, 
	    const string& reason,
	    const string& content_type = "",
	    const string& body = "",
	    const string& hdrs = "",
	    int flags = 0);

  int sendRequest(const string& method, 
		  const string& content_type = "",
		  const string& body = "",
		  const string& hdrs = "",
		  int flags = 0);

  int send_200_ack(const AmSipTransaction& t,
		   const string& content_type = "",
		   const string& body = "",
		   const string& hdrs = "",
		   int flags = 0);
    
  int bye(const string& hdrs = "");
  int cancel();
  int update(const string& hdrs);
  int reinvite(const string& hdrs,  
	       const string& content_type,
	       const string& body);
  int invite(const string& hdrs,  
	     const string& content_type,
	     const string& body);
  int refer(const string& refer_to,
	    int expires = -1);
  int transfer(const string& target);
  int drop();

  /**
   * @return the method of the corresponding uac request
   */
  string get_uac_trans_method(unsigned int cseq);

  AmSipTransaction* get_uac_trans(unsigned int cseq);

  static int reply_error(const AmSipRequest& req,
			 unsigned int  code, 
			 const string& reason,
			 const string& hdrs = "");
};


#endif
