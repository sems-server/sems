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
  JsonrpcNetstringsConnection* connection = server_event->conn;

  // todo: check event type - for now handle all types equally

  if (server_event->event_id == JsonServerEvent::SendMessage) {
    JsonServerSendMessageEvent* snd_msg_ev = 
      dynamic_cast<JsonServerSendMessageEvent*>(server_event);

    if (NULL == snd_msg_ev) {
      ERROR("wrong event type received\n");
      return;
    }

    if (NULL == connection) {
      DBG("getting connection for id %s\n", snd_msg_ev->connection_id.c_str());
      JsonrpcPeerConnection* js_connection = JsonRPCServerLoop::getConnection(snd_msg_ev->connection_id);
      if ((NULL == js_connection) || 
	  (NULL == 
	   (connection = dynamic_cast<JsonrpcNetstringsConnection*>(js_connection))))  {
	ERROR("getting connection for id %s - message will not be sent\n",
	      snd_msg_ev->connection_id.c_str());
	return;
      }
    }

    if (!snd_msg_ev->is_reply) {
      if (JsonRpcServer::createRequest(snd_msg_ev->reply_link, snd_msg_ev->method, 
				       snd_msg_ev->params, connection,
				       snd_msg_ev->udata,
				       snd_msg_ev->id.empty())) {
	ERROR("creating request\n");
	// give back connection into server loop
	JsonRPCServerLoop::returnConnection(connection);
	return;
      }
    } else {
      if (JsonRpcServer::createReply(connection, snd_msg_ev->id, snd_msg_ev->params,
				     snd_msg_ev->is_error)) {
	// give back connection into server loop
	JsonRPCServerLoop::returnConnection(connection);
	return;
      }
    }
    connection->msg_recv = false;

  }

  bool processed_message = false;

  int res = 0;
  if (connection->messagePending() && connection->messageIsRecv()) {
    DBG("processing message >%.*s<\n", connection->msg_size, connection->msgbuf);
    res = JsonRpcServer::processMessage(connection->msgbuf, &connection->msg_size, 
					connection);
    if (res<0) {
      INFO("error processing message - closing connection\n");
      connection->close();
      connection->notifyDisconnect();
      JsonRPCServerLoop::removeConnection(connection->id);
      delete connection;
      return;
    }

    connection->msg_recv = false;
    processed_message = true;
  }

  DBG("connection->messagePending() = %s\n", connection->messagePending()?"true":"false");

  if (connection->messagePending() && !connection->messageIsRecv()) {
    DBG("calling write\n");
    res = connection->netstringsBlockingWrite();
    if (res == JsonrpcNetstringsConnection::REMOVE) {
      connection->notifyDisconnect();
      JsonRPCServerLoop::removeConnection(connection->id);
      delete connection;
      return;
    }
  }

  if (processed_message && 
      (connection->flags & JsonrpcPeerConnection::FL_CLOSE_ALWAYS)) {
    DBG("closing connection marked as FL_CLOSE_ALWAYS\n");
    connection->close();
    connection->notifyDisconnect(); // ??
    JsonRPCServerLoop::removeConnection(connection->id);
    delete connection;
    return;
  }

  // give back connection into server loop
  JsonRPCServerLoop::returnConnection(connection);  

  // ev_io_init(&cli->ev_write,write_cb,cli->fd,EV_WRITE);
  // ev_io_start(loop,&cli->ev_write);   
}


RpcServerThreadpool::RpcServerThreadpool() {
  // one thread is started here so that 
  // in app initialization code, there is already
  // a server thread available to receive events 
  DBG("starting one server thread for startup requests...\n");
  addThreads(1);
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

