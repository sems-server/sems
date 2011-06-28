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

void _RtmpServer::run()
{
  int listen_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(listen_fd < 0){
    ERROR("socket() failed: %s\n",strerror(errno));
    return;
  }
  int onoff=1;
  if(setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&onoff,sizeof(onoff))<0){
    ERROR("setsockopt(...,SO_REUSEADDR,...) failed: %s\n",strerror(errno));
    close(listen_fd);
    return;
  }

  struct sockaddr_in listen_addr;
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  listen_addr.sin_port = htons(DEFAULT_RTMP_PORT);

  if(bind(listen_fd,(const sockaddr*)&listen_addr,sizeof(listen_addr)) < 0){
    ERROR("bind() failed: %s\n",strerror(errno));
    close(listen_fd);
    return;
  }

  if(listen(listen_fd,CONN_BACKLOG)<0){
    ERROR("listen() failed: %s\n",strerror(errno));
    close(listen_fd);
    return;
  }

  fds[0].fd = listen_fd;
  fds[0].events = POLLIN | POLLERR;
  fds_num++;

  RTMP_LogSetLevel(RTMP_LOGDEBUG);

  INFO("RTMP server started (%s:%i)\n",
       inet_ntoa(listen_addr.sin_addr),
       ntohs(listen_addr.sin_port));

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

