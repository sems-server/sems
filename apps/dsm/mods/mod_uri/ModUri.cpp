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
#include "ModUri.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "DSMCoreModule.h"
#include "AmUriParser.h"

SC_EXPORT(URIModule);

URIModule::URIModule() {
}

URIModule::~URIModule() {
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

DSMAction* URIModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

#define DEF_CMD(cmd_name, class_name) \
				      \
  if (cmd == cmd_name) {	      \
    class_name * a =		      \
      new class_name(params);	      \
    a->name = from_str;		      \
    return a;			      \
  }

  DEF_CMD("uri.parse", URIParseAction);
  DEF_CMD("uri.getHeader", URIGetHeaderAction);

  return NULL;
}

DSMCondition* URIModule::getCondition(const string& from_str) {
  return NULL;
}

#define GET_SCSESSION()					 \
  DSMSession* sc_sess = dynamic_cast<DSMSession*>(sess); \
  if (!sc_sess) {					 \
    ERROR("wrong session type\n");			 \
    return false;					 \
  }

CONST_ACTION_2P(URIParseAction, ',', true);

bool URIParseAction::execute(AmSession* sess, 
			     DSMCondition::EventType event,
			     map<string,string>* event_params) {
  GET_SCSESSION();

  string uri = resolveVars(par1, sess, sc_sess, event_params);
  string prefix = resolveVars(par2, sess, sc_sess, event_params);

  AmUriParser p;
  p.uri = uri;
  if (!p.parse_uri()) {
    DBG("parsing URI '%s' failed\n", uri.c_str());
    return false;
  }
  
  sc_sess->var[prefix+"display_name"] = p.display_name;
  sc_sess->var[prefix+"user"]         = p.uri_user;
  sc_sess->var[prefix+"host"]         = p.uri_host;
  sc_sess->var[prefix+"param"]        = p.uri_param;

  return false;
}

bool URIModule::onInvite(const AmSipRequest& req, DSMSession* sess) {
  sess->var["hdrs"] = req.hdrs;
  return true;
}

CONST_ACTION_2P(URIGetHeaderAction, ',', false);
bool URIGetHeaderAction::execute(AmSession* sess, 
				 DSMCondition::EventType event,
				 map<string,string>* event_params) {
  GET_SCSESSION();

  string hname  = resolveVars(par1, sess, sc_sess, event_params);
  string dstname = resolveVars(par2, sess, sc_sess, event_params);

  sc_sess->var[dstname] = getHeader(sc_sess->var["hdrs"], hname);  DBG("got header '%s' value '%s' as $%s\n", 
      hname.c_str(), sc_sess->var[dstname].c_str(), dstname.c_str());
  return false;
}
