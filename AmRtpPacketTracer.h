#ifndef _AmRtpPacketTracer_h_
#define _AmRtpPacketTracer_h_

#include "AmThread.h"

#include <sys/types.h>
#include <netinet/in.h>

#include <string>
using std::string;

class AmRtpPacket;

class AmRtpPacketTracer
{
    int trace_fd;

 public:
    AmRtpPacketTracer();
    ~AmRtpPacketTracer();

    int open(const string& trace_file, bool write = true);

    void write_packet(unsigned int ts,
#ifdef SUPPORT_IPV6
		      struct sockaddr_storage* recv_addr,
#else
		      struct sockaddr_in* recv_addr,
#endif
		      AmRtpPacket* rp);

    int read_packet(unsigned int& ts,
		    struct timeval* tv,
#ifdef SUPPORT_IPV6
		    struct sockaddr_storage* recv_addr,
#else
		    struct sockaddr_in* recv_addr,
#endif
		    AmRtpPacket* rp);
};


#endif
