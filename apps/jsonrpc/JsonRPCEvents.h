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

#ifndef _JsonRPCEvents_h_
#define _JsonRPCEvents_h_

#include "AmEvent.h"
#include "AmArg.h"

#include <string>
using std::string;

struct JsonRpcEvent
  : public AmEvent {
  JsonRpcEvent() 
    : AmEvent(122) { }
  virtual ~JsonRpcEvent() { }
};

struct JsonRpcResponse {
  string id;
  AmArg data;
  bool is_error;
  
  JsonRpcResponse(bool is_error, string id, AmArg data)
  : is_error(is_error), id(id), data(data) { }
  JsonRpcResponse(bool is_error, string id) 
  : is_error(is_error), id(id) { }

  ~JsonRpcResponse() { }
};

struct JsonRpcResponseEvent
  : public JsonRpcEvent {
  JsonRpcResponse response;

  JsonRpcResponseEvent(bool is_error, string id, AmArg data)
    : response(is_error, id, data)
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

#endif // _JsonRPCEvents_h_
