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

#ifndef _JsonRPCServer_h_
#define _JsonRPCServer_h_

#include "AmArg.h"

#include <string>
using std::string;

struct JsonrpcPeerConnection;
struct JsonrpcNetstringsConnection;

struct JsonRpcError {
  int code;
  string message;
  AmArg data; 
  JsonRpcError(int code, string message, AmArg data)
  : code(code), message(message), data(data) { }
  ~JsonRpcError() { }
};

class JsonRpcServer {
 public:
  static void execRpc(const AmArg& rpc_params, AmArg& rpc_res);
  static void execRpc(const string& method, const string& id, const AmArg& params, AmArg& rpc_res);
  static void runCoreMethod(const string& method, const AmArg& params, AmArg& res);
 public:
  static int processMessage(char* msgbuf, unsigned int* msg_size,
			    JsonrpcPeerConnection* peer);

  static int createRequest(const string& evq_link, const string& method, AmArg& params, 
			   JsonrpcNetstringsConnection* peer, const AmArg& udata,
			   bool is_notification = false);

  static int createReply(JsonrpcNetstringsConnection* peer, const string& id, 
			 AmArg& result, bool is_error);
};

#endif // _JsonRPCServer_h_
