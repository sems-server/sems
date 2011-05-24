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

#include "RpcPeer.h"
#include "log.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <string>
using std::string;

#include "AmUtils.h"
#include "AmEventDispatcher.h"
#include "JsonRPCEvents.h"
#include "sip/resolver.h"

void JsonrpcPeerConnection::notifyDisconnect() {
  // let event receivers know about broken connection
  DBG("notifying event receivers about broken connection\n");
  if (!notificationReceiver.empty())
    AmEventDispatcher::instance()->
      post(notificationReceiver, 
	   new JsonRpcConnectionEvent(JsonRpcConnectionEvent::DISCONNECT, id));
  if (!requestReceiver.empty())
    AmEventDispatcher::instance()->
      post(requestReceiver, 
	   new JsonRpcConnectionEvent(JsonRpcConnectionEvent::DISCONNECT, id));
  
  for (std::map<std::string, std::pair<std::string, AmArg > > ::iterator it=
	 replyReceivers.begin(); it != replyReceivers.end(); it++) {
    AmEventDispatcher::instance()->
      post(it->second.first,
	   new JsonRpcConnectionEvent(JsonRpcConnectionEvent::DISCONNECT, id));	
  }
}

JsonrpcNetstringsConnection::JsonrpcNetstringsConnection(const std::string& id) 
  : JsonrpcPeerConnection(id), 
    fd(0), msg_size(0), rcvd_size(0), in_msg(false), msg_recv(true)
{
}

JsonrpcNetstringsConnection::~JsonrpcNetstringsConnection() {
}

int JsonrpcNetstringsConnection::connect(const string& host, int port,
					 string& res_str) {
  struct sockaddr_in sa;
  {
    sockaddr_storage _sa;
    dns_handle       _dh;

    if(resolver::instance()->resolve_name(host.c_str(),
					&_dh,&_sa,IPv4) < 0) {
      res_str = "resolving '"+host+"' failed\n";
      return 300;
    }

    memcpy(&sa.sin_addr,&((sockaddr_in*)&_sa)->sin_addr,sizeof(in_addr));
  }

  // if (!populate_sockaddr_in_from_name(host, &sa)) {
  //   res_str = "populate_sockaddr_in_from_name failed\n";
  //   return 300;
  // }

  fd = socket(PF_INET, SOCK_STREAM, 0);
  sa.sin_port = htons(port);
  sa.sin_family = PF_INET;

  int flags = fcntl(fd, F_GETFL);
  if (flags < 0) {
    ::close(fd);
    res_str = "error setting socket non-blocking";
    return 300;
  }

  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    ::close(fd);
    res_str = "error setting socket non-blocking";
    return 300;
  }

#ifndef MSG_NOSIGNAL
  int onoff=0;
  if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &onoff, sizeof(onoff))) {
    res_str = "error in setsockopt: "+string(strerror(errno));
    ::close(fd);
    return 300;
  }  
#endif
  
  if (::connect(fd, (const struct sockaddr *)&sa,
              sizeof(sa)) ==-1 && errno != EINPROGRESS) {
    ::close(fd);
    res_str = "error connecting to "+host+": "+ strerror(errno);
    return 300;
  }

  fd_set wfds;
  struct timeval tv;
  int retval;
  
  FD_ZERO(&wfds);
  FD_SET(fd, &wfds);
  
  /* Wait up to five seconds. */
  tv.tv_sec = 5;
  tv.tv_usec = 0;

  while (true) {
    retval = select(fd+1, NULL, &wfds, NULL, &tv);
    if (retval<0 && errno == EINTR)
      continue;
    if (retval<0) {
      res_str = "error waiting for connect: "+string(strerror(errno));
      ::close(fd);
      return 300;
    }
    if (retval==0) {
      res_str = "connect to "+host+" timed out";
      ::close(fd);
      return 300;
    }
    break;
  }
  int optval;
  socklen_t optval_len =  sizeof(int);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optval_len)) {
    res_str = "error in connect: "+string(strerror(errno));
    ::close(fd);
    return 300;
  }

  if (optval) {
    res_str = "error in connect ("+int2str(optval)+")";
    ::close(fd);
    return 300;    
  } 

  return 0;
}


void JsonrpcNetstringsConnection::resetRead() {
  in_msg = false;
  msg_size = 0;
  rcvd_size = 0;
  msg_recv = true;
}

int JsonrpcNetstringsConnection::netstringsRead() {
  if (!in_msg) {
    while (true) {
      if (rcvd_size > MAX_NS_LEN_SIZE) {
	DBG("closing connection [%p/%d]: oversize length\n", this, fd);
	close();
	return REMOVE;
      }
      // reading length
      ssize_t r=read(fd,&msgbuf[rcvd_size],1);
      if (!r) {
	DBG("closing connection [%p/%d] on peer hangup\n", this, fd);
	close();
	return REMOVE;
      }

      if ((r<0 && errno == EAGAIN) || 
	  (r<0 && errno == EWOULDBLOCK))
	return CONTINUE;
     
      if (r != 1) {
	INFO("socket error on connection [%p/%d]: %s\n",
	     this, fd, strerror(errno));
	close();
	return REMOVE;
      }
      
      DBG("received '%c'\n", msgbuf[rcvd_size]);
      if (msgbuf[rcvd_size] == ':') {
	msgbuf[rcvd_size] = '\0';
	if (str2i(std::string(msgbuf, rcvd_size), msg_size)) {
	  ERROR("Protocol error decoding size '%s'\n", msgbuf);
	  close();
	  return REMOVE;
	}
	// received len - switch to receive msg mode
	in_msg = true;
	rcvd_size = read(fd,msgbuf,msg_size+1);
	// DBG("received '%.*s'\n", rcvd_size, msgbuf);

	if (rcvd_size == msg_size+1) { 
	  if (msgbuf[msg_size] == ',') {
	    msgbuf[msg_size+1] = '\0';
	    return DISPATCH;
	  }
	  INFO("Protocol error on connection [%p/%d]: netstring not terminated with ','\n",
	       this, fd);
	  close();
	  return REMOVE;
	}

	if (!rcvd_size) {
	  DBG("closing connection [%p/%d] on peer hangup\n", this, fd);
	  close();
	  return REMOVE;
	}

	if ((rcvd_size<0 && errno == EAGAIN) || 
	    (rcvd_size<0 && errno == EWOULDBLOCK))
	  return CONTINUE; // necessary?

	if (rcvd_size<0) {
	  INFO("socket error on connection [%p/%d]: %s\n",
	       this, fd, strerror(errno));
	  close();
	  return REMOVE;
	}
	       
	return CONTINUE; 	
      } 

      if (msgbuf[rcvd_size] < '0' || msgbuf[rcvd_size] > '9') {
	INFO("Protocol error on connection [%p/%d]: invalid character in size\n",
	     this, fd);
	close();
	return REMOVE;
      }
    
      rcvd_size++;
    }
  } else {
    ssize_t r = read(fd,msgbuf+rcvd_size,msg_size-rcvd_size+1);
    if (r>0) {
      rcvd_size += r;
      DBG("msgbuf='%.*s'\n", msg_size+1,msgbuf);
      if (rcvd_size == msg_size+1) { 
	DBG("msg_size = %d, rcvd_size = %d, <%c> \n", msg_size, rcvd_size, msgbuf[msg_size-1]);
	if (msgbuf[msg_size] == ',')
	  return DISPATCH;
	INFO("Protocol error on connection [%p/%d]: netstring not terminated with ','\n",
	     this, fd);
	close();
	return REMOVE;
      }
      return CONTINUE;
    }

    if (!r) {
      DBG("closing connection [%p/%d] on peer hangup\n", this, fd);
      close();
      return REMOVE;
    }

    if ((r<0 && errno == EAGAIN) || 
	(r<0 && errno == EWOULDBLOCK))
      return CONTINUE; // necessary?

    INFO("socket error on connection [%p/%d]: %s\n",
	 this, fd, strerror(errno));
    close();
    return REMOVE;
  }
}

int JsonrpcNetstringsConnection::netstringsBlockingWrite() {
  if (msg_size<0) {
    close();
    return REMOVE;
  }
  if (msg_size==0)
    return CONTINUE;

  // write size to snd_size
  string msg_size_s = int2str(msg_size);
  if (msg_size_s.length()>MAX_NS_LEN_SIZE) {
    ERROR("too large return message size len\n");
    close();
    return REMOVE;
  }
  char* ns_begin = msgbuf-(msg_size_s.length()+1);
  memcpy(ns_begin, 
	 msg_size_s.c_str(),
	 msg_size_s.length());
  ns_begin[msg_size_s.length()]=':';
  msgbuf[msg_size]=',';
  
  rcvd_size = 0;
  size_t ns_total_len = msg_size+msg_size_s.length()+2;
  while (rcvd_size != ns_total_len) {
    size_t written = send(fd, &ns_begin[rcvd_size], ns_total_len - rcvd_size, 
#ifdef MSG_NOSIGNAL
			  MSG_NOSIGNAL
#else
			  0
#endif
			  );
    if ((written<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) ||
	written==0) {
	usleep(SEND_SLEEP);
	continue;
    }
    if (written<0) {
      if (errno == ECONNRESET) {
	DBG("closing connection [%p/%d] on peer hangup\n", this, fd);
	close();
	return REMOVE;
      }
      INFO("error on connection [%p/%d]: %s\n", this, fd, strerror(errno));
      close();
      return REMOVE;
    }
    rcvd_size+=written;
  }

  rcvd_size = 0;
  msg_size = 0;
  return CONTINUE;
}

void JsonrpcNetstringsConnection::close() {
  if (fd>0) {
    shutdown(fd, SHUT_RDWR);
    ::close(fd);
  }
}

bool JsonrpcNetstringsConnection::messagePending() {
  return msg_size != 0;
}

bool JsonrpcNetstringsConnection::messageIsRecv() { 
  return msg_recv; 
}
