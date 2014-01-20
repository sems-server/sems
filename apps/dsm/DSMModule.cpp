/*
 * Copyright (C) 2008 iptego GmbH
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

#include <sstream>
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

bool isNumber(const std::string& s) {
  if (s.empty())
    return false;

  for (string::size_type i = 0; i < s.length(); i++) {
    if (!std::isdigit(s[i]))
      return false;
  }
  return true;
}

string resolveVars(const string ts, AmSession* sess,
		   DSMSession* sc_sess, map<string,string>* event_params,
		   bool eval_ops) {
  string s = ts;
  if (s.length()) {

    if(eval_ops) {
      // remove all spaces
      string::size_type p;
      for (p = s.find (" ", 0 ); 
  	p != string::npos; p = s.find(" ", p)) {
        s.erase (p, 1);
      }

      // evaluate operators
      string a,b;
      if((p = s.find("-")) != string::npos) {
        a = resolveVars(s.substr(0, p), sess, sc_sess, event_params, true);
        b = resolveVars(s.substr(p+1, string::npos), sess, sc_sess, event_params, true);
        if(isNumber(a) && isNumber(b)) {
          std::stringstream res; res << atoi(a.c_str()) - atoi(b.c_str());
          return res.str();
        }
      }
      else if((p = s.find("+")) != string::npos) {
        a = resolveVars(s.substr(0, p), sess, sc_sess, event_params, true);
        b = resolveVars(s.substr(p+1, string::npos), sess, sc_sess, event_params, true);
        if(isNumber(a) && isNumber(b)) {
          std::stringstream res; res << atoi(a.c_str()) + atoi(b.c_str());
          return res.str();
        }
      }
    }

    switch(s[0]) {
    case '$': {
      if (s.substr(1, 1)=="$")
	return "$";
      map<string, string>::iterator it = sc_sess->var.find(s.substr(1));
      if (it != sc_sess->var.end())
	return it->second;
      return "";
    }
    case '#': 
      if (s.substr(1, 1)=="#")
	return "#";
      if (event_params) {
	map<string, string>::iterator it = event_params->find(s.substr(1));
	if (it != event_params->end())
	  return it->second;
	return  "";
      }else 
	return string();
    case '@': {
      if (s.substr(1, 1)=="@")
	return "@";
      if (s.length() < 2)
	return "@";

      string s1 = s.substr(1); 
      if (s1 == "local_tag")
	return sess->getLocalTag();	
      else if (s1 == "user")
	return sess->dlg->getUser();
      else if (s1 == "domain")
	return sess->dlg->getDomain();
      else if (s1 == "remote_tag")
	return sess->getRemoteTag();
      else if (s1 == "callid")
	return sess->getCallID();
      else if (s1 == "local_uri")
	return sess->dlg->getLocalUri();
      else if (s1 == "local_party")
	return sess->dlg->getLocalParty();
      else if (s1 == "remote_uri")
	return sess->dlg->getRemoteUri();
      else if (s1 == "remote_party")
	return sess->dlg->getRemoteParty();
      else
	return string();
    } 
    default: return trim(s, "\"");
    }
  }
  return s;
}

void splitCmd(const string& from_str, 
			    string& cmd, string& params) {
  size_t b_pos = from_str.find('(');
  if (b_pos != string::npos) {
    cmd = from_str.substr(0, b_pos);
    params = from_str.substr(b_pos + 1, from_str.rfind(')') - b_pos -1);
  } else 
    cmd = from_str;  
}
