/*
 * Copyright (C) 2011 Raphael Coeffic
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

#include <string.h>

#include "RtmpServer.h"
#include "RtmpConnection.h"
#include "log.h"

#include "librtmp/rtmp.h"
#include "librtmp/log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CONN_BACKLOG 4

#define SAv4(addr) \
            ((struct sockaddr_in*)addr)


_RtmpServer::_RtmpServer()
  : AmThread(), fds_num(0)
{
}

_RtmpServer::~_RtmpServer()
{
  if(fds_num) {
    for(unsigned int i=0; i<fds_num; i++)
      ::close(fds[i].fd);
  }
}

int _RtmpServer::listen(const char* addr, unsigned short port)
{
  int listen_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(listen_fd < 0){
    ERROR("socket() failed: %s\n",strerror(errno));
    return -1;
  }
  int onoff=1;
  if(setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&onoff,sizeof(onoff))<0){
    ERROR("setsockopt(...,SO_REUSEADDR,...) failed: %s\n",strerror(errno));
    close(listen_fd);
    return -1;
  }

  memset(&listen_addr,0,sizeof(listen_addr));

  listen_addr.ss_family = AF_INET;
#if defined(BSD44SOCKETS)
  listen_addr.ss_len = sizeof(struct sockaddr_in);
#endif
  SAv4(&listen_addr)->sin_port = htons(port);

  if(inet_aton(addr,&SAv4(&listen_addr)->sin_addr)<0){
	
    ERROR("inet_aton: %s\n",strerror(errno));
    return -1;
  }

  if(bind(listen_fd,(const sockaddr*)&listen_addr,sizeof(struct sockaddr_in)) < 0){
    ERROR("bind() failed: %s\n",strerror(errno));
    close(listen_fd);
    return -1;
  }

  if(::listen(listen_fd,CONN_BACKLOG)<0){
    ERROR("listen() failed: %s\n",strerror(errno));
    close(listen_fd);
    return -1;
  }

  fds[0].fd = listen_fd;
  fds[0].events = POLLIN | POLLERR;
  fds_num++;

  return 0;
}

void _RtmpServer::run()
{
  RTMP_LogSetLevel(RTMP_LOGDEBUG);

  INFO("RTMP server started (%s:%i)\n",
       inet_ntoa(SAv4(&listen_addr)->sin_addr),
       ntohs(SAv4(&listen_addr)->sin_port));

  while(fds_num){
    int ret = poll(fds,fds_num,500/*ms*/);
    if(ret == 0){
      continue;
    }

    if(ret<0){
      switch(errno){
      case EAGAIN:
      case EINTR:
	continue;
      default:
	ERROR("poll() failed: %s\n",strerror(errno));
	return;
      }
    }

    for(unsigned int i=0; i<fds_num; i++){
      if(fds[i].revents != 0){
	if(i == 0){
	  if(fds[i].revents & POLLIN){
	    struct sockaddr_in remote_addr;
	    socklen_t remote_addr_len = sizeof(remote_addr);
	    int new_fd = accept(fds[i].fd,(struct sockaddr*)&remote_addr,
				&remote_addr_len);
	    if(new_fd < 0){
	      ERROR("accept() failed: %s\n",strerror(errno));
	      continue;
	    }
	    RtmpConnection * conn = new RtmpConnection(new_fd);
	    conn->start();
	  } else {
	    // POLLERR or POLLHUP
	    ERROR("on socket %i",fds[i].fd);
	    close(fds[i].fd);
	    if(fds_num != 1){
	      fds[i] = fds[fds_num-1];
	    }
	    memset(&(fds[fds_num-1]),0,sizeof(struct pollfd));
	    fds_num--;
	  }
	}
      }
    }
  }

  INFO("RTMP event loop finished/n");
}

void _RtmpServer::on_stop()
{
  ERROR("not yet supported!\n");
}

void _RtmpServer::dispose()
{
  ERROR("not yet supported!\n");
}
