#include "msg_logger.h"

#include "AmUtils.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

file_msg_logger::~file_msg_logger()
{
  fd_mut.lock();
  if(fd >= 0)
    close(fd);
  fd_mut.unlock();
}

int file_msg_logger::open(const char* filename)
{
  fd_mut.lock();
  if(fd != -1) {
    ERROR("file already open\n");
    fd_mut.unlock();
    return -1;
  }
  
  fd = ::open(filename,O_WRONLY | O_CREAT | O_APPEND,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(fd < 0) {
    ERROR("could not open file '%s': %s", filename, strerror(errno));
    fd_mut.unlock();
    return -1;
  }

  fd_mut.unlock();
  return 0;
}

static string addr2str(sockaddr_storage* addr)
{
  char ntop_buffer[INET6_ADDRSTRLEN];

  if(addr->ss_family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    if(!inet_ntop(AF_INET, &sin->sin_addr,
		  ntop_buffer,INET6_ADDRSTRLEN)) {
      ERROR("Could not convert IPv4 address to string: %s",strerror(errno));
      return "unknown";
    }
    
    return string(ntop_buffer) + ":" + int2str(ntohs(sin->sin_port));
  }

  struct sockaddr_in6* sin6 = (struct sockaddr_in6 *)addr;
  if(!inet_ntop(AF_INET6, &sin6->sin6_addr,
		ntop_buffer,INET6_ADDRSTRLEN)) {
    ERROR("Could not convert IPv6 address to string: %s",strerror(errno));
    return "unknown";
  }
  
  return string(ntop_buffer) + ":" + int2str(ntohs(sin6->sin6_port));
}

#define WRITE_CSTSTR(fd,str)						\
  if(write(fd,str,sizeof(str)-1) != sizeof(str)-1) {			\
    ERROR("while writing to message log: %s\n",strerror(errno));	\
    return -1;								\
  }

#define WRITE_STLSTR(fd,str)						\
  if(write(fd,str.c_str(),str.length()) != (ssize_t)str.length())  {	\
    ERROR("while writing to message log: %s\n",strerror(errno));	\
    return -1;								\
  }


int file_msg_logger::write_src_dst(const string& obj)
{
  if (known_destinations.find(obj) == known_destinations.end()) {
    known_destinations.insert(obj);
    WRITE_CSTSTR(fd,"<object name='");
    WRITE_STLSTR(fd,obj);
    WRITE_CSTSTR(fd,"' desc='");
    WRITE_STLSTR(fd,obj);
    WRITE_CSTSTR(fd,"'/>\n");
  }

  return 0;
}

int file_msg_logger::log(const char* buf, int len,
			 sockaddr_storage* src_ip,
			 sockaddr_storage* dst_ip,
			 cstring method, int reply_code)
{
  string src = addr2str(src_ip);
  string dst = addr2str(dst_ip);

  AmLock _l(fd_mut);

  write_src_dst(src);
  write_src_dst(dst);
  
  string what = c2stlstr(method);
  if(reply_code > 0) {
    what = int2str(reply_code) + " / " + what;
  }

  WRITE_CSTSTR(fd,"<call src='");
  WRITE_STLSTR(fd,src);
  WRITE_CSTSTR(fd,"' dst='");
  WRITE_STLSTR(fd,dst);
  WRITE_CSTSTR(fd,"' desc='");
  WRITE_STLSTR(fd,what);
  WRITE_CSTSTR(fd,"'>\n");

  if(write(fd,buf,len) != len) {
    ERROR("while writing to message log: %s\n",strerror(errno));
    return -1;
  }

  WRITE_CSTSTR(fd,"</call>\n");
 
  return 0;
}

