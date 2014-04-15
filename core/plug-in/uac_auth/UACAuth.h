/*
 * Copyright (C) 2006 iptego GmbH
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

#ifndef UACAuth_h
#define UACAuth_h

#include "AmApi.h"
#include "AmSession.h"
#include "AmOfferAnswer.h"
#include "ampi/UACAuthAPI.h"
#include "AmUtils.h"

#include <string>
using std::string;
#include <map>

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
  AmSessionEventHandler* getHandler(AmBasicSipDialog* dlg, 
				    CredentialHolder* s);
 public:
  UACAuthFactory(const string& name)
    : AmSessionEventHandlerFactory(name),
    AmDynInvokeFactory(name)
    { }
	
  int onLoad();

  // SessionEventHandler API
  AmSessionEventHandler* getHandler(AmSession* s);
  bool onInvite(const AmSipRequest& req, AmConfigReader& conf);

  static UACAuthFactory* instance();
  AmDynInvoke* getInstance() { return instance(); }
  void invoke(const string& method, const AmArg& args, AmArg& ret);
};

/** \brief contains necessary information for UAC auth of a SIP request */
struct SIPRequestInfo {
  string method;
  AmMimeBody body;
  string hdrs;
  //AmOfferAnswer::OAState oa_state;

  SIPRequestInfo(const string& method, 
		 const AmMimeBody* body,
		 const string& hdrs
		 )
    : method(method), hdrs(hdrs)
  {
    if(body) this->body = *body;
  }

  SIPRequestInfo() {}

};

/** \brief SessionEventHandler for implementing uac authentication */
class UACAuth : public AmSessionEventHandler
{
  static string server_nonce_secret;

  std::map<unsigned int, SIPRequestInfo> sent_requests;

  UACAuthCred* credential;
  AmBasicSipDialog* dlg;

  UACAuthDigestChallenge challenge;
  unsigned int challenge_code;

  string nonce; // last nonce received from server
  unsigned int nonce_count;

  bool nonce_reuse; // reused nonce?
 
  static std::string find_attribute(const std::string& name, const std::string& header);
  static bool parse_header(const std::string& auth_hdr, UACAuthDigestChallenge& challenge);

  static void uac_calc_HA1(const UACAuthDigestChallenge& challenge,
			   const UACAuthCred* _credential,
			   std::string cnonce,
			   HASHHEX sess_key);

  static void uac_calc_HA2( const std::string& method, const std::string& uri,
			    const UACAuthDigestChallenge& challenge,
			    HASHHEX hentity,
			    HASHHEX HA2Hex );

  static void uac_calc_hentity( const std::string& body, HASHHEX hentity );
	
  static void uac_calc_response( HASHHEX ha1, HASHHEX ha2,
				 const UACAuthDigestChallenge& challenge,
				 const std::string& cnonce, const string& qop_value,
				 unsigned int nonce_count, 
				 HASHHEX response);
	
  /**
   *  do auth on cmd with nonce in auth_hdr if possible
   *  @return true if successful
   */
  bool do_auth(const unsigned int code, const string& auth_hdr,  
	       const string& method, const string& uri, 
	       const AmMimeBody* body, string& result);

  /**
   *  do auth on cmd with saved challenge
   *  @return true if successful
   */
  bool do_auth(const UACAuthDigestChallenge& challenge,
	       const unsigned int code,
	       const string& method, const string& uri,
	       const AmMimeBody* body, string& result);
	
 public:
	
  UACAuth(AmBasicSipDialog* dlg, UACAuthCred* cred);
  virtual ~UACAuth(){ }
  
  /* SEH Hooks @see AmSessionEventHandler */
  virtual bool process(AmEvent*);
  virtual bool onSipEvent(AmSipEvent*);
  virtual bool onSipRequest(const AmSipRequest&);
  virtual bool onSipReply(const AmSipRequest&, const AmSipReply&, 
			  AmBasicSipDialog::Status old_status);
  virtual bool onSendRequest(AmSipRequest& req, int& flags);
  virtual bool onSendReply(const AmSipRequest& req, AmSipReply& reply, int& flags);

  static string calcNonce();
  static bool checkNonce(const string& nonce);
  static void checkAuthentication(const AmSipRequest* req, const string& realm,
				  const string& user, const string& pwd, AmArg& ret);

  static void setServerSecret(const string& secret);

  /** time-constant string compare function (but leaks timing of length mismatch)
      @return true if matching */
  static bool tc_isequal(const std::string& s1, const std::string& s2);
  /** time-constant string compare function @return true if matching */
  static bool tc_isequal(const char* s1, const char* s2, size_t len);

};


#endif
