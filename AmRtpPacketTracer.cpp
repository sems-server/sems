#include "AmRtpPacketTracer.h"
#include "AmRtpPacket.h"
#include "log.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

AmRtpPacketTracer::AmRtpPacketTracer()
{
}

AmRtpPacketTracer::~AmRtpPacketTracer()
{
    if(trace_fd>=0)
	close(trace_fd);
}

int AmRtpPacketTracer::open(const string& trace_file, bool write)
{
    if(write)
	trace_fd = ::open(trace_file.c_str(),O_WRONLY|O_CREAT|O_TRUNC, DEFFILEMODE/*0666*/);
    else
	trace_fd = ::open(trace_file.c_str(),O_RDONLY);

    if(trace_fd<0){
	DBG("RTP packet tracer: could not open trace file (%s): %s\n",
		trace_file.c_str(),strerror(errno));
	return -1;
    }
    DBG("*** trace file @ %s opened ***\n",trace_file.c_str());
    return 0;
}

void AmRtpPacketTracer::write_packet(unsigned int ts,
#ifdef SUPPORT_IPV6
				     struct sockaddr_storage* recv_addr,
#else
				     struct sockaddr_in* recv_addr,
#endif
				     AmRtpPacket* rp)
{
    if(trace_fd<0)
	return;

    unsigned int s = sizeof(unsigned int) 
	+ 2*sizeof(struct timeval) 
	+ sizeof(struct sockaddr_in) 
	+ rp->b_size;

    struct timeval now;
    gettimeofday(&now,NULL);

    write(trace_fd,&s,sizeof(unsigned int));
    write(trace_fd,&ts,sizeof(unsigned int));
    write(trace_fd,&now,sizeof(struct timeval));
#ifdef SUPPORT_IPV6
    write(trace_fd,recv_addr,sizeof(struct sockaddr_storage));
#else
    write(trace_fd,recv_addr,sizeof(struct sockaddr_in));
#endif
    write(trace_fd,&rp->recv_time,sizeof(struct timeval));
    write(trace_fd,rp->buffer,rp->b_size);
}

#define SAFE_READ(ptr,s)\
          do{\
            int err = read(trace_fd,ptr,s);\
            if(err == -1){\
                ERROR("read_packet: %s\n",strerror(errno));\
                return -1;\
            }\
            if(err == 0){\
                DBG("read_packet: End of file reached\n");\
                return 1;\
            }\
            if(err != (s)){\
		ERROR("read_packet: partial read\n");\
                return -1;\
	    }\
	  }while(0)


/**
 * returns:  0: OK
 *          -1: Error
 *           1: EOF
 */
int AmRtpPacketTracer::read_packet(unsigned int& ts,
				   struct timeval* tv,
#ifdef SUPPORT_IPV6
				   struct sockaddr_storage* recv_addr,
#else
				   struct sockaddr_in* recv_addr,
#endif
				   AmRtpPacket* rp)
{
    if(trace_fd<0){
	ERROR("read_packet: trace file is not open.\n");
	return -1;
    }

    unsigned int s;
    SAFE_READ(&s,sizeof(unsigned int));
    SAFE_READ(&ts,sizeof(unsigned int));
    SAFE_READ(tv,sizeof(struct timeval));

#ifdef SUPPORT_IPV6
    SAFE_READ(recv_addr,sizeof(struct sockaddr_storage));
#else
    SAFE_READ(recv_addr,sizeof(struct sockaddr_in));
#endif
    SAFE_READ(&rp->recv_time,sizeof(struct timeval));

    rp->b_size = s - sizeof(unsigned int) 
	- 2*sizeof(struct timeval) 
	- sizeof(struct sockaddr_in);
    
    if(rp->buffer)
	delete [] rp->buffer;
    rp->buffer = new unsigned char [rp->b_size];
    if(!rp->buffer){
	ERROR("could not allocate memory for packet buffer (size=%u)\n",
	      rp->b_size);
	return -1;
    }

    SAFE_READ(rp->buffer,(int)rp->b_size);

    return 0;
}
