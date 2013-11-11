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
#include "ModUri.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "AmUriParser.h"

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("uri.parse", URIParseAction);
  DEF_CMD("uri.parseNameaddr", URIParseNameaddrAction);
  DEF_CMD("uri.getHeader", URIGetHeaderAction);

  DEF_CMD("uri.encode", URIEncodeAction);
  DEF_CMD("uri.decode", URIDecodeAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

bool URIModule::onInvite(const AmSipRequest& req, DSMSession* sess) {
  sess->var["hdrs"] = req.hdrs;
  return true;
}

CONST_ACTION_2P(URIParseAction, ',', true);
EXEC_ACTION_START(URIParseAction) {

  string uri = resolveVars(par1, sess, sc_sess, event_params);
  string prefix = resolveVars(par2, sess, sc_sess, event_params);

  AmUriParser p;
  p.uri = uri;
  if (!p.parse_uri()) {
    DBG("parsing URI '%s' failed\n", uri.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("parsing URI '"+uri+"%s' failed");
    return false;
  }
  
  sc_sess->var[prefix+"user"]         = p.uri_user;
  sc_sess->var[prefix+"host"]         = p.uri_host;
  sc_sess->var[prefix+"port"]         = p.uri_port;
  sc_sess->var[prefix+"headers"]      = p.uri_headers;
  sc_sess->var[prefix+"param"]        = p.uri_param;

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

CONST_ACTION_2P(URIParseNameaddrAction, ',', true);
EXEC_ACTION_START(URIParseNameaddrAction) {

  string uri = resolveVars(par1, sess, sc_sess, event_params);
  string prefix = resolveVars(par2, sess, sc_sess, event_params);

  AmUriParser p;
  if (!p.parse_nameaddr(uri)) {
    DBG("parsing nameaddr '%s' failed\n", uri.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("parsing nameaddr '"+uri+"%s' failed");
    return false;
  }
  
  sc_sess->var[prefix+"display_name"] = p.display_name;
  sc_sess->var[prefix+"user"]         = p.uri_user;
  sc_sess->var[prefix+"host"]         = p.uri_host;
  sc_sess->var[prefix+"port"]         = p.uri_port;
  sc_sess->var[prefix+"headers"]      = p.uri_headers;
  sc_sess->var[prefix+"param"]        = p.uri_param;

  for (map<string, string>::iterator it=p.params.begin(); it != p.params.end(); it++) {
    sc_sess->var[prefix+"uri_param."+it->first] = it->second;
  }


  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

CONST_ACTION_2P(URIGetHeaderAction, ',', false);
EXEC_ACTION_START(URIGetHeaderAction) {

  string hname  = resolveVars(par1, sess, sc_sess, event_params);
  string dstname = resolveVars(par2, sess, sc_sess, event_params);

  sc_sess->var[dstname] = getHeader(sc_sess->var["hdrs"], hname, true);  
  DBG("got header '%s' value '%s' as $%s\n", 
      hname.c_str(), sc_sess->var[dstname].c_str(), dstname.c_str());

} EXEC_ACTION_END;


CONST_ACTION_2P(URIEncodeAction, '=', false);
EXEC_ACTION_START(URIEncodeAction) {

  string varname  = par1;
  if (varname.size() && varname[0]=='$')
    varname.erase(0,1);
  string str = resolveVars(par2, sess, sc_sess, event_params);

  sc_sess->var[varname] = URL_encode(str);
  DBG("URL-encoded: $%s=\"%s\"\n", varname.c_str(), sc_sess->var[varname].c_str());
} EXEC_ACTION_END;

CONST_ACTION_2P(URIDecodeAction, '=', false);
EXEC_ACTION_START(URIDecodeAction) {

  string varname  = par1;
  if (varname.size() && varname[0]=='$')
    varname.erase(0,1);
  string str = resolveVars(par2, sess, sc_sess, event_params);

  sc_sess->var[varname] = URL_decode(str);
  DBG("URL-decoded: $%s=\"%s\"\n", varname.c_str(), sc_sess->var[varname].c_str());
} EXEC_ACTION_END;
