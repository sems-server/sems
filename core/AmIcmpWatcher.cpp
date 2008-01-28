#include "AmIcmpWatcher.h"
#include "AmRtpStream.h"
#include "log.h"

#include <string>

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#define __FAVOR_BSD /* only for linux */
#include <netinet/udp.h>

AmIcmpWatcher* AmIcmpWatcher::_instance=0;

AmIcmpWatcher* AmIcmpWatcher::instance()
{
  if(!_instance)
    _instance = new AmIcmpWatcher();

  return _instance;
}

AmIcmpWatcher::AmIcmpWatcher()
  : raw_sd(-1)
{
}

AmIcmpWatcher::~AmIcmpWatcher()
{
  if(raw_sd != -1)
    close(raw_sd);
}

void AmIcmpWatcher::run()
{
  raw_sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
  if(raw_sd == -1){
    ERROR("ICMP Watcher: could not create RAW socket: %s\n",strerror(errno));
    ERROR("ICMP Watcher: try to run SEMS as root or suid.\n");
    return;
  }

  int  raw_sz;
  char msg_buf[ICMP_BUF_SIZE];

  struct sockaddr_in from;
  socklen_t          from_len;
  char               from_str[INET_ADDRSTRLEN];

  struct ip          *ip_hdr1,*ip_hdr2;
  size_t             hlen_ip1,hlen_ip2;

  struct icmp*       icmp_hdr;
  size_t             icmp_len;
  std::string        icmp_type_str;

  char               src_str[INET_ADDRSTRLEN];
  char               dst_str[INET_ADDRSTRLEN];

  while(true){
	
    from_len = sizeof(from);
    raw_sz = recvfrom(raw_sd, msg_buf, ICMP_BUF_SIZE, 0, 
		      (struct sockaddr*)&from, &from_len);

    inet_ntop(AF_INET,&from.sin_addr,from_str,INET_ADDRSTRLEN);

    ip_hdr1  = (struct ip*)msg_buf;
    hlen_ip1 = ip_hdr1->ip_hl << 2; /* convert to bytes */

    icmp_hdr = (struct icmp*)(msg_buf + hlen_ip1);
    icmp_len = raw_sz - hlen_ip1;

    /* if ICMP smaller than minimal length */
    if(icmp_len < 8){
      ERROR("icmp_len < 8\n");
      continue;
    }

    // 	DBG("%d bytes ICMP from %s: type = %d  code = %d\n",
    // 	    raw_sz,from_str,icmp_hdr->icmp_type,icmp_hdr->icmp_code);

    switch(icmp_hdr->icmp_type){
    case ICMP_UNREACH:
      icmp_type_str = "Destination Unreachable";
      break;
    case ICMP_SOURCEQUENCH:
      icmp_type_str = "Source Quench";
      break;
    case ICMP_TIMXCEED:
      icmp_type_str = "Time Exceeded";
      break;
    default:
      continue;
    }

    /* if ICMP smaller than expected length */
    if(icmp_len < 8 + 20 + 8){
      ERROR("icmp_len < 8 + 20 + 8\n");
      continue;
    }

    ip_hdr2  = (struct ip*)(msg_buf + hlen_ip1 + 8);
    hlen_ip2 = ip_hdr2->ip_hl << 2;
	
    inet_ntop(AF_INET,&ip_hdr2->ip_src,src_str,INET_ADDRSTRLEN);
    inet_ntop(AF_INET,&ip_hdr2->ip_dst,dst_str,INET_ADDRSTRLEN);

    if(ip_hdr2->ip_p == IPPROTO_UDP){
	    
      struct udphdr* udp_hdr = 
	(struct udphdr*)(msg_buf + hlen_ip1 + 8 + hlen_ip2);

      int srcport = ntohs(udp_hdr->uh_sport);
      int dstport = ntohs(udp_hdr->uh_dport);
	    
      stream_map_m.lock();
      std::map<int,AmRtpStream*>::iterator str_it = stream_map.find(srcport);

      if(str_it != stream_map.end()){

	DBG("ICMP from %s: type='%s' src=%s:%d dst=%s:%d\n",
	    from_str,icmp_type_str.c_str(),
	    src_str,srcport,dst_str,dstport);

	IcmpReporter* rep = new IcmpReporter(str_it->second);
	rep->start();
	AmThreadWatcher::instance()->add(rep);
      }
      stream_map_m.unlock();
    }
  }
}

void AmIcmpWatcher::on_stop()
{
}

void AmIcmpWatcher::addStream(int localport, AmRtpStream* str)
{
  stream_map_m.lock();
  stream_map[localport] = str;
  stream_map_m.unlock();
}

void AmIcmpWatcher::removeStream(int localport)
{
  stream_map_m.lock();
  stream_map.erase(localport);
  stream_map_m.unlock();
}

void IcmpReporter::run()
{
  rtp_str->icmpError();
}

void IcmpReporter::on_stop()
{
}

IcmpReporter::IcmpReporter(AmRtpStream* str)
  : rtp_str(str)
{
}
