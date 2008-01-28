/*
 * $Id$
 *
 * Copyright (C) 2006 iptego GmbH
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

#ifndef UACAuth_h
#define UACAuth_h

#include "AmApi.h"
#include "AmSession.h"
#include "ampi/UACAuthAPI.h"

#include <string>
using std::string;
#include <map>

#define HASHLEN 16
typedef unsigned char HASH[HASHLEN];

#define HASHHEXLEN 32
typedef unsigned char HASHHEX[HASHHEXLEN+1];

/** \brief Challenge in uac auth */
struct UACAuthDigestChallenge {
  std::string realm;
  std::string qop;

  std::string nonce;
  std::string opaque;
  bool stale;
  std::string algorithm;
};

/** \brief factory for uac_auth session event handlers */
class UACAuthFactory
: public AmSessionEventHandlerFactory,
  public AmDynInvokeFactory,
  public AmDynInvoke
{
  static UACAuthFactory* _instance;
  AmSessionEventHandler* getHandler(AmSipDialog* dlg, 
				    CredentialHolder* s);
 public:
  UACAuthFactory(const string& name)
    : AmSessionEventHandlerFactory(name),
    AmDynInvokeFactory(name)
    { }
	
  int onLoad();

  // SessionEventHandler API
  AmSessionEventHandler* getHandler(AmSession* s);
  bool onInvite(const AmSipRequest&);

  static UACAuthFactory* instance();
  AmDynInvoke* getInstance() { return instance(); }
  void invoke(const string& method, const AmArg& args, AmArg& ret);
};

/** \brief contains necessary information for UAC auth of a SIP request */
struct SIPRequestInfo {
  string method;
  string content_type;
  string body;
  string hdrs;
	
  SIPRequestInfo(const string& method, 
		 const string& content_type,
		 const string& body,
		 const string& hdrs)
    : method(method), content_type(content_type),
       body(body), hdrs(hdrs) { }

  SIPRequestInfo() {}

};

/** \brief SessionEventHandler for implementing uac authentication */
class UACAuth : public AmSessionEventHandler
{
  std::map<unsigned int, SIPRequestInfo> sent_requests;

  UACAuthCred* credential;
  AmSipDialog* dlg;

  std::string find_attribute(const std::string& name, const std::string& header);
  bool parse_header(const std::string& auth_hdr, UACAuthDigestChallenge& challenge);

  void uac_calc_HA1(UACAuthDigestChallenge& challenge,
		    std::string cnonce,
		    HASHHEX sess_key);

  void uac_calc_HA2( const std::string& method, const std::string& uri,
		     UACAuthDigestChallenge& challenge,
		     HASHHEX hentity,
		     HASHHEX HA2Hex );
	
  void uac_calc_response( HASHHEX ha1, HASHHEX ha2,
			  UACAuthDigestChallenge& challenge,
			  const std::string& nc, const std::string& cnonce,
			  HASHHEX response);
	
  /** 
   *  do auth on cmd with nonce in auth_hdr if possible 
   *  @return true if successful 
   */
  bool do_auth(const unsigned int code, const string& auth_hdr,  
	       const string& method, const string& uri, string& result);
	
 public:
	
  UACAuth(AmSipDialog* dlg, UACAuthCred* cred);
  virtual ~UACAuth(){ }
  
  /* SEH Hooks @see AmSessionEventHandler */
  virtual bool process(AmEvent*);
  virtual bool onSipEvent(AmSipEvent*);
  virtual bool onSipRequest(const AmSipRequest&);
  virtual bool onSipReply(const AmSipReply&);
	
  virtual bool onSendRequest(const string& method, 
			     const string& content_type,
			     const string& body,
			     string& hdrs,
			     unsigned int cseq);
	
  virtual bool onSendReply(const AmSipRequest& req,
			   unsigned int  code,
			   const string& reason,
			   const string& content_type,
			   const string& body,
			   string& hdrs);
};


#endif
