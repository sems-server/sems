#include "AmRtpPacketTracer.h"
#include "AmRtpPacket.h"
#include "AmRtpAudio.h"
#include "AmPlugIn.h"
#include "AmUtils.h"
#include "AmSdp.h"

#include "traceloader.h"
#include "log.h"

#include <sys/time.h>
#include <netinet/in.h>

#ifdef SUPPORT_IPV6
typedef struct sockaddr_storage mysockaddr_t;
#else
typedef struct sockaddr_in mysockaddr_t; 
#endif

class TraceModule {

    AmRtpPacket    rp;
    AmRtpAudio     rtp_a;
    AmAudioFile    af;

    unsigned int   ts;
    struct timeval tv;
    mysockaddr_t   recv_addr;

    void bufferPacket();

public:
    TraceModule() : af() {}
    virtual ~TraceModule(){}

    /**
     * Every function return != 0
     * if error occured.
     */

    virtual int onInit(){ return 0; }
    virtual int onPacket(){ return 0; }

    /**
     * Please don't mess with the current packet's datas 
     * as the next one has already been loaded...
     */
    virtual int processAudio(){ return 0; }

    int read(AmRtpPacketTracer& trace);
};

void TraceModule::bufferPacket()
{
    AmRtpPacket* pp = new AmRtpPacket();
    pp->copy(&rp);
    rtp_a.bufferPacket(pp);
}

int TraceModule::read(AmRtpPacketTracer& trace)
{
    int err;
    if((err = trace.read_packet(ts,&tv,&recv_addr,&rp)) != 0) 
	return -1;

    unsigned int last_ts = ts;

    SdpPayload pl;
    pl.int_pt = rp.payload;
    pl.payload_type = rp.payload;

    rtp_a.init(&pl);

    if(af.open("out.wav",AmAudioFile::Write)!=0){
	ERROR("while opening audio file\n");
	return -1;
    }

    if( ((err = onInit()) == 0) &&
	((err = onPacket()) == 0) ) {

	bufferPacket();

	unsigned char buffer[AUDIO_BUFFER_SIZE];
	while(!err){
	    
	    if(ts != last_ts){
		rtp_a.receive(last_ts);
		int s = rtp_a.get(last_ts,buffer,160);
		af.put(last_ts,buffer,(unsigned int)s);
		if((err = processAudio()) != 0)
		    break;
		last_ts = ts;
	    }
	    
	    if((err = onPacket()) != 0)
		break;
	    
	    bufferPacket();
	    err = trace.read_packet(ts,&tv,&recv_addr,&rp);
	}
    }
    
    return (err == -1) ? -1 : 0;
}


void print_usage(char* progname)
{
    printf("Usage: %s trace_file\n",
	   progname);
}

int main(int argc, char** argv)
{
    if(argc != 2){
	print_usage(argv[0]);
	return -1;
    }

    log_level  = 3;
    log_stderr = 1;

    AmRtpPacketTracer trace;
    if(trace.open(argv[1],false)==-1){
	ERROR("while opening trace file\n");
	return -1;
    }

    AmPlugIn::instance()->load("lib/audio",AmPlugIn::Audio);
    
    TraceModule mod;
    return mod.read(trace);
}




// Schrottplatz !!!
//
// 	if(!begin_i){
// 		begin_ts = ts;
// 		ts = 0;
// 		last_recv_tv = rp.recv_time;
// 		memset(&diff_tv,0,sizeof(struct timeval));
// 		begin_i = true;
// 	}
// 	else {
// 		ts -= begin_ts;
// 		timersub(&rp.recv_time,&last_recv_tv,&diff_tv);
// 	}
// 	string addr = get_addr_str(recv_addr.sin_addr);
// 	addr += ":" + int2str(htons(recv_addr.sin_port));
// 	DBG("ts=%u; diff_recv=%lu addr=%s\n",
// 	    ts/8,diff_tv.tv_sec*1000 + diff_tv.tv_usec/1000,
// 	    addr.c_str());
// 	last_recv_tv = rp.recv_time;
	    
