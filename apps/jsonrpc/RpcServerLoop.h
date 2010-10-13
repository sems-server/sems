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

#ifndef _RpcServerLoop_h_
#define _RpcServerLoop_h_
#include <ev.h>

#include "AmEvent.h"
#include "AmEventQueue.h"
#include "AmThread.h"

#include "RpcPeer.h"
#include "RpcServerThread.h"
#include "JsonRPCEvents.h"

#include "AmArg.h"

#include <map>

class JsonRPCServerLoop 
: public AmThread, public AmEventQueue, public AmEventHandler
{
  static  RpcServerThreadpool threadpool;
  static ev_async async_w;
  static struct ev_loop *loop;

  static JsonRPCServerLoop* _instance;

  static std::map<string, JsonrpcPeerConnection*> connections;
  static AmMutex connections_mut;

  static vector<JsonServerEvent*> pending_events; // todo: use map<set<> > if many pending events
  static AmMutex pending_events_mut;

 public:
  JsonRPCServerLoop();
  ~JsonRPCServerLoop();

  static JsonRPCServerLoop* instance();

  static void returnConnection(JsonrpcNetstringsConnection* conn);
  static void dispatchServerEvent(AmEvent* ev);
  static void _processEvents();

  static void execRpc(const string& evq_link, 
		      const string& notificationReceiver,
		      const string& requestReceiver,
		      int flags,
		      const string& host, 
		      int port, const string& method, 
		      const AmArg& params,
		      const AmArg& udata,
		      AmArg& ret);

  static void sendMessage(const string& connection_id, 
			  int msg_type, 
			  const string& method, 
			  const string& id,
			  const string& reply_sink,
			  const AmArg& params,
			  const AmArg& udata,
			  AmArg& ret);
  void run();
  void on_stop();
  void process(AmEvent* ev);

  static string newConnectionId();

  /**
     add connection with id
     @return whether connection with this id existed before
  */
  static bool registerConnection(JsonrpcPeerConnection* peer, const string& id);

  /**
     remove a connection with id
     @return whether connection with this id existed
  */
  static bool removeConnection(const string& id);

  /**
     get a connection with id
     @return NULL if not found
  */
  static JsonrpcPeerConnection* getConnection(const string& id);

};

#endif
