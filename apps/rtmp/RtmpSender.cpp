/*
 * Copyright (C) 2011 Raphael Coeffic
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

#include <string.h>

#include "RtmpSender.h"
#include "log.h"

RtmpSender::RtmpSender(RTMP* r)
  : has_work(false),
    p_rtmp(r),
    running(false)
{
}

RtmpSender::~RtmpSender()
{
  // flush the packet queue
  m_q_send.lock();
  while(!q_send.empty()){
      RTMPPacket p = q_send.front();
      q_send.pop();
      RTMPPacket_Free(&p);
  }
  m_q_send.unlock();
}

int RtmpSender::push_back(const RTMPPacket& p)
{
  RTMPPacket np = p;
  if(!RTMPPacket_Alloc(&np,np.m_nBodySize)){
    ERROR("could not allocate packet.\n");
    return 0;
  }
  memcpy(np.m_body,p.m_body,p.m_nBodySize);

  m_q_send.lock();
  q_send.push(np);
  has_work.set(!q_send.empty());
  m_q_send.unlock();

  return 1;
}

void RtmpSender::run()
{
  running.set(true);

  while(running.get()){
    
    //wait for some work
    // (at most 1s)
    //DBG("waiting for work...\n");
    has_work.wait_for();//_to(1000);

    // send packets in the queue
    m_q_send.lock();
    while(!q_send.empty()){
      RTMPPacket p = q_send.front();
      q_send.pop();
      m_q_send.unlock();

      if((p.m_nBodySize > (unsigned)p_rtmp->m_outChunkSize) &&
	 (p.m_packetType == RTMP_PACKET_TYPE_AUDIO)) {
	// adapt chunk size to the maximum body size
	// (TODO: set a reasonable max value (spec: 64K))
	p_rtmp->m_outChunkSize = p.m_nBodySize;
	SendChangeChunkSize();
      }

      if(!RTMP_SendPacket(p_rtmp,&p,FALSE)) {
	ERROR("could not send packet.\n");
      }

      RTMPPacket_Free(&p);
      
      m_q_send.lock();
    }
    has_work.set(!q_send.empty());
    m_q_send.unlock();
  }
}

void RtmpSender::on_stop()
{
  running.set(false);
}

int RtmpSender::SendChangeChunkSize()
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = 0x02;
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
  packet.m_packetType = 0x01; // SetChunkSize
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
  
  packet.m_nBodySize = 4;

  AMF_EncodeInt32(packet.m_body, pend, p_rtmp->m_outChunkSize);
  DBG("changing send chunk size to %i\n",p_rtmp->m_outChunkSize);

  return RTMP_SendPacket(p_rtmp,&packet,FALSE);
}
