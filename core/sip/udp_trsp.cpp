/*
 * $Id: udp_trsp.cpp 1713 2010-03-30 14:11:14Z rco $
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
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

/** @see trsp_socket */
int udp_trsp_socket::bind(const string& bind_ip, unsigned short bind_port)
{
    if(sd){
	WARN("re-binding socket\n");
	close(sd);
    }
    
    memset(&addr,0,sizeof(addr));

    addr.ss_family = AF_INET;
#if defined(BSD44SOCKETS)
    addr.ss_len = sizeof(struct sockaddr_in);
#endif
    SAv4(&addr)->sin_port = htons(bind_port);

    if(inet_aton(bind_ip.c_str(),&SAv4(&addr)->sin_addr)<0){
	
	ERROR("inet_aton: %s\n",strerror(errno));
	return -1;
    }

    if(SAv4(&addr)->sin_addr.s_addr == INADDR_ANY){
	ERROR("Sorry, we cannot bind 'ANY' address\n");
	return -1;
    }

    if((sd = socket(PF_INET,SOCK_DGRAM,0)) == -1){
	ERROR("socket: %s\n",strerror(errno));
	return -1;
    } 
    

    if(::bind(sd,(const struct sockaddr*)&addr,
	     sizeof(struct sockaddr_in))) {

	ERROR("bind: %s\n",strerror(errno));
	close(sd);
	return -1;
    }
    
    int true_opt = 1;

    if(setsockopt(sd, IPPROTO_IP, DSTADDR_SOCKOPT,
		  (void*)&true_opt, sizeof (true_opt)) == -1) {
	
	ERROR("%s\n",strerror(errno));
	close(sd);
	return -1;
    }

    if (SipCtrlInterface::udp_rcvbuf > 0) {
	DBG("trying to set SIP UDP socket buffer to %d\n",
	    SipCtrlInterface::udp_rcvbuf);
	if(setsockopt(sd, SOL_SOCKET, SO_RCVBUF,
		      (void*)&SipCtrlInterface::udp_rcvbuf,
		      sizeof (SipCtrlInterface::udp_rcvbuf)) == -1) {
	    WARN("could not set SIP UDP socket buffer: '%s'\n",
		 strerror(errno));
	} else {
	    socklen_t optlen;
	    int set_rcvbuf_size=0;
	    if (getsockopt(sd, SOL_SOCKET, SO_RCVBUF,
			   &set_rcvbuf_size, &optlen) == -1) {
		WARN("could not read back SIP UDP socket buffer length: '%s'\n",
		     strerror(errno));
	    } else {
		if (set_rcvbuf_size != SipCtrlInterface::udp_rcvbuf) {
		    WARN("failed to set SIP UDP RCVBUF size (wanted %d, got %d)\n",
			 SipCtrlInterface::udp_rcvbuf, set_rcvbuf_size);
		}
	    }
	}
    }

    port = bind_port;
    ip   = bind_ip;

    DBG("UDP transport bound to %s:%i\n",ip.c_str(),port);

    return 0;
}

/** @see trsp_socket */

udp_trsp::udp_trsp(udp_trsp_socket* sock)
    : transport(sock)
{
}

udp_trsp::~udp_trsp()
{
}


/** @see AmThread */
void udp_trsp::run()
{
    char buf[MAX_UDP_MSGLEN];
    int buf_len;

    msghdr           msg;
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

    if(sock->get_sd()<=0){
	ERROR("Transport instance not bound\n");
	return;
    }

    DBG("Started UDP server listening to %s:%i\n",sock->get_ip(),sock->get_port());

    while(true){

	//DBG("before recvmsg (%s:%i)\n",sock->get_ip(),sock->get_port());

	buf_len = recvmsg(sock->get_sd(),&msg,0);
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

	if (SipCtrlInterface::log_raw_messages >= 0) {
	    _LOG(SipCtrlInterface::log_raw_messages, 
		 "recvd msg\n--++--\n%.*s--++--\n", s_msg->len, s_msg->buf);
	}
	memcpy(&s_msg->remote_ip,msg.msg_name,msg.msg_namelen);

	for (cmsgptr = CMSG_FIRSTHDR(&msg);
             cmsgptr != NULL;
             cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
	    
            if (cmsgptr->cmsg_level == IPPROTO_IP &&
                cmsgptr->cmsg_type == DSTADDR_SOCKOPT) {
		
		s_msg->local_ip.ss_family = AF_INET;
		((sockaddr_in*)(&s_msg->local_ip))->sin_port   = htons(sock->get_port());
                memcpy(&((sockaddr_in*)(&s_msg->local_ip))->sin_addr,dstaddr(cmsgptr),sizeof(in_addr));
            }
        } 

	// pass message to the parser / transaction layer
	trans_layer::instance()->received_msg(s_msg);
    }
}

/** @see AmThread */
void udp_trsp::on_stop()
{

}

    

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
