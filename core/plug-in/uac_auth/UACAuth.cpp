/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "UACAuth.h"
#include "AmSipMsg.h"
#include "AmUtils.h"

#include <map>
using std::map;

#define MOD_NAME "uac_auth"

EXPORT_SESSION_EVENT_HANDLER_FACTORY(UACAuthFactory, MOD_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(UACAuthFactory, MOD_NAME);

UACAuthFactory* UACAuthFactory::_instance=0;

UACAuthFactory* UACAuthFactory::instance()
{
  if(!_instance)
    _instance = new UACAuthFactory(MOD_NAME);
  return _instance;
}

void UACAuthFactory::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  if(method == "getHandler"){
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
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}


int UACAuthFactory::onLoad()
{
  return 0;
}

bool UACAuthFactory::onInvite(const AmSipRequest& req)
{
  return false;
}

AmSessionEventHandler* UACAuthFactory::getHandler(AmSession* s)
{
  CredentialHolder* c = dynamic_cast<CredentialHolder*>(s);
  if (c != NULL) {
    return getHandler(&s->dlg, c);
  } else {
    DBG("no credentials for new session. not enabling auth session handler.\n");
  }

  return NULL;
}

AmSessionEventHandler* UACAuthFactory::getHandler(AmSipDialog* dlg, CredentialHolder* c) {
  return new UACAuth(dlg, c->getCredentials());
}

UACAuth::UACAuth(AmSipDialog* dlg, 
		 UACAuthCred* cred)
  : dlg(dlg),
    credential(cred),
    AmSessionEventHandler()
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

bool UACAuth::onSipReply(const AmSipReply& reply)
{
  bool processed = false;
  if (reply.code==407 || reply.code==401) {
    DBG("SIP reply with code %d cseq %d .\n", reply.code, reply.cseq);
		
    map<unsigned int, SIPRequestInfo >::iterator ri = 
      sent_requests.find(reply.cseq);
    if (ri!= sent_requests.end())
      {
	DBG(" UACAuth - processing with reply code %d \n", reply.code);
	// 			DBG("realm %s user %s pwd %s ----------------\n", 
	// 				credential->realm.c_str(),
	// 				credential->user.c_str(),
	// 				credential->pwd.c_str());
	if (((reply.code == 401) && 
	     getHeader(ri->second.hdrs, "Authorization").length()) ||
	    ((reply.code == 407) && 
	     getHeader(ri->second.hdrs, "Proxy-Authorization").length())) {
	  DBG("Authorization failed!\n");
	} else {
	  string auth_hdr = (reply.code==407) ? getHeader(reply.hdrs, "Proxy-Authenticate") : 
	    getHeader(reply.hdrs, "WWW-Authenticate");
	  string result; 
			
	  string auth_uri; 
	  auth_uri = dlg->remote_uri;
				
	  if (do_auth(reply.code, auth_hdr,  
		      ri->second.method,
		      auth_uri, result)) {
	    string hdrs = ri->second.hdrs; 
	    // TODO: strip headers 
	    // ((code==401) ? stripHeader(ri->second.hdrs, "Authorization")  :
	    //	 		    stripHeader(ri->second.hdrs, "Proxy-Authorization"));
	    hdrs += result;

	    if (dlg->getStatus() < AmSipDialog::Connected) {
	      // reset remote tag so remote party 
	      // thinks its new dlg
	      dlg->remote_tag = "";
	    }
	    // resend request 
	    if (dlg->sendRequest(ri->second.method,
				 ri->second.content_type,
				 ri->second.body, 
				 hdrs) == 0) 			
	      processed = true;
	  }
	} 
      }
  }
	
  if (reply.code >= 200)
    sent_requests.erase(reply.cseq); // now we dont need it any more
	
  return processed;
}

bool UACAuth::onSendRequest(const string& method, 
			    const string& content_type,
			    const string& body,
			    string& hdrs,
			    unsigned int cseq)
{
  DBG("adding %d to list of sent requests.\n", cseq);
  sent_requests[cseq] = SIPRequestInfo(method, 
				       content_type,
				       body,
				       hdrs);
  return false;
}


bool UACAuth::onSendReply(const AmSipRequest& req,
			  unsigned int  code,const string& reason,
			  const string& content_type,const string& body,
			  string& hdrs)
{
  return false;
}



#include "md5global.h"

typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

extern "C" void MD5Init  (MD5_CTX * ctx);
extern "C" void MD5Update (MD5_CTX *, unsigned char *, unsigned int);
extern "C" void MD5Final (unsigned char [16], MD5_CTX *);


using std::string;

void w_MD5Update(MD5_CTX *ctx, const string& s) {
  unsigned char a[255];
  if (s.length()>255) {
    ERROR("string too long\n");
    return;
  }
  memcpy(a, s.c_str(), s.length());
  MD5Update(ctx, a, s.length());
}


string UACAuth::find_attribute(const string& name, const string& header) {
  string res;
  size_t pos1 = header.find(name);
  if (pos1!=string::npos) {
    pos1+=name.length();
    pos1 = header.find_first_not_of(" =\"", pos1);
    if (pos1 != string::npos) {
      size_t pos2 = header.find_first_of(",\"", pos1);
      if (pos2 != string::npos) {
	res = header.substr(pos1, pos2-pos1);
      }
    }
  }
  return res;
}

bool UACAuth::parse_header(const string& auth_hdr, UACAuthDigestChallenge& challenge) {
  size_t p = auth_hdr.find_first_not_of(' ');
  if (auth_hdr.substr(p, 6) != "Digest") {
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
		      const string& method, const string& uri, string& result) {
  if (!auth_hdr.length()) {
    ERROR("empty auth header.\n");
    return false;
  }

  UACAuthDigestChallenge challenge;
  if (!parse_header(auth_hdr, challenge)) {
    ERROR("error parsing auth header '%s'\n", auth_hdr.c_str());
    return false;
  }
  
  if ((challenge.algorithm.length()) && (challenge.algorithm != "MD5")) {
    DBG("unsupported algorithm: '%s'\n", challenge.algorithm.c_str());
    return false;
  }

  DBG("realm='%s', nonce='%s'\n", challenge.realm.c_str(), 
      challenge.nonce.c_str());

  if (credential->realm.length() 
      && (credential->realm != challenge.realm)) {
    DBG("realm mismatch ('%s' vs '%s'). auth failed.\n", 
 	credential->realm.c_str(),challenge.realm.c_str());
  }
 
  HASHHEX ha1;
  HASHHEX ha2;
  HASHHEX response;

  /* do authentication */
  uac_calc_HA1( challenge, ""/*cnonce*/, ha1);
  uac_calc_HA2( method, uri, challenge, 0/*hentity*/, ha2);
  uac_calc_response( ha1, ha2, challenge, ""/*nc*/, "" /*cnonce*/, response);
  DBG("calculated response = %s\n", response);

  // compile auth response
  result = ((code==401) ? "Authorization: Digest username=\"" : 
	    "Proxy-Authorization: Digest username=\"")
    + credential->user + "\", realm=\"" + challenge.realm + "\", nonce=\""+challenge.nonce + 
    "\", uri=\""+uri+"\", ";
  if (challenge.opaque.length())
    result+="opaque=\""+challenge.opaque+"\", ";
  
  result+="response=\""+string((char*)response)+"\", algorithm=MD5\n";

  DBG("Auth req hdr: '%s'\n", result.c_str());
  
  return true;
}

// These functions come basically from ser's uac module 
static inline void cvt_hex(HASH bin, HASHHEX hex)
{
  unsigned short i;
  unsigned char j;

  for (i = 0; i<HASHLEN; i++)
    {
      j = (bin[i] >> 4) & 0xf;
      if (j <= 9)
	{
	  hex[i * 2] = (j + '0');
	} else {
	  hex[i * 2] = (j + 'a' - 10);
	}

      j = bin[i] & 0xf;

      if (j <= 9)
	{
	  hex[i * 2 + 1] = (j + '0');
	} else {
	  hex[i * 2 + 1] = (j + 'a' - 10);
	}
    };

  hex[HASHHEXLEN] = '\0';
}


/* 
 * calculate H(A1)
 */
void UACAuth::uac_calc_HA1(UACAuthDigestChallenge& challenge,
			   string cnonce,
			   HASHHEX sess_key)
{
  MD5_CTX Md5Ctx;
  HASH HA1;

  MD5Init(&Md5Ctx);
  w_MD5Update(&Md5Ctx, credential->user);
  w_MD5Update(&Md5Ctx, ":");
  // use realm from challenge 
  w_MD5Update(&Md5Ctx, challenge.realm); 
  w_MD5Update(&Md5Ctx, ":");
  w_MD5Update(&Md5Ctx, credential->pwd);
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
			    UACAuthDigestChallenge& challenge,
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

  if ( challenge.qop == "auth-int" ) 
    {
      MD5Update(&Md5Ctx, hc, 1);
      MD5Update(&Md5Ctx, hentity, HASHHEXLEN);
    };

  MD5Final(HA2, &Md5Ctx);
  cvt_hex(HA2, HA2Hex);
}



/* 
 * calculate request-digest/response-digest as per HTTP Digest spec 
 */
void UACAuth::uac_calc_response( HASHHEX ha1, HASHHEX ha2,
				 UACAuthDigestChallenge& challenge,
				 const string& nc, const string& cnonce,
				 HASHHEX response)
{
  unsigned char hc[1]; hc[0]=':';
  MD5_CTX Md5Ctx;
  HASH RespHash;

  MD5Init(&Md5Ctx);
  MD5Update(&Md5Ctx, ha1, HASHHEXLEN);
  MD5Update(&Md5Ctx, hc, 1);
  w_MD5Update(&Md5Ctx, challenge.nonce);
  MD5Update(&Md5Ctx, hc, 1);

  if (challenge.qop.length()
      && challenge.qop == "auth_int")
    {
      
      w_MD5Update(&Md5Ctx, nc);
      MD5Update(&Md5Ctx, hc, 1);
      w_MD5Update(&Md5Ctx, cnonce);
      MD5Update(&Md5Ctx, hc, 1);
      w_MD5Update(&Md5Ctx, "" /*challenge.qop*/);
      MD5Update(&Md5Ctx, hc, 1);
    };

  MD5Update(&Md5Ctx, ha2, HASHHEXLEN);
  MD5Final(RespHash, &Md5Ctx);
  cvt_hex(RespHash, response);
}
