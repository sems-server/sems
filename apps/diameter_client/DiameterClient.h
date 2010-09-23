/*
 * Copyright (C) 2007-2009 IPTEGO GmbH
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

#ifndef _DIAMETER_CLIENT_H
#define _DIAMETER_CLIENT_H

#include "AmApi.h"

#include "ServerConnection.h"

#include <string>
#include <map>
using std::string;
using std::multimap;

class DiameterClient  
: public AmDynInvokeFactory,
  public AmDynInvoke
{

  static DiameterClient* _instance;

  multimap<string, ServerConnection*> connections;
  AmMutex conn_mut;

  void newConnection(const AmArg& args, AmArg& ret);
  void sendRequest(const AmArg& args, AmArg& ret);

 public:
  DiameterClient(const string& name);
  ~DiameterClient();

  // DI factory
  AmDynInvoke* getInstance() { return instance(); }

  // DI API
  static DiameterClient* instance();
  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);

  // DI-factory
  int onLoad();	
};

#endif
