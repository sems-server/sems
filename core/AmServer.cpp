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

#include "log.h"
#include "AmServer.h"

#include <unistd.h>

//
// AmServer methods
//

AmServer* AmServer::_instance;
AmCtrlInterface* AmServer::ctrlIface;

AmServer*  AmServer::instance() 
{
  return _instance ? _instance : ((_instance = new AmServer()));
}

void AmServer::run()
{
  ctrlIface->start();
  ctrlIface->join();
}

void AmServer::regIface(const AmCtrlInterface *i)
{
  if (ctrlIface) {
    ERROR("control interface already registered; aborting second attempt.\n");
    return;
  }
  ctrlIface = const_cast<AmCtrlInterface *>(i);
}

bool AmServer::sendReply(const AmSipReply &reply) 
{
  return ctrlIface->send(reply);
}

bool AmServer::sendRequest(const AmSipRequest &req, string &serKey)
{
  return ctrlIface->send(req, serKey);
}

string AmServer::getContact(const string &displayName, 
    const string &userName, const string &hostName, 
    const string &uriParams, const string &hdrParams)
{
  return ctrlIface->getContact(displayName, userName, hostName, uriParams,
      hdrParams);
}
