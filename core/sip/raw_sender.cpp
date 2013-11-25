#include "raw_sender.h"
#include "raw_sock.h"
#include "AmConfig.h"

#include "log.h"

#include <errno.h>
#include <string.h>

int raw_sender::rsock = -1;

int raw_sender::init()
{
  if(rsock >= 0) {
    return 0;
  }
  
  rsock = raw_udp_socket(1);
  if(rsock < 0) {
    if(errno == EPERM) {
      ERROR("SEMS must be running as root to be able to use raw sockets.");
      ERROR("-> raw socket usage will be deactivated.");
      return -1;
    }
    else {
      ERROR("raw_udp_socket(): %s",strerror(errno));
      ERROR("-> raw socket usage will be deactivated.");
      return -1;
    }
  }

  return 0;
}

int raw_sender::send(const char* buf, unsigned int len, int sys_if_idx,
		     const sockaddr_storage* from, const sockaddr_storage* to)
{
  //TODO: grab the MTU from the interface def
  int ret = raw_iphdr_udp4_send(rsock,buf,len,from,to,
				AmConfig::SysIfs[sys_if_idx].mtu);
  if(ret < 0) {
    ERROR("send(): %s",strerror(errno));
    return ret;
  }

  // if((unsigned int)ret < len) {
  //   ERROR("incomplete udp send (%i instead of %i)",ret,len);
  //   return -1;
  // }

  return 0;
}
