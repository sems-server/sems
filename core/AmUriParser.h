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
#ifndef _AmUriParser_h_
#define _AmUriParser_h_

#include <string>
#include <map>
using std::map;
using std::string;

struct AmUriParser {
  string display_name;
  string uri;
	
  string uri_user; 
  string uri_host; 
  string uri_port; 
  string uri_headers;
  string uri_param;		// <sip:user@host;uri_param>
                                // <sip:user;user_param@host>

  map<string, string> params; 	// <sip:user;@host>;params

  bool isEqual(const AmUriParser& c) const;
  /** parse nameaddr from pos
       @return true on success
       @return end of current nameaddr */
  bool parse_contact(const string& line, size_t pos, size_t& end);
  /** parse a name-addr @return true on success */
  bool parse_nameaddr(const string& line);

  /** @return true on success */
  bool parse_uri();
  bool parse_params(const string& line, int& pos);

  /** param_string is semicolon separated list of parameters with or without value.
   * method can be used to add/replace param for uri and user parameters */
  static string add_param_to_param_list(const string& param_name,
	    const string& param_value, const string& param_list);

  void dump() const;
  string uri_str() const;
  string canon_uri_str() const;
  string nameaddr_str() const;
  
  AmUriParser() { }

  string print();
};

#endif
