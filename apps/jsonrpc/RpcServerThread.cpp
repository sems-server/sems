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

#include "RpcServerThread.h"

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

#include "RpcPeer.h"
#include "RpcServerLoop.h"
#include "jsonArg.h"
#include "JsonRPCServer.h"

#include "log.h"

RpcServerThread::RpcServerThread()
  : AmEventQueue(this) {
}

RpcServerThread::~RpcServerThread() {
}

void RpcServerThread::run() {
  while (true) {
    waitForEvent();
    processEvents();
  }
}

void RpcServerThread::on_stop() {
  INFO("TODO: stop server thread\n");
}

void RpcServerThread::process(AmEvent* event) {
  JsonServerEvent* server_event = dynamic_cast<JsonServerEvent*>(event);
  if (server_event == NULL) {
    ERROR("invalid event to process\n");
    return;
  }
  JsonrpcNetstringsConnection* conn = server_event->conn;


  bool processed_message = false;

  int res = 0;
  if (conn->messagePending() && conn->messageIsRecv()) {
    processed_message = true;
    DBG("processing message >%.*s<\n", conn->msg_size, conn->msgbuf);
    res = JsonRpcServer::processMessage(conn->msgbuf, &conn->msg_size, 
					conn);
    if (res<0) {
      INFO("error processing message - closing connection\n");
      conn->close();
      delete conn;
      return;
    }

    conn->msg_recv = false;
  }

  if (conn->messagePending() && !conn->messageIsRecv()) {
    res = conn->netstringsBlockingWrite();
    if (res == JsonrpcNetstringsConnection::REMOVE) {
      delete conn;
      return;
    }
  }

  if (processed_message && 
      (conn->flags & JsonrpcPeerConnection::FL_CLOSE_ALWAYS)) {
    DBG("closing connection marked as FL_CLOSE_ALWAYS\n");
    conn->close();
    delete conn;
    return;
  }

  // give back connection into server loop
  JsonRPCServerLoop::returnConnection(conn);  

  // ev_io_init(&cli->ev_write,write_cb,cli->fd,EV_WRITE);
  // ev_io_start(loop,&cli->ev_write);   
}


RpcServerThreadpool::RpcServerThreadpool() {
}

RpcServerThreadpool::~RpcServerThreadpool() {
}

/** round-robin dispatch to one thread */  
void RpcServerThreadpool::dispatch(AmEvent* ev) {
  threads_mut.lock();
  if (!threads.size()) {
    ERROR("no threads started for Rpc servers\n");
    delete ev;
    threads_mut.unlock();
    return;
  }

  (*t_it)->postEvent(ev);

  t_it++;
  if (t_it == threads.end())
    t_it = threads.begin();

  threads_mut.unlock();
}

void RpcServerThreadpool::addThreads(unsigned int cnt) {
  DBG("adding %u RPC server threads\n", cnt);
  threads_mut.lock();
  for (unsigned int i=0;i<cnt;i++) {
    RpcServerThread* thr = new RpcServerThread();
    thr->start();
    threads.push_back(thr);
  }
  t_it = threads.begin();
  threads_mut.unlock();
}

