/*
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

#ifndef _JsonRPCEvents_h_
#define _JsonRPCEvents_h_

#include "AmEvent.h"
#include "AmArg.h"

#include <string>
using std::string;

#define JSONRPC_MSG_REQUEST  0
#define JSONRPC_MSG_RESPONSE 1
#define JSONRPC_MSG_ERROR    2
struct JsonrpcNetstringsConnection;

struct JsonRpcEvent
  : public AmEvent {
  string connection_id;
  JsonRpcEvent() 
    : AmEvent(122) { }
  virtual ~JsonRpcEvent() { }
};

struct JsonRpcResponse {
  string id;
  AmArg data;
  bool is_error;
  
  JsonRpcResponse(bool is_error, string id, const AmArg& data)
  : id(id), data(data), is_error(is_error) { }
  JsonRpcResponse(bool is_error, string id) 
  : id(id), is_error(is_error) { }

  ~JsonRpcResponse() { }
};

struct JsonRpcResponseEvent
  : public JsonRpcEvent {
  JsonRpcResponse response;
  AmArg udata;

 JsonRpcResponseEvent(bool is_error, string id, const AmArg& data, const AmArg& udata)
   : response(is_error, id, data), udata(udata)
    { }
  JsonRpcResponseEvent(bool is_error, string id)
    : response(is_error, id)
    { }
  ~JsonRpcResponseEvent() { }
};

struct JsonRpcRequestEvent
  : public JsonRpcEvent {
  string method;
  string id;
  AmArg params;

  // notification without parameters 
 JsonRpcRequestEvent(string method) 
    : method(method) { }
  
  // notification with parameters
 JsonRpcRequestEvent(string method, AmArg params) 
   : method(method), params(params) { }

  // request without parameters 
 JsonRpcRequestEvent(string method, string id) 
   : method(method), id(id) { }

  // request with parameters 
 JsonRpcRequestEvent(string method, string id, AmArg params) 
   : method(method), id(id), params(params) { }
  
  bool isNotification() { return id.empty(); }
};

struct JsonRpcConnectionEvent
  : public JsonRpcEvent {

  enum {
    DISCONNECT = 0
  };

  int what;
  string connection_id;

 JsonRpcConnectionEvent(int what, const string& connection_id) 
   : what(what), connection_id(connection_id) { }
  ~JsonRpcConnectionEvent() { }
};


// events used internally: 

struct JsonServerEvent 
 : public AmEvent {

  enum EventType { 
    StartReadLoop = 0,
    SendMessage    
  };

  JsonrpcNetstringsConnection* conn;
  string connection_id;

 JsonServerEvent(JsonrpcNetstringsConnection* c,
		 EventType ev_type)
    : AmEvent(ev_type), conn(c) { }

 JsonServerEvent(const string& connection_id,
		 EventType ev_type)
   : AmEvent(ev_type), conn(NULL),
    connection_id(connection_id) { }

  ~JsonServerEvent() { }
};

struct JsonServerSendMessageEvent
  : public JsonServerEvent {

  bool is_reply; 
  string method;
  string id;
  AmArg params;
  string reply_link;
  bool is_error;
  AmArg udata;

  JsonServerSendMessageEvent(const string& connection_id,
			     bool is_reply,
			     const string& method,
			     const string& id,
			     const AmArg& params,
			     const AmArg& udata = AmArg(),
			     const string& reply_link = "")
    : JsonServerEvent(connection_id, SendMessage),
    is_reply(is_reply), method(method),
    id(id), params(params), reply_link(reply_link), udata(udata) { }

 JsonServerSendMessageEvent(const JsonServerSendMessageEvent& e,
			    JsonrpcNetstringsConnection* conn)
   : JsonServerEvent(conn, SendMessage),
    is_reply(e.is_reply), method(e.method),
    id(e.id), params(e.params), reply_link(e.reply_link),
    is_error(e.is_error), udata(e.udata) {
    connection_id = e.connection_id;
  }

};

#endif // _JsonRPCEvents_h_
