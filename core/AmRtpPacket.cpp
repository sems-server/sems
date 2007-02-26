/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmRtpPacket.h"
#include "rtp/rtp.h"
#include "log.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

AmRtpPacket::AmRtpPacket()
    : data_offset(0)
{
    // buffer will be overwritten by received packet 
    // of hdr+data - does not need to be set to 0s
    //    memset(buffer,0,4096);
}

AmRtpPacket::~AmRtpPacket()
{
}

#ifdef SUPPORT_IPV6

void AmRtpPacket::setAddr(struct sockaddr_storage* a)
{
    memcpy(&addr,a,sizeof(sockaddr_storage));
}

void AmRtpPacket::getAddr(struct sockaddr_storage* a)
{
    memcpy(a,&addr,sizeof(sockaddr_storage));
}

#else

void AmRtpPacket::setAddr(struct sockaddr_in* a)
{
    memcpy(&addr,a,sizeof(sockaddr_in));
}

void AmRtpPacket::getAddr(struct sockaddr_in* a)
{
    memcpy(a,&addr,sizeof(sockaddr_in));
}

#endif

int AmRtpPacket::parse()
{
    assert(buffer);
    assert(b_size);

    rtp_hdr_t* hdr = (rtp_hdr_t*)buffer;

    if(hdr->version != RTP_VERSION){
	DBG("received RTP packet with unsupported version (%i).\n",
	    hdr->version);
	return -1;
    }

    if(hdr->x != 0){
	DBG("RTP extension headers not supported.\n");
	return -1;
    }

    payload = hdr->pt;
    marker = hdr->m;
    sequence = ntohs(hdr->seq);
    timestamp = ntohl(hdr->ts);
    ssrc = ntohl(hdr->ssrc);

    data_offset = sizeof(rtp_hdr_t) + (hdr->cc*4);
    if (data_offset >= b_size) {
 	ERROR("bad rtp packet (header size too big) !\n");
 	return -1;
    }
    d_size = b_size - data_offset;

    if(hdr->p){
 	if (buffer[b_size-1]>=d_size){
 		ERROR("bad rtp packet (invalid padding size) !\n");
 		return -1;
 	}
	d_size -= buffer[b_size-1];
    }

    return 0;
}

unsigned char *AmRtpPacket::getData()
{
    return &buffer[data_offset];
}

int AmRtpPacket::compile(unsigned char* data_buf, unsigned int size)
{
    assert(data_buf);
    assert(size);

    d_size = size;
    b_size = d_size + sizeof(rtp_hdr_t);
    assert(b_size <= 4096);
    rtp_hdr_t* hdr = (rtp_hdr_t*)buffer;

    if(b_size>sizeof(buffer)){
 	ERROR("builtin buffer size (%d) exceeded: %d\n",
	      (int)sizeof(buffer), b_size);
  	return -1;
    }

    memset(hdr,0,sizeof(rtp_hdr_t));
    hdr->version = RTP_VERSION;
    hdr->m = marker;
    hdr->pt = payload;
    
    hdr->seq = htons(sequence);
    hdr->ts = htonl(timestamp);
    hdr->ssrc = htonl(ssrc);
    
    data_offset = sizeof(rtp_hdr_t);
    memcpy(&buffer[data_offset],data_buf,d_size);

    return 0;
}

int AmRtpPacket::send(int sd)
{
    int err;
#ifdef SUPPORT_IPV6
    if(addr.ss_family != PF_INET)
	err = sendto(sd,buffer,b_size,0,
		     (const struct sockaddr *)saddr,
		     sizeof(struct sockaddr_in6));
    else 
#endif
	err = sendto(sd,buffer,b_size,0,
		     (const struct sockaddr *)&addr,
		     sizeof(struct sockaddr_in));

    if(err == -1){
	ERROR("while sending RTP packet: %s\n",strerror(errno));
	return -1;
    }

    return 0;
}

int AmRtpPacket::recv(int sd)
{
#ifdef SUPPORT_IPV6
    socklen_t recv_addr_len = sizeof(struct sockaddr_storage);
#else
    socklen_t recv_addr_len = sizeof(struct sockaddr_in);
#endif

    int ret = recvfrom(sd,buffer,sizeof(buffer),0,
		       (struct sockaddr*)&addr,
		       &recv_addr_len);

    if(ret > 0){

        if(ret > 4096)
	    return -1;

	b_size = ret;
    }
    
    return ret;
}

void AmRtpPacket::copy(const AmRtpPacket* p)
{
    memcpy(this,p,sizeof(AmRtpPacket));
    memcpy(buffer,p->buffer,b_size);
}
