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

#ifndef _AmCmd_h_
#define _AmCmd_h_

#include <string>
using std::string;

/* definition imported from Ser parser/msg_parser.h */
#define FL_FORCE_ACTIVE 2

/**
 * \brief represents a command received from \ref AmCtrlInterface
 * Structure to store the parameters.
 */
struct AmCmd 
{
    string cmd;      // application plug-in name or 'bye'
    string method;   // SIP method
    string user;     // local user
    //string email;    // local user's email 
    string domain;   // local user's domain
    string dstip;    // local ip
    string port;     // local SIP port
    string r_uri;    // Incoming request only: request uri
    string from_uri; // remote SIP uri
    string from;     // remote SIP address
    string to;       // local SIP address
    string callid;   // Call-Id
    string from_tag; // remote SIP user tag
    string to_tag;   // local SIP user tag
    int    cseq;     // SIP CSeq

    /**
     * Incoming request only:
     *   transaction number needed by Ser's 't_reply'
     */
    string key;

    /**
     * Pre-calculated route set
     * for SIP 'Route:' header.
     */
    string route;

    /**
     * Pre-calculated next hop
     * for Ser's 't_uac_dlg'.
     */
    string next_hop;

    /**
     * Additional headers
     */
    string hdrs;
    string getHeader(const string& hdr_name) const;
  
    /** remove  a header @returns whether header existed */
    bool stripHeader(const string& hdr_name);

    /** find a header, 
     if found, value is between pos1 and pos2 
     and hdr start is the start of the header 
     @return true if found */
    bool findHeader(const string& hdr_name, 
	    size_t& pos1, size_t& pos2, size_t& hdr_start) const;

    /** 
     * add a header. Caution: this does not check for 
     *  duplicates. 
     */
    void addHeader(const string& hdr);
};

#endif

// Local Variables:
// mode:C++
// End:
