/*
 * $Id: ModMysql.cpp 1764 2010-04-01 14:33:30Z peter_lemenkov $
 *
 * Copyright (C) 2010 TelTech Systems Inc.
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

#ifndef _JSON_RPC_H_
#define _JSON_RPC_H_
#include "AmApi.h"
#include "RpcServerLoop.h"

#define DEFAULT_JSONRPC_SERVER_PORT    7080
#define DEFAULT_JSONRPC_SERVER_THREADS 5

class JsonRPCServerModule
: public AmDynInvokeFactory, 
  public AmDynInvoke 
{
  static JsonRPCServerModule* _instance;

  int load();

  JsonRPCServerLoop* server_loop;

  // DI methods
  void execRpc(const AmArg& args, AmArg& ret);
  void sendMessage(const AmArg& args, AmArg& ret);

 public:
  JsonRPCServerModule(const string& mod_name);
  ~JsonRPCServerModule();
  int onLoad();
  // DI factory
  AmDynInvoke* getInstance() { return instance(); }

  // DI API
  static JsonRPCServerModule* instance();

  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);

  // configuration
  static int port;
  static int threads;
};

#endif
