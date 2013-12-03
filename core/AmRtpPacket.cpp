/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#define __APPLE_USE_RFC_3542
#include <netinet/in.h>

#include "AmRtpPacket.h"
#include "rtp/rtp.h"
#include "log.h"
#include "AmConfig.h"

#include "sip/raw_sender.h"
#include "sip/ip_util.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "sip/msg_logger.h"

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

void AmRtpPacket::setAddr(struct sockaddr_storage* a)
{
  memcpy(&addr,a,sizeof(sockaddr_storage));
}

void AmRtpPacket::getAddr(struct sockaddr_storage* a)
{
  memcpy(a,&addr,sizeof(sockaddr_storage));
}

int AmRtpPacket::parse()
{
  assert(buffer);
  assert(b_size);

  rtp_hdr_t* hdr = (rtp_hdr_t*)buffer;
  // ZRTP "Hello" packet has version == 0
  if ((hdr->version != RTP_VERSION) && (hdr->version != 0))
      {
	DBG("received RTP packet with unsupported version (%i).\n",
	    hdr->version);
	return -1;
      }

  data_offset = sizeof(rtp_hdr_t) + (hdr->cc*4);

  if(hdr->x != 0) {
    //#ifndef WITH_ZRTP 
    //if (AmConfig::IgnoreRTPXHdrs) {
    //  skip the extension header
    //#endif
    if (b_size >= data_offset + 4) {
      data_offset +=
	ntohs(((rtp_xhdr_t*) (buffer + data_offset))->len)*4;
    }
    // #ifndef WITH_ZRTP
    //   } else {
    //     DBG("RTP extension headers not supported.\n");
    //     return -1;
    //   }
    // #endif
  }

  payload = hdr->pt;
  marker = hdr->m;
  sequence = ntohs(hdr->seq);
  timestamp = ntohl(hdr->ts);
  ssrc = ntohl(hdr->ssrc);
  version = hdr->version;

  if (data_offset > b_size) {
    ERROR("bad rtp packet (hdr-size=%u;pkt-size=%u) !\n",
	  data_offset,b_size);
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

unsigned char *AmRtpPacket::getBuffer()
{
  return &buffer[0];
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

int AmRtpPacket::compile_raw(unsigned char* data_buf, unsigned int size)
{
  if ((!size) || (!data_buf))
    return -1;

  if(size>sizeof(buffer)){
    ERROR("builtin buffer size (%d) exceeded: %d\n",
	  (int)sizeof(buffer), size);
    return -1;
  }

  memcpy(&buffer[0], data_buf, size);
  b_size = size;

  return size;
}

int AmRtpPacket::sendto(int sd)
{
  int err = ::sendto(sd,buffer,b_size,0,
		     (const struct sockaddr *)&addr,
		     SA_len(&addr));

  if(err == -1){
    ERROR("while sending RTP packet: %s\n",strerror(errno));
    return -1;
  }

  return 0;
}

int AmRtpPacket::sendmsg(int sd, unsigned int sys_if_idx)
{
  struct msghdr hdr;
  struct cmsghdr* cmsg;
    
  union {
    char cmsg4_buf[CMSG_SPACE(sizeof(struct in_pktinfo))];
    char cmsg6_buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  } cmsg_buf;

  struct iovec msg_iov[1];
  msg_iov[0].iov_base = (void*)buffer;
  msg_iov[0].iov_len  = b_size;

  bzero(&hdr,sizeof(hdr));
  hdr.msg_name = (void*)&addr;
  hdr.msg_namelen = SA_len(&addr);
  hdr.msg_iov = msg_iov;
  hdr.msg_iovlen = 1;

  bzero(&cmsg_buf,sizeof(cmsg_buf));
  hdr.msg_control = &cmsg_buf;
  hdr.msg_controllen = sizeof(cmsg_buf);

  cmsg = CMSG_FIRSTHDR(&hdr);
  if(addr.ss_family == AF_INET) {
    cmsg->cmsg_level = IPPROTO_IP;
    cmsg->cmsg_type = IP_PKTINFO;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));

    struct in_pktinfo* pktinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
    pktinfo->ipi_ifindex = sys_if_idx;
  }
  else if(addr.ss_family == AF_INET6) {
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
    
    struct in6_pktinfo* pktinfo = (struct in6_pktinfo*) CMSG_DATA(cmsg);
    pktinfo->ipi6_ifindex = sys_if_idx;
  }

  hdr.msg_controllen = cmsg->cmsg_len;
  
  // bytes_sent = ;
  if(::sendmsg(sd, &hdr, 0) < 0) {
      ERROR("sendto: %s\n",strerror(errno));
      return -1;
  }

  return 0;
}

int AmRtpPacket::send(int sd, unsigned int sys_if_idx,
		      sockaddr_storage* l_saddr)
{
  if(sys_if_idx && AmConfig::UseRawSockets) {
    return raw_sender::send((char*)buffer,b_size,sys_if_idx,l_saddr,&addr);
  }

  if(sys_if_idx && AmConfig::ForceOutboundIf) {
    return sendmsg(sd,sys_if_idx);
  }
  
  return sendto(sd);
}

int AmRtpPacket::recv(int sd)
{
  socklen_t recv_addr_len = sizeof(struct sockaddr_storage);
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

void AmRtpPacket::logReceived(msg_logger *logger, struct sockaddr_storage *laddr)
{
  static const cstring empty;
  logger->log((const char *)buffer, b_size, &addr, laddr, empty);
}

void AmRtpPacket::logSent(msg_logger *logger, struct sockaddr_storage *laddr)
{
  static const cstring empty;
  logger->log((const char *)buffer, b_size, laddr, &addr, empty);
}

