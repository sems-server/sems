/*
 * Copyright (C) 2013 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "ReplacesMapper.h"
#include "AmUtils.h"
#include "AmUriParser.h"
#include "AmSipHeaders.h"

bool findTag(const string replaces, const string& tag, size_t& p1, size_t& len);

void fixReplaces(string& req_hdrs, bool is_invite) {

  string replaces;
  string refer_to;
  AmUriParser refer_target;
  vector<string> hdrs;                      // headers from Refer-To URI
  vector<string>::iterator replaces_hdr_it; // Replaces header from Refer-To URI

  DBG("Replaces handler: fixing %s request\n", is_invite?"INVITE":"REFER");

  if (is_invite) {
    replaces = getHeader(req_hdrs, SIP_HDR_REPLACES, true);
    if (replaces.empty()) {
      DBG("Replaces handler: no Replaces in INVITE, ignoring\n");
      return;
    }
  } else {
    refer_to = getHeader(req_hdrs, SIP_HDR_REFER_TO, SIP_HDR_REFER_TO_COMPACT, true);
    if (refer_to.empty()) {
      DBG("Replaces handler: empty Refer-To header, ignoring\n");
      return;
    }

    size_t pos=0; size_t end=0;
    if (!refer_target.parse_contact(refer_to, pos, end)) {
      DBG("Replaces handler: unable to parse Refer-To name-addr, ignoring\n");
      return;
    }

    if (refer_target.uri_headers.empty()) {
      DBG("Replaces handler: no headers in Refer-To target, ignoring\n");
      return;
    }

    hdrs = explode(refer_target.uri_headers, ";");
    for (replaces_hdr_it=hdrs.begin(); replaces_hdr_it != hdrs.end(); replaces_hdr_it++) {

      string s = URL_decode(*replaces_hdr_it);
      const char* Replaces_str = "Replaces";
      if ((s.length() >= 8) &&
	  !strncmp(Replaces_str, s.c_str(), 8)) {
	size_t pos = 8;
	while (s.length()>pos && (s[pos] == ' ' || s[pos] == '\t')) pos++;
	if (s[pos] != '=')
	  continue;
	pos++;
	while (s.length()>pos && (s[pos] == ' ' || s[pos] == '\t')) pos++;
	replaces = s.substr(pos);
	break;
      }
    }
    
    if (replaces_hdr_it == hdrs.end()) {
      DBG("Replaces handler: no Replaces headers in Refer-To target, ignoring\n");
      return;
    }
  }

  DBG("Replaces found: '%s'\n", replaces.c_str());
  size_t ftag_begin; size_t ftag_len;
  size_t ttag_begin; size_t ttag_len;
  size_t cid_len=0;
 
  // todo: parse full replaces header and reconstruct including unknown params
  if (!findTag(replaces, "from-tag=", ftag_begin, ftag_len)) {
    WARN("Replaces missing 'from-tag', ignoring\n");
    return;
  }

  if (!findTag(replaces, "to-tag=", ttag_begin, ttag_len)) {
    WARN("Replaces missing 'to-tag', ignoring\n");
    return;
  }
  while (cid_len < replaces.size() && replaces[cid_len] != ';')
    cid_len++;

  string ftag = replaces.substr(ftag_begin, ftag_len);
  string ttag = replaces.substr(ttag_begin, ttag_len);
  string callid = replaces.substr(0, cid_len);
  bool early_only = replaces.find("early-only") != string::npos;

  DBG("Replaces handler: found callid='%s', ftag='%s', ttag='%s'\n",
      callid.c_str(), ftag.c_str(), ttag.c_str());

  SBCCallRegistryEntry other_dlg;
  if (SBCCallRegistry::lookupCall(ttag, other_dlg)) {
    replaces = other_dlg.callid+
      ";from-tag="+other_dlg.ltag+";to-tag="+other_dlg.rtag;
    if (early_only)
      replaces += ";early_only";
    DBG("Replaces handler: mapped Replaces to: '%s'\n", replaces.c_str());

    if (is_invite) {
      removeHeader(req_hdrs, SIP_HDR_REPLACES);
      req_hdrs+=SIP_HDR_COLSP(SIP_HDR_REPLACES)+replaces+CRLF;
    } else {
      string replaces_enc = SIP_HDR_REPLACES "="+URL_encode(replaces);
      string new_hdrs;
      for (vector<string>::iterator it = hdrs.begin(); it != hdrs.end(); it++) {
	if (it != hdrs.begin())
	  new_hdrs+=";";

	if (it != replaces_hdr_it) {
	  // different hdr, just add it
	  new_hdrs+=*it;
	} else {
	  //reconstructed replaces hdr
	  new_hdrs+=replaces_enc;
	}
      }
      refer_target.uri_headers=new_hdrs;
      removeHeader(req_hdrs, SIP_HDR_REFER_TO);
      removeHeader(req_hdrs, SIP_HDR_REFER_TO_COMPACT);
      req_hdrs+=SIP_HDR_COLSP(SIP_HDR_REFER_TO)+refer_target.nameaddr_str()+CRLF;
    }

  } else {
    DBG("Replaces handler: call with tag '%s' not found\n", ttag.c_str());
  }

 
}

bool findTag(const string replaces, const string& tag, size_t& p1, size_t& len)
{
  size_t i = replaces.find(tag);
  if (i == string::npos) return false;

  p1 = i+tag.length();
  size_t j = replaces.find(';', p1);
  
  if (j != string::npos) {
    len = j - p1;
  } else {
    len = replaces.size() - i;
  }
  return true;
}

