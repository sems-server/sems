#include "StatsUDPServer.h"
#include "Statistics.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "log.h"

#include <string>
using std::string;

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#define CTRL_MSGBUF_SIZE 2048
// int msg_get_line(char*& msg_c, char* str, size_t len)
// {
//     size_t l;
//     char*  s=str;

//     if(!len)
// 	return 0;
    
//     for(l=len; l && (*msg_c) && (*msg_c !='\n'); msg_c++ ){
// 	if(*msg_c!='\r'){
// 	    *(s++) = *msg_c;
// 	    l--;
// 	}
//     }

//     if(*msg_c)
// 	msg_c++;

//     if(l>0){
// 	// We need one more character
// 	// for trailing '\0'.
// 	*s='\0';

// 	return int(s-str);
//     }
//     else
// 	// buffer overran.
// 	return -1;
// }

// int msg_get_param(char*& msg_c, string& p)
// {
//     char line_buf[MSG_BUF_SIZE];

//     if( msg_get_line(msg_c,line_buf,MSG_BUF_SIZE) != -1 ){

// 	if(!strcmp(".",line_buf))
// 	    line_buf[0]='\0';

// 	p = line_buf;
// 	return 0;
//     }

//     return -1;
// }

StatsUDPServer* StatsUDPServer::_instance=0;

StatsUDPServer* StatsUDPServer::instance()
{
    if(!_instance) {
	_instance = new StatsUDPServer();
	if(_instance->init() != 0){
	    delete _instance;
	    _instance = 0;
	}
	else {
	    _instance->start();
	}
    }
    return _instance;
}

StatsUDPServer::StatsUDPServer()
    : sd(0)
{
    sc = AmSessionContainer::instance();
}

StatsUDPServer::~StatsUDPServer()
{
    if(sd)
	close(sd);
}

int StatsUDPServer::init()
{
    string udp_addr;
    int    udp_port = 0;
    int    optval;

    AmConfigReader cfg;
    if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1, MOD_NAME ".conf")))
	return -1;

    udp_port = (int)cfg.getParameterInt("monit_udp_port",(unsigned int)-1);
    if(udp_port == -1){
	ERROR("invalid port number in the monit_udp_port parameter\n ");
	return -1;
    }
    if(!udp_port)
	udp_port = DEFAULT_MONIT_UDP_PORT;

    DBG("udp_port = %i\n",udp_port);
    udp_addr = cfg.getParameter("monit_udp_ip","");

    sd = socket(PF_INET,SOCK_DGRAM,0);
    if(sd == -1){
	ERROR("could not open socket: %s\n",strerror(errno));
	return -1;
    }

    /* set sock opts? */
    optval=1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, sizeof(optval)) ==-1){
        ERROR("ERROR: setsockopt(reuseaddr): %s\n", strerror(errno));
	return -1;	
    }
    /* tos */
    optval=IPTOS_LOWDELAY;
    if (setsockopt(sd, IPPROTO_IP, IP_TOS, (void*)&optval, sizeof(optval)) ==-1){
        ERROR("WARNING: setsockopt(tos): %s\n", strerror(errno));
	/* continue since this is not critical */
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(udp_port);
    
    if(!inet_aton(udp_addr.c_str(),(in_addr*)&sa.sin_addr.s_addr)){
	// non valid address
	ERROR("invalid IP in the monit_udp_ip parameter\n");
	return -1;
    }

    //bool socket_bound = false;
    //while(!socket_bound){
	if( bind(sd,(sockaddr*)&sa,sizeof(struct sockaddr_in)) == -1 ){
	    ERROR("could not bind socket at port %i: %s\n",udp_port,strerror(errno));
	    //udp_port += 1;
	    //sa.sin_port = htons(udp_port);

	    return -1;
	}
	else{
	    DBG("socket bound at port %i\n",udp_port);
	    //socket_bound = true;
	}
	//}

	return 0;
}

void StatsUDPServer::run()
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    char msg_buf[MSG_BUF_SIZE];
    int  msg_buf_s;

    while(true){

	msg_buf_s = recvfrom(sd,msg_buf,MSG_BUF_SIZE,0,(sockaddr*)&addr,&addrlen);
	if(msg_buf_s == -1){

	    switch(errno){
		case EINTR:
		case EAGAIN:
		    continue;
		default: break;
	    };

	    ERROR("recvfrom: %s\n",strerror(errno));
	    break;
	}

	//printf("received packet from: %s:%i\n",
	//       inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));

	string             reply;
	struct sockaddr_in reply_addr;

	if(execute(msg_buf,reply,reply_addr) == -1)
	    continue;

	send_reply(reply,addr);
    }
    
}



int StatsUDPServer::execute(char* msg_buf, string& reply, 
			    struct sockaddr_in& addr)
{
    char buffer[CTRL_MSGBUF_SIZE];
    string cmd_str,reply_addr,reply_port;
    char *msg_c = msg_buf;

    msg_get_param(msg_c,cmd_str,buffer,CTRL_MSGBUF_SIZE);

    if(cmd_str == "calls")
	reply = "Active calls: " + int2str(sc->getSize()) + "\n";
    else
	reply = "Unknown command: '" + cmd_str + "'\n";

    return 0;
}

int StatsUDPServer::send_reply(const string& reply,
			       const struct sockaddr_in& reply_addr)
{
    int err = sendto(sd,reply.c_str(),reply.length()+1,0,
		     (const sockaddr*)&reply_addr,
		     sizeof(struct sockaddr_in));

    return (err <= 0) ? -1 : 0;
}
