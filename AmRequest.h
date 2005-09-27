/*
 * $Id: AmRequest.h,v 1.20.2.2 2005/08/03 21:00:30 sayer Exp $
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

#ifndef _AmRequest_h_
#define _AmRequest_h_

#include "AmCmd.h"
#include "AmSdp.h"
#include "AmThread.h"
#include "AmEventQueue.h"
#include "AmUACAuth.h"

#include <stdio.h>
#include <string>
using std::string;

class AmSession;
class AmSessionContainer;

/**
 * Holds and handles the client's request and following responses.
 * 
 */
class AmRequest
{
protected:

    AmSdp                sdp;
    AmSessionContainer*  session_container;
    bool                 replied;
    string               m_body;
    std::string          content_type;

    /** @return 0 if succeded. */
    int sendRequest(const string& method,
		    unsigned int& code, 
		    string& reason,
		    std::string& auth_header,
		    const string& body = "",
		    const string& content_type = "");

    /** send a request without body. @return 0 if succeded. */
    int sendRequest(const std::string& method,
	 	    unsigned int& code, 
		    std::string& reason);

    /** @return 0 if succeded. */
    int send(const string& fifo_name, const string& msg,
	     unsigned int& code, string& reason, std::string& auth_header);

    /** without auth_header. @return 0 if succeded. */
    int send(const string& ser_cmd, const string& msg, 
	   unsigned int& code, string& reason);

    /** @return 0 if succeded. */
    int getReturnCode(char* reply_buf, unsigned int& code, 
		      std::string& reason, std::string& auth_hdr);

    string getContactHdr();

    AmRequest(const AmCmd& _cmd);

public:
    AmCmd cmd;

    bool getReplied() { return replied; }

    virtual AmRequest* duplicate()=0;
    virtual ~AmRequest(){}

    /**
     * Sends a reply.
     * @return 0 if succeded.
     */
    virtual int reply(int code, const string& reason, const string& body="") { return 0; }

    /** 
     * Accept the SDP request. 
     * Warning: throws AmSession::Exception()
     */
    virtual void accept(int localport) {}

    /**
     * Process the request.
     * @return true if failed
     */
    virtual void execute()=0;

    /**
     * End the dialog.
     * @return 0 if succeded.
     */
    int bye();

    /**
     * Raise an error.
     */
    void error(int code, const string& reason);
    
    friend class AmSession;
};

class AmRequestUAS: public AmRequest
{
public:
    AmRequestUAS(const AmCmd& _cmd, char* body_buf);
    AmRequest* duplicate();
    void execute();

    void accept(int localport);
    int reply(int code, const string& reason, const string& body="");

    const string& getBody() { return m_body; }
};

class AmRequestUAC: public AmRequest
{

    std::string status_subscriber_dlg_hash;
    std::string status_subscriber_dlg_key;
    void notify_uac_req_status(unsigned int code, std::string& reason);

    std::vector<UACAuthCredential> credentials;
public:
    AmRequestUAC(const AmCmd& _cmd, 
		 std::string req_status_subscriber_hash_ = "", 
		 std::string req_status_subscriber_key_ = "")
	: AmRequest(_cmd), status_subscriber_dlg_hash(req_status_subscriber_hash_),
	  status_subscriber_dlg_key(req_status_subscriber_key_)  {}

    AmRequest* duplicate();

    void execute();
    
    /**
     * Dials a SIP User Agent.
     * @param user      Any string you want to get back in AmCmd.user
     *                  within the following call to AmStateFactory::onInvite.
     * @param app_name  Name of the application plug-in which gets the call.
     * @param uri       Client's uri.
     * @param from_user Value for SIP 'From' header.
     * @param dialout_status_subscriber  Eventqueue that will get the response 
     *                                   of the new call.
     * @return          A copy of the AmCmd object which has used to 
     *                  constuct the request.
     */
    static AmCmd dialout(const string& user,
			 const string& app_name,
			 const string& uri, 
			 const string& from_user,
			 std::string dialout_status_subscriber_hash = "", 
			 std::string dialout_status_subscriber_key = "");

     /**
      * Dialout with a credential.
      *
      * @see AmRequest::dialout
      */
       static AmCmd dialoutEx(const std::string& user,
			      const std::string& app_name,
			      const std::string& uri, 
			      const std::string& from_user,
			      const std::string& next_hop,
			      const UACAuthCredential cred,
			      std::string dialout_status_subscriber_hash = "", 
			      std::string dialout_status_subscriber_key = "");

     /**
      * send a REGISTER
      *
      */
      static AmCmd registerUAC(const std::string& app_name,
			       const std::string& uri,
			       const std::string& to,
			       const std::string& from,
			       const std::string& user,
			       const std::string& next_hop,
			       unsigned int expires,
			       const UACAuthCredential cred,
			       std::string dialout_status_subscriber_hash = "", 
			       std::string dialout_status_subscriber_key = "");

    /**
     * Sends an in-dialog REFER.
     * @param cmd       A copy of the AmCmd object which has
     *                  been used to construct the request which
     *                  has to be REFER-ed.
     * @param refer_to  The SIP URI that the call shal be referred to.
     * @param refer_status_subscriber EventQueue that the response of the refer
     *                                will go into.
     */
    
    static AmCmd refer(const AmCmd& cmd,
		       const string& refer_to,
		       std::string refer_status_subscriber_hash = "", 
		       std::string refer_status_subscriber_key = "");

    /**
     * Cancels a previous Request.
     * @param _cmd A copy of the AmCmd object which has
     *             been used to construct the request which
     *             has to be canceled.
     */
    static void cancel(const AmCmd& cmd);

     /**
      *  add a credential to the request
      */
     void add_credential(const UACAuthCredential& cred);
};

class AmRequestUACStatusEvent : public AmEvent {

public:
    int code;
    string reason;
    AmRequestUAC request;

    enum EventType { Accepted = 4, Error };
    AmRequestUACStatusEvent(EventType event_type, const AmRequestUAC& request,
			    int code_, string reason_);
};


class AmASyncRequestUAC: public AmRequestUAC, public AmThread
{
public:
    AmASyncRequestUAC(const AmCmd& _cmd, 
		      std::string status_subscriber_dlg_hash = "", 
		      std::string status_subscriber_dlg_key = "");
    void run();
    void on_stop();
};

/**
 * AmASyncFullRequestUAC can have a body and content-type.
 * 
 * @see AmASyncRequestUAC
 */

class AmASyncFullRequestUAC : public AmASyncRequestUAC
{
public: 
    AmASyncFullRequestUAC(const AmCmd& _cmd, const std::string& body = "", 
			  const std::string& content_type_ = "",
			  std::string status_subscriber_dlg_hash = "", 
			  std::string status_subscriber_dlg_key = "") 
	: AmASyncRequestUAC(_cmd, status_subscriber_dlg_hash, status_subscriber_dlg_key)
	{ 
	    m_body = body;
	    content_type = content_type_;
	}
};

#endif

// Local Variables:
// mode:C++
// End:
