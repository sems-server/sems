/*
 * $Id: AmUACAuth.cpp,v 1.1.2.1 2005/08/03 21:01:27 sayer Exp $
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

/*
 * This file contains some code stolen from ser's uac module 
 * by Ramona which is (c) 2005 Voice Sistem
 */

#include "AmUACAuth.h"
#include "log.h"
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


string UACAuthCredential::find_attribute(const string& name, const string& header) {
  string res;
  size_t pos1 = header.find(name);
  if (pos1!=string::npos) {
    pos1+=name.length();
    pos1 = header.find_first_not_of(" =\"", pos1);
    if (pos1 != string::npos) {
      size_t pos2 = header.find("\"", pos1);
      if (pos2 != string::npos) {
	res = header.substr(pos1, pos2-pos1);
      }
    }
  }
  return res;
}

bool UACAuthCredential::parse_header(const string& auth_hdr, UACAuthDigestChallenge& challenge) {
  size_t p = auth_hdr.find_first_not_of(' ');
  if (auth_hdr.substr(p, 6) != "Digest") {
    ERROR("only Digest auth supported\n");
    return false;
  }

  // inefficient parsing...TODO: optimize this
  challenge.realm = find_attribute("realm", auth_hdr);
  challenge.domain = find_attribute("domain", auth_hdr);
  challenge.nonce = find_attribute("nonce", auth_hdr);
  challenge.opaque = find_attribute("opaque", auth_hdr);
  challenge.algorithm = find_attribute("algorithm", auth_hdr);
  challenge.qop = find_attribute("qop", auth_hdr);
  return (challenge.realm.length() && challenge.nonce.length());
}

bool UACAuthCredential::do_auth(AmCmd& cmd, const unsigned int code, const string& auth_hdr) {
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

  DBG("realm='%s', domain='%s', nonce='%s'\n", challenge.realm.c_str(), 
      challenge.domain.c_str(), challenge.nonce.c_str());

  if (realm != challenge.realm) {
    DBG("realm mismatch ('%s' vs '%s'). auth failed.\n", realm.c_str(),challenge.realm.c_str());
    return false;
  }
 
  HASHHEX ha1;
  HASHHEX ha2;
  HASHHEX response;

  /* do authentication */
  uac_calc_HA1( challenge, ""/*cnonce*/, ha1);
  uac_calc_HA2( cmd.method, cmd.from_uri, challenge, 0/*hentity*/, ha2);
  uac_calc_response( ha1, ha2, challenge, ""/*nc*/, "" /*cnonce*/, response);
  DBG("calculated response = %s\n", response);

  // compile auth response
  string r_hdr = (code==401) ? "Authorization: Digest username=\"" : 
    "Proxy-Authorization: Digest username=\"";
  r_hdr+=user + "\", realm=\"" + realm + "\", nonce=\""+challenge.nonce + 
    "\", uri=\""+cmd.from_uri+"\", ";
  if (challenge.opaque.length())
    r_hdr+="opaque=\""+challenge.opaque+"\", ";
  
  r_hdr+="response=\""+string((char*)response)+"\", algorithm=\"MD5\"\n";

  DBG("Auth req hdr: '%s'\n", r_hdr.c_str());
  
  // remove previous auth headers (if any)
  if (code == 401)
    cmd.stripHeader("Authorization");
  else 
    cmd.stripHeader("Proxy-Authorization");
  // add auth header
  cmd.addHeader(r_hdr);
  return true;
}

// These functions come basically from the uac module 
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
void UACAuthCredential::uac_calc_HA1(UACAuthDigestChallenge& challenge,
				     string cnonce,
				     HASHHEX sess_key)
{
	MD5_CTX Md5Ctx;
	HASH HA1;

	MD5Init(&Md5Ctx);
	w_MD5Update(&Md5Ctx, user);
	w_MD5Update(&Md5Ctx, ":");
	w_MD5Update(&Md5Ctx, realm);
	w_MD5Update(&Md5Ctx, ":");
	w_MD5Update(&Md5Ctx, pwd);
	MD5Final(HA1, &Md5Ctx);

	// MD5sess not supported
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
void UACAuthCredential::uac_calc_HA2( const string& method, const string& uri,
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
void UACAuthCredential::uac_calc_response( HASHHEX ha1, HASHHEX ha2,
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

  if (challenge.qop.length())
    {
      
      w_MD5Update(&Md5Ctx, nc);
      MD5Update(&Md5Ctx, hc, 1);
      w_MD5Update(&Md5Ctx, cnonce);
      MD5Update(&Md5Ctx, hc, 1);
      w_MD5Update(&Md5Ctx, challenge.qop);
      MD5Update(&Md5Ctx, hc, 1);
    };
  MD5Update(&Md5Ctx, ha2, HASHHEXLEN);
  MD5Final(RespHash, &Md5Ctx);
  cvt_hex(RespHash, response);
}
