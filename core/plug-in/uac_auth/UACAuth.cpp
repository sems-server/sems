/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "UACAuth.h"
#include "AmSipMsg.h"
#include "AmUtils.h"
#include "AmSipHeaders.h"

#include <map>

#include <cctype>
#include <algorithm>

#include "md5.h"

using std::string;


#define MOD_NAME "uac_auth"

EXPORT_SESSION_EVENT_HANDLER_FACTORY(UACAuthFactory, MOD_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(UACAuthFactory, MOD_NAME);

UACAuthFactory* UACAuthFactory::_instance=0;
string UACAuth::server_nonce_secret = "CKASLD§$>NLKJSLDKFJ"; // replaced on load

UACAuthFactory* UACAuthFactory::instance()
{
  if(!_instance)
    _instance = new UACAuthFactory(MOD_NAME);
  return _instance;
}

void UACAuthFactory::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  if (method == "getHandler") {
    CredentialHolder* c = dynamic_cast<CredentialHolder*>(args.get(0).asObject());
    DialogControl* cc = dynamic_cast<DialogControl*>(args.get(1).asObject());

    if ((c!=NULL)&&(cc!=NULL)) {
      AmArg handler;
      handler.setBorrowedPointer(getHandler(cc->getDlg(), c));
      ret.push(handler);
    } else {
      ERROR("wrong types in call to getHandler.  (c=%ld, cc= %ld)\n", 
	    (unsigned long)c, (unsigned long)cc);
    }
  } else if (method == "checkAuth") {

    // params: Request realm user pwd
    if (args.size() < 4) {
      ERROR("missing arguments to uac_auth checkAuth function, expected Request realm user pwd\n");
      throw AmArg::TypeMismatchException();
    }

    AmSipRequest* req = dynamic_cast<AmSipRequest*>(args.get(0).asObject());
    if (NULL == req)
      throw AmArg::TypeMismatchException();
    UACAuth::checkAuthentication(req, args.get(1).asCStr(),
				 args.get(2).asCStr(),
				 args.get(3).asCStr(), ret);
  } else 
    throw AmDynInvoke::NotImplemented(method);
}


int UACAuthFactory::onLoad()
{
  string secret;
  AmConfigReader conf;
  string cfg_file_path = AmConfig::ModConfigPath + "uac_auth.conf";
  if(conf.loadFile(cfg_file_path)){
    WARN("Could not open '%s', assuming that default values are fine\n",
	 cfg_file_path.c_str());
    secret = AmSession::getNewId(); // ?? TODO: is this cryptoproof?
  } else {
    secret = conf.getParameter("server_secret");
    if (secret.size()<5) {
      ERROR("server_secret in '%s' too short!\n", cfg_file_path.c_str());
      return -1;
    }
  }

  UACAuth::setServerSecret(secret);
  return 0;
}

bool UACAuthFactory::onInvite(const AmSipRequest& req, AmConfigReader& conf)
{
  return true;
}

AmSessionEventHandler* UACAuthFactory::getHandler(AmSession* s)
{
  CredentialHolder* c = dynamic_cast<CredentialHolder*>(s);
  if (c != NULL) {
    return getHandler(s->dlg, c);
  } else {
    DBG("no credentials for new session. not enabling auth session handler.\n");
  }

  return NULL;
}

AmSessionEventHandler* UACAuthFactory::getHandler(AmBasicSipDialog* dlg, 
						  CredentialHolder* c) {
  return new UACAuth(dlg, c->getCredentials());
}

UACAuth::UACAuth(AmBasicSipDialog* dlg,
		 UACAuthCred* cred)
  : AmSessionEventHandler(),
    credential(cred),
    dlg(dlg),
    nonce_count(0),
    nonce_reuse(false)
{
}

bool UACAuth::process(AmEvent* ev)
{
  return false;
}

bool UACAuth::onSipEvent(AmSipEvent* ev)
{
  return false;
}

bool UACAuth::onSipRequest(const AmSipRequest& req)
{   
  return false;
}

bool UACAuth::onSipReply(const AmSipRequest& req, const AmSipReply& reply, 
			 AmBasicSipDialog::Status old_dlg_status)
{
  bool processed = false;
  if (reply.code==407 || reply.code==401) {
    DBG("SIP reply with code %d cseq %d .\n", reply.code, reply.cseq);
		
    std::map<unsigned int, SIPRequestInfo>::iterator ri = 
      sent_requests.find(reply.cseq);
    if (ri!= sent_requests.end())
      {
	DBG(" UACAuth - processing with reply code %d \n", reply.code);
	// 			DBG("realm %s user %s pwd %s ----------------\n", 
	// 				credential->realm.c_str(),
	// 				credential->user.c_str(),
	// 				credential->pwd.c_str());
	if (!nonce_reuse &&
	    (((reply.code == 401) &&
	     getHeader(ri->second.hdrs, SIP_HDR_AUTHORIZATION, true).length()) ||
	    ((reply.code == 407) && 
	     getHeader(ri->second.hdrs, SIP_HDR_PROXY_AUTHORIZATION, true).length()))) {
	  DBG("Authorization failed!\n");
	} else {
	  nonce_reuse = false;

	  string auth_hdr = (reply.code==407) ? 
	    getHeader(reply.hdrs, SIP_HDR_PROXY_AUTHENTICATE, true) : 
	    getHeader(reply.hdrs, SIP_HDR_WWW_AUTHENTICATE, true);
	  string result; 

	  string auth_uri; 
	  auth_uri = dlg->getRemoteUri();

	  if (do_auth(reply.code, auth_hdr,  
		      ri->second.method,
		      auth_uri, &(ri->second.body), result)) {
	    string hdrs = ri->second.hdrs;

	    // strip other auth headers
	    if (reply.code == 401) {
	      removeHeader(hdrs, SIP_HDR_AUTHORIZATION);
	    } else {
	      removeHeader(hdrs, SIP_HDR_PROXY_AUTHORIZATION);
	    }

	    if (hdrs == "\r\n" || hdrs == "\r" || hdrs == "\n")
	      hdrs = result;
	    else
	      hdrs += result;

	    if (dlg->getStatus() < AmSipDialog::Connected && 
		ri->second.method != SIP_METH_BYE) {
	      // reset remote tag so remote party 
	      // thinks its new dlg
	      dlg->setRemoteTag(string());

	      if (AmConfig::ProxyStickyAuth) {
		// update remote URI to resolved IP
		size_t hpos = dlg->getRemoteUri().find("@");
		if (hpos != string::npos && reply.remote_ip.length()) {
		  string remote_uri = dlg->getRemoteUri().substr(0, hpos+1) 
		    + reply.remote_ip + ":"+int2str(reply.remote_port);
		  dlg->setRemoteUri(remote_uri);
		  DBG("updated remote URI to '%s'\n", remote_uri.c_str());
		}
	      }

	    }

	    int flags = SIP_FLAGS_VERBATIM | SIP_FLAGS_NOAUTH;
	    size_t skip = 0, pos1, pos2, hdr_start;
	    if (findHeader(hdrs, SIP_HDR_CONTACT, skip, pos1, pos2, hdr_start) ||
		findHeader(hdrs, "m", skip, pos1, pos2, hdr_start))
	      flags |= SIP_FLAGS_NOCONTACT;

	    // resend request 
	    if (dlg->sendRequest(ri->second.method,
				 &(ri->second.body),
				 hdrs,  flags) == 0) {
	      processed = true;
              DBG("authenticated request successfully sent.\n");
	      // undo SIP dialog status change
	      if (dlg->getStatus() != old_dlg_status)
	        dlg->setStatus(old_dlg_status);
            } else {
              ERROR("failed to send authenticated request.\n");
            }
	  }
	}
	sent_requests.erase(ri);
      }
  } else if (reply.code >= 200) {
    sent_requests.erase(reply.cseq); // now we dont need it any more
  }
	
  return processed;
}

bool UACAuth::onSendRequest(AmSipRequest& req, int& flags)
{
  // add authentication header if nonce is already there
  string result;
  if (!(flags & SIP_FLAGS_NOAUTH) &&
      !challenge.nonce.empty() &&
      do_auth(challenge, challenge_code,
	      req.method, dlg->getRemoteUri(), &req.body, result)) {
    // add headers
    if (req.hdrs == "\r\n" || req.hdrs == "\r" || req.hdrs == "\n")
      req.hdrs = result;
    else
      req.hdrs += result;

    nonce_reuse = true;
  } else {
    nonce_reuse = false;
  }

  DBG("adding %d to list of sent requests.\n", req.cseq);
  sent_requests[req.cseq] = SIPRequestInfo(req.method, 
					   &req.body,
					   req.hdrs//,
					   // TODO: fix this!!!
					   /*dlg->getOAState()*/);
  return false;
}


bool UACAuth::onSendReply(const AmSipRequest& req, AmSipReply& reply, int& flags)
{
  return false;
}

void w_MD5Update(MD5_CTX *ctx, const string& s) {
  unsigned char a[255];
  if (s.length()>255) {
    ERROR("string too long\n");
    return;
  }
  memcpy(a, s.c_str(), s.length());
  MD5Update(ctx, a, s.length());
}

/** time-constant string compare function, but leaks timing of length mismatch */
bool UACAuth::tc_isequal(const std::string& s1, const std::string& s2) {
  if (s1.length() != s2.length())
    return false;

  bool res = false;

  for (size_t i=0;i<s1.length();i++)
    res |= s1[i]^s2[i];

  return !res;
}

/** time-constant string compare function, but leaks timing of length mismatch */
bool UACAuth::tc_isequal(const char* s1, const char* s2, size_t len) {

  bool res = false;

  for (size_t i=0;i<len;i++)
    res |= s1[i]^s2[i];

  return !res;
}

// supr gly
string UACAuth::find_attribute(const string& name, const string& header) {
  size_t pos1 = header.find(name);

  while (true) {
    if (pos1 == string::npos)
      return "";
    
    if (!pos1 || header[pos1-1] == ',' || header[pos1-1] == ' ')
      break;

    pos1 = header.find(name, pos1+1);
  }

  pos1+=name.length();
  pos1 = header.find_first_not_of(" =\"", pos1);
  if (pos1 != string::npos) {
    size_t pos2 = header.find_first_of(",\"", pos1);
    if (pos2 != string::npos) {
      return header.substr(pos1, pos2-pos1);
    } else {
      return header.substr(pos1); // end of hdr
    }
  }

  return "";
}

bool UACAuth::parse_header(const string& auth_hdr, UACAuthDigestChallenge& challenge) {
  size_t p = auth_hdr.find_first_not_of(' ');
  string method = auth_hdr.substr(p, 6);
  std::transform(method.begin(), method.end(), method.begin(), 
		 (int(*)(int)) toupper);
  if (method != "DIGEST") {
    ERROR("only Digest auth supported\n");
    return false;
  }

  // inefficient parsing...TODO: optimize this
  challenge.realm = find_attribute("realm", auth_hdr);
  challenge.nonce = find_attribute("nonce", auth_hdr);
  challenge.opaque = find_attribute("opaque", auth_hdr);
  challenge.algorithm = find_attribute("algorithm", auth_hdr);
  challenge.qop = find_attribute("qop", auth_hdr);
  
  return (challenge.realm.length() && challenge.nonce.length());
}

bool UACAuth::do_auth(const unsigned int code, const string& auth_hdr,  
		      const string& method, const string& uri,
		      const AmMimeBody* body, string& result)
{
  if (!auth_hdr.length()) {
    ERROR("empty auth header.\n");
    return false;
  }

  if (!parse_header(auth_hdr, challenge)) {
    ERROR("error parsing auth header '%s'\n", auth_hdr.c_str());
    return false;
  }

  challenge_code = code;

  return do_auth(challenge, code, method, uri, body, result);
}


bool UACAuth::do_auth(const UACAuthDigestChallenge& challenge,
		      const unsigned int code,
		      const string& method, const string& uri, 
		      const AmMimeBody* body, string& result) 
{
  if ((challenge.algorithm.length()) && !(challenge.algorithm == "MD5" || challenge.algorithm == "md5" )) {
    DBG("unsupported algorithm: '%s'\n", challenge.algorithm.c_str());
    return false;
  }

  DBG("realm='%s', nonce='%s', qop='%s'\n", 
      challenge.realm.c_str(), 
      challenge.nonce.c_str(),
      challenge.qop.c_str());

  if (credential->realm.length() 
      && (credential->realm != challenge.realm)) {
    DBG("authentication realm mismatch ('%s' vs '%s').\n", 
 	credential->realm.c_str(),challenge.realm.c_str());
  }

  HASHHEX ha1;
  HASHHEX ha2;
  HASHHEX hentity;
  HASHHEX response;
  bool    qop_auth=false;
  bool    qop_auth_int=false;
  string  cnonce;
  string  qop_value;

  if(!challenge.qop.empty()){

    qop_auth = key_in_list(challenge.qop,"auth");
    qop_auth_int = key_in_list(challenge.qop,"auth-int");

    if(qop_auth || qop_auth_int) {

      cnonce = int2hex(get_random(),true);

      if(challenge.nonce == nonce)
	nonce_count++;
      else
	nonce_count = 1;

      if(qop_auth_int){
	string body_str;
	if(body) body->print(body_str);
	uac_calc_hentity(body_str,hentity);
	qop_value = "auth-int";
      }
      else
	qop_value = "auth";
    }
  }

  /* do authentication */
  uac_calc_HA1( challenge, credential, cnonce, ha1);
  uac_calc_HA2( method, uri, challenge, qop_auth_int ? hentity : NULL, ha2);
  uac_calc_response( ha1, ha2, challenge, cnonce, qop_value, nonce_count, response);
  DBG("calculated response = %s\n", response);

  // compile auth response
  result = ((code==401) ? SIP_HDR_COLSP(SIP_HDR_AUTHORIZATION) : 
	    SIP_HDR_COLSP(SIP_HDR_PROXY_AUTHORIZATION));

  result += "Digest username=\"" + credential->user + "\", "
    "realm=\"" + challenge.realm + "\", "
    "nonce=\"" + challenge.nonce + "\", "
    "uri=\"" + uri + "\", ";

  if (challenge.opaque.length())
    result += "opaque=\"" + challenge.opaque + "\", ";
  
  if (!qop_value.empty())
    result += "qop=" + qop_value + ", "
      "cnonce=\"" + cnonce + "\", "
      "nc=" + int2hex(nonce_count,true) + ", ";

  result += "response=\"" + string((char*)response) + "\", algorithm=MD5\n";

  DBG("Auth req hdr: '%s'\n", result.c_str());
  
  return true;
}

/* 
 * calculate H(A1)
 */
void UACAuth::uac_calc_HA1(const UACAuthDigestChallenge& challenge,
			   const UACAuthCred* _credential,
			   string cnonce,
			   HASHHEX sess_key)
{
  if (NULL == _credential)
    return;

  MD5_CTX Md5Ctx;
  HASH HA1;

  MD5Init(&Md5Ctx);
  w_MD5Update(&Md5Ctx, _credential->user);
  w_MD5Update(&Md5Ctx, ":");
  // use realm from challenge 
  w_MD5Update(&Md5Ctx, challenge.realm); 
  w_MD5Update(&Md5Ctx, ":");
  w_MD5Update(&Md5Ctx, _credential->pwd);
  MD5Final(HA1, &Md5Ctx);

  // MD5sess ...not supported
  // 	if ( flags & AUTHENTICATE_MD5SESS )
  // 	  {
  // 		MD5Init(&Md5Ctx);
  // 		MD5Update(&Md5Ctx, HA1, HASHLEN);
  // 		MD5Update(&Md5Ctx, ":", 1);
  // 		MD5Update(&Md5Ctx, challenge.nonce.c_str(), challenge.nonce.length());
  // 		MD5Update(&Md5Ctx, ":", 1);
  // 		MD5Update(&Md5Ctx, cnonce.c_str(), cnonce.length());
  // 		MD5Final(HA1, &Md5Ctx);
  // 	  }; 
  cvt_hex(HA1, sess_key);
}


/* 
 * calculate H(A2)
 */
void UACAuth::uac_calc_HA2( const string& method, const string& uri,
			    const UACAuthDigestChallenge& challenge,
			    HASHHEX hentity,
			    HASHHEX HA2Hex )
{
  unsigned char hc[1]; hc[0]=':';
  MD5_CTX Md5Ctx;
  HASH HA2;
  
  MD5Init(&Md5Ctx);
  w_MD5Update(&Md5Ctx, method);
  MD5Update(&Md5Ctx, hc, 1);
  w_MD5Update(&Md5Ctx, uri);

  if ( hentity != 0 ) 
    {
      MD5Update(&Md5Ctx, hc, 1);
      MD5Update(&Md5Ctx, hentity, HASHHEXLEN);
    };

  MD5Final(HA2, &Md5Ctx);
  cvt_hex(HA2, HA2Hex);
}

/*
 * calculate H(body)
 */

void UACAuth::uac_calc_hentity( const string& body, HASHHEX hentity )
{
  MD5_CTX Md5Ctx;
  HASH    h;

  MD5Init(&Md5Ctx);
  w_MD5Update(&Md5Ctx, body);
  MD5Final(h, &Md5Ctx);
  cvt_hex(h,hentity);
}

/* 
 * calculate request-digest/response-digest as per HTTP Digest spec 
 */
void UACAuth::uac_calc_response(HASHHEX ha1, HASHHEX ha2,
				const UACAuthDigestChallenge& challenge, const string& cnonce,
				const string& qop_value, unsigned int nonce_count, HASHHEX response)
{
  unsigned char hc[1]; hc[0]=':';
  MD5_CTX Md5Ctx;
  HASH RespHash;

  MD5Init(&Md5Ctx);
  MD5Update(&Md5Ctx, ha1, HASHHEXLEN);
  MD5Update(&Md5Ctx, hc, 1);
  w_MD5Update(&Md5Ctx, challenge.nonce);
  MD5Update(&Md5Ctx, hc, 1);


  if (!qop_value.empty()) {
      
    w_MD5Update(&Md5Ctx, int2hex(nonce_count,true));
    MD5Update(&Md5Ctx, hc, 1);
    w_MD5Update(&Md5Ctx, cnonce);
    MD5Update(&Md5Ctx, hc, 1);
    w_MD5Update(&Md5Ctx, qop_value);
    MD5Update(&Md5Ctx, hc, 1);
  };

  MD5Update(&Md5Ctx, ha2, HASHHEXLEN);
  MD5Final(RespHash, &Md5Ctx);
  cvt_hex(RespHash, response);
}

/** calculate nonce: time-stamp H(time-stamp private-key) */
string UACAuth::calcNonce() {
  string result;
  HASHHEX hash;
  MD5_CTX Md5Ctx;
  HASH RespHash;

  time_t now = time(NULL);
  result = int2hex(now);
  
  MD5Init(&Md5Ctx);
  w_MD5Update(&Md5Ctx, result);
  w_MD5Update(&Md5Ctx, server_nonce_secret);
  MD5Final(RespHash, &Md5Ctx);
  cvt_hex(RespHash, hash);

  return result+string((const char*)hash);
}

/** check nonce integrity. @return true if correct */
bool UACAuth::checkNonce(const string& nonce) {
  HASHHEX hash;
  MD5_CTX Md5Ctx;
  HASH RespHash;

#define INT_HEX_LEN int(2*sizeof(int))

  if (nonce.size() != INT_HEX_LEN+HASHHEXLEN) {
    DBG("wrong nonce length (expected %u, got %zd)\n", INT_HEX_LEN+HASHHEXLEN, nonce.size());
    return false;
  }

  MD5Init(&Md5Ctx);
  w_MD5Update(&Md5Ctx, nonce.substr(0,INT_HEX_LEN));
  w_MD5Update(&Md5Ctx, server_nonce_secret);
  MD5Final(RespHash, &Md5Ctx);
  cvt_hex(RespHash, hash);

  return tc_isequal((const char*)hash, &nonce[INT_HEX_LEN], HASHHEXLEN);
}

void UACAuth::setServerSecret(const string& secret) {
  server_nonce_secret = secret;
  DBG("Server Nonce secret set\n");
}

void UACAuth::checkAuthentication(const AmSipRequest* req, const string& realm, const string& user,
				  const string& pwd, AmArg& ret) {
  if (req->method == SIP_METH_ACK || req->method == SIP_METH_CANCEL) {
    DBG("letting pass %s request without authentication\n", req->method.c_str());
    ret.push(200);
    ret.push("OK");
    ret.push("");
    return;
  }

  string auth_hdr = getHeader(req->hdrs, "Authorization");
  bool authenticated = false;

  if (auth_hdr.size()) {
    UACAuthDigestChallenge r_challenge;

    r_challenge.realm = find_attribute("realm", auth_hdr);
    r_challenge.nonce = find_attribute("nonce", auth_hdr);
    r_challenge.qop = find_attribute("qop", auth_hdr);
    string r_response = find_attribute("response", auth_hdr);
    string r_username = find_attribute("username", auth_hdr);
    string r_uri = find_attribute("uri", auth_hdr);
    string r_cnonce = find_attribute("cnonce", auth_hdr);

    DBG("got realm '%s' nonce '%s', qop '%s', response '%s', username '%s' uri '%s' cnonce '%s'\n",
	r_challenge.realm.c_str(), r_challenge.nonce.c_str(), r_challenge.qop.c_str(),
	r_response.c_str(), r_username.c_str(), r_uri.c_str(), r_cnonce.c_str() );

    if (r_response.size() != HASHHEXLEN) {
      DBG("Auth: response length mismatch (wanted %u hex chars): '%s'\n", HASHHEXLEN, r_response.c_str());
      goto auth_end;
    }

    if (realm != r_challenge.realm) {
      DBG("Auth: realm mismatch: required '%s' vs '%s'\n", realm.c_str(), r_challenge.realm.c_str());
      goto auth_end;
    }

    if (user != r_username) {
      DBG("Auth: user mismatch: '%s' vs '%s'\n", user.c_str(), r_username.c_str());
      goto auth_end;
    }

    if (!checkNonce(r_challenge.nonce)) {
      DBG("Auth: incorrect nonce '%s'\n", r_challenge.nonce.c_str());
      goto auth_end;
    }

    // we don't check the URI
    // if (r_uri != req->r_uri) {
    //   DBG("Auth: incorrect URI in request: '%s'\n", r_challenge.nonce.c_str());
    //   goto auth_end;
    // }

    UACAuthCred credential;
    credential.user = user;
    credential.pwd = pwd;

    unsigned int client_nonce_count = 1;

    HASHHEX ha1;
    HASHHEX ha2;
    HASHHEX hentity;
    HASHHEX response;
    bool    qop_auth=false;
    bool    qop_auth_int=false;
    string  qop_value;

    if(!r_challenge.qop.empty()){

      if (r_challenge.qop == "auth")
	qop_auth = true;
      else if (r_challenge.qop == "auth-int")
	qop_auth_int = true;

      if(qop_auth || qop_auth_int) {

	// get nonce count from request
	string nonce_count_str = find_attribute("nc", auth_hdr);
	if (str2i(nonce_count_str, client_nonce_count)) {
	  DBG("Error parsing nonce_count '%s'\n", nonce_count_str.c_str());
	  goto auth_end;
	}

	DBG("got client_nonce_count %u\n", client_nonce_count);

	// auth-int? calculate hentity
	if(qop_auth_int){
	  string body_str;
	  if(!req->body.empty()) req->body.print(body_str);
	  uac_calc_hentity(body_str, hentity);
	  qop_value = "auth-int";
	} else {
	  qop_value = "auth";
	}

      }
    }

    uac_calc_HA1(r_challenge, &credential, r_cnonce, ha1);
    uac_calc_HA2(req->method, r_uri, r_challenge, qop_auth_int ? hentity : NULL, ha2);
    uac_calc_response( ha1, ha2, r_challenge, r_cnonce, qop_value, client_nonce_count, response);
    DBG("calculated our response vs request: '%s' vs '%s'", response, r_response.c_str());

    if (tc_isequal((const char*)response, r_response.c_str(), HASHHEXLEN)) {
      DBG("Auth: authentication successfull\n");
      authenticated = true;
    } else {
      DBG("Auth: authentication NOT successfull\n");
    }
  }

 auth_end: 
  if (authenticated) {
    ret.push(200);
    ret.push("OK");
    ret.push("");
  } else {
    ret.push(401);
    ret.push("Unauthorized");
    ret.push(SIP_HDR_COLSP(SIP_HDR_WWW_AUTHENTICATE) "Digest "
	     "realm=\""+realm+"\", "
	     "qop=\"auth,auth-int\", "
	     "nonce=\""+calcNonce()+"\"\r\n");
  }
}
