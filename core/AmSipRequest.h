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
/** @file AmSipRequest.h */
#ifndef AmSipRequest_h
#define AmSipRequest_h

#include "AmEventQueue.h"

#include <string>
using std::string;

/** \brief represents a SIP request */
class AmSipRequest
{
 public:
  string cmd;

  string method;
  string user;
  string domain;
  string dstip; // IP where Ser received the message
  string port;  // Ser's SIP port
  string r_uri;
  string from_uri;
  string from;
  string to;
  string callid;
  string from_tag;
  string to_tag;

  unsigned int cseq;

  string hdrs;
  string body;

  string route;     // record routing
  string next_hop;  // next_hop for t_uac_dlg
    
  string key; // transaction key to be used in t_reply
};

string getHeader(const string& hdrs,const string& hdr_name);

string getHeader(const string& hdrs,const string& hdr_name, 
		 const string& compact_hdr_name);

/** find a header, 
    if found, value is between pos1 and pos2 
    and hdr start is the start of the header 
    @return true if found */
bool findHeader(const string& hdrs,const string& hdr_name, 
		size_t& pos1, size_t& pos2, 
		size_t& hdr_start);

#endif
