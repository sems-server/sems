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

#include "RpcServerLoop.h"
#include "JsonRPCServer.h"
#include "JsonRPC.h"
#include "JsonRPCEvents.h"

#include "AmSession.h"
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
RpcServerThreadpool JsonRPCServerLoop::threadpool;

std::map<string, JsonrpcPeerConnection*> JsonRPCServerLoop::connections;
AmMutex JsonRPCServerLoop::connections_mut;

vector<JsonServerEvent*> JsonRPCServerLoop::pending_events;
AmMutex JsonRPCServerLoop::pending_events_mut;

JsonRPCServerLoop* JsonRPCServerLoop::instance() {
  if (_instance == NULL) {
    _instance = new JsonRPCServerLoop();
  }
  return _instance;
}

int setnonblock(int fd)
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

  struct JsonrpcNetstringsConnection *peer= 
    ((struct JsonrpcNetstringsConnection*) 
     (((char*)w) - offsetof(JsonrpcNetstringsConnection,ev_read)));

  DBG("read_cb in connection %p\n", peer);

  // int r=0;
  // char rbuff[1024];
  if (revents & EV_READ){
    // read message - here in main server thread (more efficient for small messages)
    int res = peer->netstringsRead();
    switch (res) {
    case JsonrpcNetstringsConnection::CONTINUE: 
      ev_io_start(loop,&peer->ev_read); return;

    case JsonrpcNetstringsConnection::REMOVE: {
      ev_io_stop(EV_A_ w); 

      peer->notifyDisconnect();

      JsonRPCServerLoop::instance()->removeConnection(peer->id);
      delete peer;

    } return;
    case JsonrpcNetstringsConnection::DISPATCH: {
      ev_io_stop(EV_A_ w); 
      JsonRPCServerLoop::
	dispatchServerEvent(new JsonServerEvent(peer, JsonServerEvent::StartReadLoop));
    } return;
    }
    return;
  }
  // put back to read loop
  // ev_io_start(loop,&cli->ev_read);

  // ev_io_stop(EV_A_ w);
  // ev_io_init(&cli->ev_write,write_cb,cli->fd,EV_WRITE);
  // ev_io_start(loop,&cli->ev_write);	
}

void JsonRPCServerLoop::dispatchServerEvent(AmEvent* ev) {
  threadpool.dispatch(ev);
}

static void accept_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
  int client_fd;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  client_fd = accept(w->fd, (struct sockaddr *)&client_addr, &client_len);
  if (client_fd == -1) {
    return;
  }
   	 
  string connection_id = JsonRPCServerLoop::newConnectionId();
  JsonrpcNetstringsConnection* peer = new JsonrpcNetstringsConnection(connection_id);
  peer->fd=client_fd;
  if (setnonblock(peer->fd) < 0) {
    delete peer;
    ERROR("failed to set client socket to non-blocking");
    return;
  }

  JsonRPCServerLoop::registerConnection(peer, connection_id);

  ev_io_init(&peer->ev_read,read_cb,peer->fd,EV_READ);
  ev_io_start(loop,&peer->ev_read);
}

static void async_cb (EV_P_ ev_async *w, int revents)
{
  JsonRPCServerLoop::_processEvents();
}

void JsonRPCServerLoop::_processEvents() {
  instance()->processEvents();
}

void JsonRPCServerLoop::process(AmEvent* ev) {
  DBG("server loop - processing event\n");
  JsonServerEvent* server_event=dynamic_cast<JsonServerEvent*>(ev);
  if (server_event==NULL) {
    ERROR("wrong event type received\n");
    return;
  }

  switch (server_event->event_id) {
  case JsonServerEvent::StartReadLoop: {
    JsonrpcNetstringsConnection* a_client = server_event->conn;

    DBG("checking for pending events to connection %p/%s\n",
	a_client, a_client->id.c_str());

    pending_events_mut.lock();
    // (check whether event for that connection pending)
    for (vector<JsonServerEvent*>::iterator it=
	   pending_events.begin(); it != pending_events.end(); it++) {
      if ((*it)->connection_id == a_client->id) {
	// stop read loop
	ev_io_stop(loop,&a_client->ev_read);

	JsonServerEvent* server_event = *it;
	pending_events.erase(it);
	pending_events_mut.unlock();
	
	DBG("got pending event for connection '%s'\n", a_client->id.c_str());
	
	server_event->conn = a_client;
	dispatchServerEvent(server_event);

	return;
      }
    }
    pending_events_mut.unlock();
    DBG("no pending events for connection %p/%s, starting read loop\n",
	a_client, a_client->id.c_str());

    a_client->resetRead();
    ev_io_init(&a_client->ev_read,read_cb,a_client->fd,EV_READ);
    ev_io_start(loop,&a_client->ev_read);
  }; break;

  case JsonServerEvent::SendMessage: {
    JsonServerSendMessageEvent* snd_msg_ev = 
      dynamic_cast<JsonServerSendMessageEvent*>(server_event);
    if (snd_msg_ev == NULL) {
      ERROR("invalid SendMessage type event received\n");
      return;
    }

    JsonrpcPeerConnection* p_peer = getConnection(snd_msg_ev->connection_id);
    if (p_peer == NULL) {
      WARN("dropping message to inexistent/broken connection '%s' "
	   "(is_reply=%s, method=%s, id=%s, params='%s')",
	   snd_msg_ev->connection_id.c_str(), snd_msg_ev->is_reply?"true":"false", 
	   snd_msg_ev->method.c_str(), snd_msg_ev->id.c_str(), 
	   AmArg::print(snd_msg_ev->params).c_str());
      return;
    }
    JsonrpcNetstringsConnection* peer = dynamic_cast<JsonrpcNetstringsConnection*>(p_peer);
    if (NULL == peer) {
      ERROR("invalid connection type\n"); // todo: other transports
      return;
    }

    if (ev_is_active(&peer->ev_read)) {
      // ok, peer is in read loop, we can dispatch it to thread for sending
      ev_io_stop(EV_A_ &peer->ev_read);
      JsonRPCServerLoop::
	dispatchServerEvent(new JsonServerSendMessageEvent(*snd_msg_ev, peer));
    } else {
      // peer is being processed by thread, save event for later sending
      pending_events_mut.lock();
      // (need to copy event here: original event is deleted when event processed)
      pending_events.push_back(new JsonServerSendMessageEvent(*snd_msg_ev));
      size_t q_size = pending_events.size();
      pending_events_mut.unlock();
      DBG("queued event for connection %s (total %zd events pending)\n", 
	  snd_msg_ev->connection_id.c_str(), q_size);     
    }
      
  }; break;
    // todo: process remove connection event 

  default: 
    ERROR("unknown server event type received\n");
    return;
  }

}

JsonRPCServerLoop::JsonRPCServerLoop()
  : AmEventQueue(this)
{
  loop = ev_default_loop (0);
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

  // async watcher to process our events in event loop
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
  pending_events_mut.lock();
  // (check whether event for that connection pending)
  DBG("checking %zd pending events\n", pending_events.size());
  for (vector<JsonServerEvent*>::iterator it=
	 pending_events.begin(); it != pending_events.end(); it++) {
    DBG("%s vs %s\n", (*it)->connection_id.c_str(),conn->id.c_str());
    if ((*it)->connection_id == conn->id) {
      JsonServerEvent* server_event = *it;
      pending_events.erase(it);
      pending_events_mut.unlock();

      DBG("got pending event for connection '%s'\n", conn->id.c_str());

      server_event->conn = conn;
      dispatchServerEvent(server_event);
      return;
    }
  }
  pending_events_mut.unlock();

  DBG("returning connection %p\n", conn);
  instance()->postEvent(new JsonServerEvent(conn, JsonServerEvent::StartReadLoop));

  ev_async_send(loop, &async_w);
}

void JsonRPCServerLoop::execRpc(const string& evq_link, 
				const string& notificationReceiver,
				const string& requestReceiver,
				int flags,
				const string& host,
				int port, const string& method, 
				const AmArg& params,
				const AmArg& udata,
				AmArg& ret) {
  string connection_id = newConnectionId();
  JsonrpcNetstringsConnection* peer = new JsonrpcNetstringsConnection(connection_id);
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

  // DBG("evq_link  = '%s'\n", evq_link.c_str());
  // if (JsonRpcServer::createRequest(evq_link, method, params, peer)) {
  //   ret.push(400);
  //   ret.push("Error creating request message");
  //   delete peer;
  //   return;
  // }

  registerConnection(peer, connection_id);

  DBG("dispatching JsonServerSendMessageEvent\n");
  JsonServerSendMessageEvent* send_message_event = 
    new JsonServerSendMessageEvent(connection_id, false, method, "1" /* id - not empty */, 
				   params, udata, evq_link);

  JsonRPCServerLoop::dispatchServerEvent(send_message_event);

  // JsonRPCServerLoop::
  //   dispatchServerEvent(new JsonServerEvent(peer, JsonServerEvent::SendMessage));

  ret.push(200);
  ret.push("OK");
  ret.push(connection_id);
}

void JsonRPCServerLoop::sendMessage(const string& connection_id, 
				    int msg_type, 
				    const string& method, 
				    const string& id,
				    const string& reply_sink,
				    const AmArg& params,
				    const AmArg& udata,
				    AmArg& ret) {
  // check for presence of connection
  // (connection might still be removed until we really 
  // process the request to send message, but here we already 
  // catch most failures)
  if (getConnection(connection_id)==NULL) {
    ret.push(400);
    ret.push("unknown connection");
    return;
  }

  JsonServerSendMessageEvent* ev = 
    new JsonServerSendMessageEvent(connection_id, msg_type != JSONRPC_MSG_REQUEST, 
				   method, id, params, udata, reply_sink);
  ev->is_error = msg_type == JSONRPC_MSG_ERROR;
  instance()->postEvent(ev);

  // wake up event loop to process message
  ev_async_send(loop, &async_w);

  ret.push(200);
  ret.push("posted");
}

string JsonRPCServerLoop::newConnectionId() {
  return AmSession::getNewId();
}

bool JsonRPCServerLoop::registerConnection(JsonrpcPeerConnection* peer, const string& id) {
  bool res = false;
  connections_mut.lock();
  if (connections.find(id) != connections.end())
    res = true;
  connections[id] = peer;
  connections_mut.unlock();

  DBG("registered connection '%s'\n", id.c_str());
  return res;
}

bool JsonRPCServerLoop::removeConnection(const string& id) {
  bool res = false;
  connections_mut.lock();
  std::map<string, JsonrpcPeerConnection*>::iterator it = 
    connections.find(id);
  if (it != connections.end()) {
    res = true;
    connections.erase(it);
  }
  connections_mut.unlock();
  DBG("deregistered connection '%s'\n", id.c_str());
  return res;
}

JsonrpcPeerConnection* JsonRPCServerLoop::getConnection(const string& id) {
  JsonrpcPeerConnection* res = NULL;
  connections_mut.lock();
  std::map<string, JsonrpcPeerConnection*>::iterator it =
    connections.find(id);
  if (it != connections.end()) 
    res = it->second;  
  connections_mut.unlock();
  return res;
}

