/*
 * $Id$
 *
 * Copyright (C) 2006 Stefan Sayer
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

#include "AmUAC.h"
#include "AmSipMsg.h"
#include "AmSession.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"

AmSession* AmUAC::dialout(const string& user,
			  const string& app_name,
			  const string& r_uri, 
			  const string& from,
			  const string& from_uri,
			  const string& to,
			  const string& local_tag,
			  const string& hdrs,
			  AmArg*  session_params) {
 
  AmSipRequest req;

  req.cmd      = app_name;
  req.user     = user;
  req.method   = "INVITE";
  req.dstip    = AmConfig::LocalIP;
  req.r_uri    = r_uri;
  req.from     = from;
  req.from_uri = from_uri;
  if (!local_tag.length())
    req.from_tag   = AmSession::getNewId();
  else 
    req.from_tag   = local_tag;
  req.to       = to;
  req.to_tag   = "";
  req.callid   = AmSession::getNewId() + "@" + AmConfig::LocalIP;
  req.hdrs     = hdrs;
    
  // set outbound proxy as next hop 
  if (!AmConfig::OutboundProxy.empty()) 
    req.next_hop = AmConfig::OutboundProxy;

  return AmSessionContainer::instance()->startSessionUAC(req, session_params);
}

