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

#include "RpcServerLoop.h"
#include "JsonRPCServer.h"
#include "JsonRPC.h"
#include "JsonRPCEvents.h"

#include "AmEventDispatcher.h"

#include "log.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> 
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <stddef.h>

#include "RpcPeer.h"

ev_io ev_accept;
ev_async JsonRPCServerLoop::async_w;
struct ev_loop* JsonRPCServerLoop::loop = 0;
JsonRPCServerLoop* JsonRPCServerLoop::_instance = NULL;

JsonRPCServerLoop* JsonRPCServerLoop::instance() {
  if (_instance == NULL) {
    _instance = new JsonRPCServerLoop();
  }
  return _instance;
}

int
setnonblock(int fd)
{
  int flags;

  flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    return flags;
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) 
    return -1;

  return 0;
}

// todo: use write_cb 
// static void write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
// { 
//   struct JsonrpcClientConnection *cli= 
// ((struct JsonrpcClientConnection*) (((char*)w) - offsetof(struct JsonrpcClientConnection,ev_write)));
//   if (revents & EV_WRITE){
//     ssize_t written = write(cli->fd,superjared,strlen(superjared));
//     if (written != strlen(superjared)) {
//       ERROR("writing response\n");
//     }
//     ev_io_stop(EV_A_ w);
//   }
//   close(cli->fd);
//   delete cli;
// }

static void read_cb(struct ev_loop *loop, struct ev_io *w, int revents) { 	
  struct JsonrpcNetstringsConnection *cli= ((struct JsonrpcNetstringsConnection*) (((char*)w) - 
					 offsetof(JsonrpcNetstringsConnection,ev_read)));
  // int r=0;
  // char rbuff[1024];
  if (revents & EV_READ){
    int res = cli->netstringsRead();
    switch (res) {
    case JsonrpcNetstringsConnection::CONTINUE: 
      ev_io_start(loop,&cli->ev_read); return;
    case JsonrpcNetstringsConnection::REMOVE: {
      ev_io_stop(EV_A_ w); 

      // let event receivers know about broken connection
      // todo: add connection id
      if (!cli->notificationReceiver.empty())
	AmEventDispatcher::instance()->post(cli->notificationReceiver, 
					    new JsonRpcConnectionEvent(JsonRpcConnectionEvent::DISCONNECT));
      if (!cli->requestReceiver.empty())
	AmEventDispatcher::instance()->post(cli->requestReceiver, 
					    new JsonRpcConnectionEvent(JsonRpcConnectionEvent::DISCONNECT));
      for (std::map<std::string, std::string>::iterator it=
	     cli->replyReceivers.begin(); it != cli->replyReceivers.end(); it++) {
	AmEventDispatcher::instance()->post(it->second, 
					    new JsonRpcConnectionEvent(JsonRpcConnectionEvent::DISCONNECT));	
      }
      delete cli; 
    } return;
    case JsonrpcNetstringsConnection::DISPATCH: {
      ev_io_stop(EV_A_ w); 
      JsonRPCServerLoop::dispatchServerEvent(new JsonServerEvent(cli));
    } return;
    }
    // todo: put into reader thread
    //r=read(cli->fd,&rbuff,1024);
    return;
  }
  // put back to read loop
  // ev_io_start(loop,&cli->ev_read);

  // ev_io_stop(EV_A_ w);
  // ev_io_init(&cli->ev_write,write_cb,cli->fd,EV_WRITE);
  // ev_io_start(loop,&cli->ev_write);	
}

void JsonRPCServerLoop::dispatchServerEvent(AmEvent* ev) {
  instance()->threadpool.dispatch(ev);
}

static void accept_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
  int client_fd;
  struct JsonrpcNetstringsConnection *a_client;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  client_fd = accept(w->fd, (struct sockaddr *)&client_addr, &client_len);
  if (client_fd == -1) {
    return;
  }
   	 
  a_client = new JsonrpcNetstringsConnection();
  a_client->fd=client_fd;
  if (setnonblock(a_client->fd) < 0) {
    ERROR("failed to set client socket to non-blocking");
    return;
  }
  ev_io_init(&a_client->ev_read,read_cb,a_client->fd,EV_READ);
  ev_io_start(loop,&a_client->ev_read);
}

static void async_cb (EV_P_ ev_async *w, int revents)
{
  JsonRPCServerLoop::_processEvents();
}

void JsonRPCServerLoop::_processEvents() {
  instance()->processEvents();
}

void JsonRPCServerLoop::process(AmEvent* ev) {
  DBG("processing event in server loop\n");
  JsonServerEvent* server_event=dynamic_cast<JsonServerEvent*>(ev);
  if (server_event==NULL) {
    ERROR("wrong event type received\n");
    return;
  }

  JsonrpcNetstringsConnection* a_client = server_event->conn;
  a_client->resetRead();
  ev_io_init(&a_client->ev_read,read_cb,a_client->fd,EV_READ);
  ev_io_start(loop,&a_client->ev_read);
}

JsonRPCServerLoop::JsonRPCServerLoop()
  : AmEventQueue(this)
{
  loop = ev_default_loop (0);
  // one thread is started here so that 
  // in app initialization code, there is already
  // a server thread available to receive events 
  DBG("starting one server thread for startup requests...\n");
  threadpool.addThreads(1);
}


JsonRPCServerLoop::~JsonRPCServerLoop() {
}

void JsonRPCServerLoop::run() {
  DBG("adding %d more server threads \n", 
      JsonRPCServerModule::threads - 1);
  threadpool.addThreads(JsonRPCServerModule::threads - 1);

  INFO("running server loop; listening on port %d\n", 
       JsonRPCServerModule::port);
  int listen_fd;
  struct sockaddr_in listen_addr; 
  int reuseaddr_on = 1;
  listen_fd = socket(AF_INET, SOCK_STREAM, 0); 
  if (listen_fd < 0)
    err(1, "listen failed");
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on,
		 sizeof(reuseaddr_on)) == -1)
    err(1, "setsockopt failed");
  memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(JsonRPCServerModule::port);
  if (bind(listen_fd, (struct sockaddr *)&listen_addr,
	   sizeof(listen_addr)) < 0) {
    ERROR("bind failed\n");
    return;
  }
  if (listen(listen_fd,5) < 0) {
    ERROR("listen failed\n");
    return;
  }
  if (setnonblock(listen_fd) < 0) {
    ERROR("failed to set server socket to non-blocking\n");
    return;
  }
	 
  ev_io_init(&ev_accept,accept_cb,listen_fd,EV_READ);
  ev_io_start(loop,&ev_accept);

  ev_async_init (&async_w, async_cb);
  ev_async_start (EV_A_ &async_w);

  INFO("running event loop\n");
  ev_loop (loop, 0);
  INFO("event loop finished\n");
}

void JsonRPCServerLoop::on_stop() {
  INFO("todo\n");
}

void JsonRPCServerLoop::returnConnection(JsonrpcNetstringsConnection* conn) {
  DBG("returning connection %p\n", conn);
  instance()->postEvent(new JsonServerEvent(conn));
  ev_async_send(loop, &async_w);
}

void JsonRPCServerLoop::execRpc(const string& evq_link, 
				const string& notificationReceiver,
				const string& requestReceiver,
				int flags,
				const string& host,
				int port, const string& method, 
				AmArg& params,
				AmArg& ret) {
  JsonrpcNetstringsConnection* peer = new JsonrpcNetstringsConnection();
  peer->flags = flags;
  peer->notificationReceiver = notificationReceiver;
  peer->requestReceiver = requestReceiver;

  string res_str;
  int res = peer->connect(host, port, res_str);
  if (res) {
    ret.push(400);
    ret.push("Error in connect: "+res_str);
    delete peer;
    return;
  }

  if (JsonRpcServer::createRequest(evq_link, method, params, peer)) {
    ret.push(400);
    ret.push("Error creating request message");
  }

  JsonRPCServerLoop::dispatchServerEvent(new JsonServerEvent(peer));

  ret.push(200);
  ret.push("OK");
}
