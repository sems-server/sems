/*
 * $Id: $
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "udp_trsp.h"
#include "sip_parser.h"
#include "trans_layer.h"
#include "log.h"

#include "SipCtrlInterface.h"

#include <netinet/in.h>
#include <sys/param.h>
#include <arpa/inet.h>

#include <errno.h>
#include <string.h>

// FIXME: support IPv6

#if defined IP_RECVDSTADDR
# define DSTADDR_SOCKOPT IP_RECVDSTADDR
# define DSTADDR_DATASIZE (CMSG_SPACE(sizeof(struct in_addr)))
# define dstaddr(x) (CMSG_DATA(x))
#elif defined IP_PKTINFO
# define DSTADDR_SOCKOPT IP_PKTINFO
# define DSTADDR_DATASIZE (CMSG_SPACE(sizeof(struct in_pktinfo)))
# define dstaddr(x) (&(((struct in_pktinfo *)(CMSG_DATA(x)))->ipi_addr))
#else
# error "can't determine socket option (IP_RECVDSTADDR or IP_PKTINFO)"
#endif

// union control_data {
//     struct cmsghdr  cmsg;
//     u_char          data[DSTADDR_DATASIZE];
// };


udp_trsp::udp_trsp(trans_layer* tl)
    : transport(tl), sd(0)
{
    tl->register_transport(this);
}

udp_trsp::~udp_trsp()
{
}


const char* udp_trsp::get_local_ip()
{
    return local_ip.c_str();
}

unsigned short udp_trsp::get_local_port()
{
    return local_port;
}

void udp_trsp::copy_local_addr(sockaddr_storage* sa)
{
    memcpy(sa,&local_addr,sizeof(sockaddr_storage));
}

/** @see AmThread */
void udp_trsp::run()
{
    char buf[MAX_UDP_MSGLEN];
    int buf_len;

    msghdr           msg;
    //control_data     cmsg;
    cmsghdr*         cmsgptr; 
    sockaddr_storage from_addr;
    iovec            iov[1];

    iov[0].iov_base = buf;
    iov[0].iov_len  = MAX_UDP_MSGLEN;

    memset(&msg,0,sizeof(msg));
    msg.msg_name       = &from_addr;
    msg.msg_namelen    = sizeof(sockaddr_storage);
    msg.msg_iov        = iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = new u_char[DSTADDR_DATASIZE];
    msg.msg_controllen = DSTADDR_DATASIZE;

    if(sd<=0){
	ERROR("Transport instance not bound\n");
	return;
    }

    while(true){

	DBG("before recvmsg (%s:%i)\n",local_ip.c_str(),local_port);

	buf_len = recvmsg(sd,&msg,0);
	if(buf_len <= 0){
	    ERROR("recvfrom returned %d: %s\n",buf_len,strerror(errno));
	    switch(errno){
	    case EBADF:
	    case ENOTSOCK:
	    case EOPNOTSUPP:
		return;
	    }
	    continue;
	}

	if(buf_len > MAX_UDP_MSGLEN){
	    ERROR("Message was too big (>%d)\n",MAX_UDP_MSGLEN);
	    continue;
	}
	sip_msg* s_msg = new sip_msg(buf,buf_len);

	if (SipCtrlInterfaceFactory::log_raw_messages >= 0) {
	    _LOG(SipCtrlInterfaceFactory::log_raw_messages, 
		 "recvd msg\n--++--\n%s--++--\n", s_msg->buf);
	}
	memcpy(&s_msg->remote_ip,msg.msg_name,msg.msg_namelen);
	//msg->remote_ip_len = sizeof(sockaddr_storage);

	for (cmsgptr = CMSG_FIRSTHDR(&msg);
             cmsgptr != NULL;
             cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
	    
            if (cmsgptr->cmsg_level == IPPROTO_IP &&
                cmsgptr->cmsg_type == DSTADDR_SOCKOPT) {
		
		s_msg->local_ip.ss_family = AF_INET;
		((sockaddr_in*)(&s_msg->local_ip))->sin_port   = htons(local_port);
                memcpy(&((sockaddr_in*)(&s_msg->local_ip))->sin_addr,dstaddr(cmsgptr),sizeof(in_addr));
            }
        } 

	// pass message to the parser / transaction layer
	tl->received_msg(s_msg);
    }
}

/** @see AmThread */
void udp_trsp::on_stop()
{

}

    
/** @see transport */
int udp_trsp::bind(const string& address, unsigned short port)
{
    if(sd){
	WARN("re-binding socket\n");
	close(sd);
    }
    
    memset(&local_addr,0,sizeof(local_addr));
    local_addr.ss_family = AF_INET;
#if defined(BSD44SOCKETS)
    local_addr.ss_len = sizeof(struct sockaddr_in);
#endif
    SAv4(&local_addr)->sin_port = htons(port);

    if(inet_aton(address.c_str(),&SAv4(&local_addr)->sin_addr)<0){
	
	ERROR("inet_aton: %s\n",strerror(errno));
	return -1;
    }

    if(SAv4(&local_addr)->sin_addr.s_addr == INADDR_ANY){
	ERROR("Sorry, we cannot bind 'ANY' address\n");
	return -1;
    }

    if((sd = socket(PF_INET,SOCK_DGRAM,0)) == -1){
	ERROR("socket: %s\n",strerror(errno));
	return -1;
    } 
    

    if(::bind(sd,(const struct sockaddr*)&local_addr,
	     sizeof(struct sockaddr_in))) {

	ERROR("bind: %s\n",strerror(errno));
	close(sd);
	return -1;
    }
    
    int true_opt = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		  (void*)&true_opt, sizeof (true_opt)) == -1) {
	
	ERROR("%s\n",strerror(errno));
	close(sd);
	return -1;
    }

    if(setsockopt(sd, IPPROTO_IP, DSTADDR_SOCKOPT,
		  (void*)&true_opt, sizeof (true_opt)) == -1) {
	
	ERROR("%s\n",strerror(errno));
	close(sd);
	return -1;
    }

    local_port = port;
    local_ip   = address;

    DBG("UDP transport bound to %s:%i\n",address.c_str(),port);

    return 0;
}

/** @see transport */
int udp_trsp::send(const sockaddr_storage* sa, const char* msg, const int msg_len)
{
    if ((SipCtrlInterfaceFactory::log_raw_messages >= 0)
	&& (SipCtrlInterfaceFactory::log_raw_messages <=log_level)) {
	char buf[MAX_UDP_MSGLEN];
	memcpy(buf, msg, msg_len);
	buf[msg_len]='\0';
	_LOG(SipCtrlInterfaceFactory::log_raw_messages, 
	     "send  msg\n--++--\n%s--++--\n", buf);
    }

  int err;
#ifdef SUPPORT_IPV6
  if (sa->ss_family == AF_INET6) {
    err = sendto(sd, msg, msg_len, 0, (const struct sockaddr*)sa, sizeof(sockaddr_in6));
  }
  else {
#endif
    err = sendto(sd, msg, msg_len, 0, (const struct sockaddr*)sa, sizeof(sockaddr_in));
#ifdef SUPPORT_IPV6
  }
#endif

  if (err < 0) {
    ERROR("sendto: %s\n",strerror(errno));
    return err;
  }
  else if (err != msg_len) {
    ERROR("sendto: sent %i instead of %i bytes\n", err, msg_len);
    return -1;
  }

  return 0;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
