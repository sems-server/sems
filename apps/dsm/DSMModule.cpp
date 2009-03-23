/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#include "DSMModule.h"
#include "DSMSession.h"
#include "AmSession.h"

DSMModule::DSMModule() {
}

DSMModule::~DSMModule() {
}

SCStrArgAction::SCStrArgAction(const string& m_arg) { 
  arg = trim(m_arg, " \t");
  if (arg.length() && arg[0] == '"')
    arg = trim(arg, "\"");
  else if (arg.length() && arg[0] == '\'')
    arg = trim(arg, "'");
}

string trim(string const& str,char const* sepSet)
{
  string::size_type const first = str.find_first_not_of(sepSet);
  return ( first==string::npos )
    ? std::string()  : 
    str.substr(first, str.find_last_not_of(sepSet)-first+1);
}

string resolveVars(const string s, AmSession* sess,
		   DSMSession* sc_sess, map<string,string>* event_params) {
  if (s.length()) {
    switch(s[0]) {
    case '$': return sc_sess->var[s.substr(1)];
    case '#': 
      if (event_params) 
	return  (*event_params)[s.substr(1)];
      else 
	return string();
    case '@': {
      string s1 = s.substr(1); 
      if (s1 == "local_tag")
	return sess->getLocalTag();	
      else if (s1 == "user")
	return sess->dlg.user;
      else if (s1 == "domain")
	return sess->dlg.domain;
      else if (s1 == "remote_tag")
	return sess->getRemoteTag();
      else if (s1 == "callid")
	return sess->getCallID();
      else if (s1 == "local_uri")
	return sess->dlg.local_uri;
      else if (s1 == "remote_uri")
	return sess->dlg.remote_uri;
      else
	return string();
    } 
    default: return trim(s, "\"");
    }
  }
  return s;
}
