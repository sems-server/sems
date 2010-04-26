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

#ifndef _RpcServerLoop_h_
#define _RpcServerLoop_h_
#include <ev.h>

#include "AmEvent.h"
#include "AmEventQueue.h"
#include "AmThread.h"

#include "RpcPeer.h"
#include "RpcServerThread.h"
#include "AmArg.h"
class JsonRPCServerLoop 
: public AmThread, public AmEventQueue, public AmEventHandler
{
  RpcServerThreadpool threadpool;
  static ev_async async_w;
  static struct ev_loop *loop;

  static JsonRPCServerLoop* _instance;

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
		      AmArg& params,
		      AmArg& ret);
  void run();
  void on_stop();
  void process(AmEvent* ev);
};

#endif
