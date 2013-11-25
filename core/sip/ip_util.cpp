#include "ip_util.h"
#include "log.h"

#include <string.h>
#include <errno.h>

#include <netdb.h>
#include <arpa/inet.h>

int am_inet_pton(const char* src, struct sockaddr_storage* dst)
{
  char src_addr[NI_MAXHOST];

  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;

  bool must_be_ipv6 = false;

  if(!src)
    return 0;

  size_t src_len = strlen(src);
  if(!src_len || (src_len > NI_MAXHOST-1))
    return 0;

  if( (src[0] == '[') &&
      (src[src_len-1] == ']') ) {

    // IPv6
    memcpy(src_addr,src+1,src_len-2);
    src_addr[src_len-2] = '\0';
    must_be_ipv6 = true;
  }
  else {
    // IPv4
    memcpy(src_addr,src,src_len+1);
  }

  sin = (struct sockaddr_in *)dst;
  sin6 = (struct sockaddr_in6 *)dst;
    
  if(!must_be_ipv6 && (inet_pton(AF_INET, src_addr, &sin->sin_addr) > 0)) {
    dst->ss_family = AF_INET;
    return 1;
  }

  if(inet_pton(AF_INET6, src_addr, &sin6->sin6_addr) > 0) {
    dst->ss_family = AF_INET6;
#ifdef SIN6_LEN
    sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
    return 1;
  }

  return 0;
}

const char* am_inet_ntop(const sockaddr_storage* addr, char* str, size_t size)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)addr;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

  if(addr->ss_family == AF_INET) {
    if(!inet_ntop(AF_INET,&sin->sin_addr,str,size)) {
      ERROR("Could not convert IPv4 address to string: %s",strerror(errno));
      return NULL;
    }
  }
  else {
    if(!inet_ntop(AF_INET6,&sin6->sin6_addr,str,size)) {
      ERROR("Could not convert IPv6 address to string: %s",strerror(errno));
      return NULL;
    }
  }

  return str;
}

string am_inet_ntop(const sockaddr_storage* addr)
{
  char host[NI_MAXHOST] = "";
  am_inet_ntop(addr,host,NI_MAXHOST);
  return host;
}

const char* am_inet_ntop_sip(const sockaddr_storage* addr, char* str, size_t size)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)addr;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

  if(addr->ss_family == AF_INET) {
    if(!inet_ntop(AF_INET,&sin->sin_addr,str,size)) {
      ERROR("Could not convert IPv4 address to string: %s",strerror(errno));
      return NULL;
    }
  }
  else {
    if(!inet_ntop(AF_INET6,&sin6->sin6_addr,str+1,size-2)) {
      ERROR("Could not convert IPv6 address to string: %s",strerror(errno));
      return NULL;
    }
    size_t str_len = strlen(str);
    str[0] = '[';
    str[str_len] = ']';
    str[str_len+1] = '\0';
  }

  return str;
}

void am_set_port(struct sockaddr_storage* addr, short port)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)addr;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

  if(addr->ss_family == AF_INET) {
    sin->sin_port = htons(port);
  } else {
    sin6->sin6_port = htons(port);
  }
}

unsigned short am_get_port(const sockaddr_storage* addr)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)addr;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

  if(addr->ss_family == AF_INET) {
    return ntohs(sin->sin_port);
  }

  return ntohs(sin6->sin6_port);
}


