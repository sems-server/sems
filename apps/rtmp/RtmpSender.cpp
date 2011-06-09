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
    // (at most 5s)
    DBG("waiting for work...\n");
    has_work.wait_for();//_to(5000*1000);

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
