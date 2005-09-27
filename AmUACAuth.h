/*
 * $Id: AmUACAuth.h,v 1.1.2.1 2005/08/03 21:01:27 sayer Exp $
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

#ifndef _AM_UAC_AUTH_H
#define _AM_UAC_AUTH_H

#include "AmCmd.h"

#include <string>

#define HASHLEN 16
typedef unsigned char HASH[HASHLEN];

#define HASHHEXLEN 32
typedef unsigned char HASHHEX[HASHHEXLEN+1];

struct UACAuthDigestChallenge {
  std::string domain;
  std::string realm;
  std::string qop;

  std::string nonce;
  std::string opaque;
  bool stale;
  std::string algorithm;
};

class UACAuthCredential {
  std::string user;
  std::string realm;
  std::string pwd;

  unsigned int flags;

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
 public:

  UACAuthCredential(const std::string& user, const std::string& realm, const std::string& pwd) 
    : user(user), realm(realm), pwd(pwd) { }

  /** 
   *  do auth on cmd with nonce in auth_hdr if possible 
   *  @return true if successful 
   */
  bool do_auth(AmCmd& cmd, const unsigned int code, const string& auth_hdr);
};


#endif

