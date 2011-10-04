/*
 * Copyright (C) 2011 Raphael Coeffic
 *
 * For the code parts originating from rtmpdump (rtmpsrv.c):
 *  Copyright (C) 2009 Andrej Stepanchuk
 *  Copyright (C) 2009 Howard Chu
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
#include "RtmpUtils.h"
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

int RtmpSender::SendConnectResult(double txn)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);
  AMFObject obj;
  AMFObjectProperty p, op;
  AVal av;

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "FMS/3,5,1,525");
  enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  STR2AVAL(av, "NetConnection.Connect.Success");
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
  STR2AVAL(av, "Connection succeeded.");
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, p_rtmp->m_fEncoding);
  STR2AVAL(p.p_name, "version");
  STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
  p.p_type = AMF_STRING;
  obj.o_num = 1;
  obj.o_props = &p;
  op.p_type = AMF_OBJECT;
  STR2AVAL(op.p_name, "data");
  op.p_vu.p_object = obj;
  enc = AMFProp_Encode(&op, enc, pend);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;

  return push_back(packet);
}

int RtmpSender::SendRegisterResult(double txn, const char* str)
{
  RTMPPacket packet;
  char pbuf[512], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  AVal av;
  char *enc = packet.m_body;

  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_NULL;
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  STR2AVAL(av, str);
  enc = AMF_EncodeNamedString(enc, pend, &av_uri, &av);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;

  return push_back(packet);
}

int RtmpSender::SendErrorResult(double txn, const char* str)
{
  RTMPPacket packet;
  char pbuf[512], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  AVal av;
  char *enc = packet.m_body;

  enc = AMF_EncodeString(enc, pend, &av__error);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_NULL;
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_error);
  STR2AVAL(av, str);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;

  return push_back(packet);
}

int RtmpSender::SendResultNumber(double txn, double ID)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_NULL;
  enc = AMF_EncodeNumber(enc, pend, ID);

  packet.m_nBodySize = enc - packet.m_body;

  return push_back(packet);
}


int RtmpSender::SendPlayStart()
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);

  *enc++ = AMF_NULL;//rco: seems to be needed
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &p_rtmp->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return push_back(packet);
}

int RtmpSender::SendPlayStop()
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);

  *enc++ = AMF_NULL;//rco: needed!
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Stop);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Stopped_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &p_rtmp->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return push_back(packet);
}

int RtmpSender::SendPause(int DoPause, int iTime)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf + sizeof(pbuf);
  char *enc;

  packet.m_nChannel = 0x08;	/* video channel */
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = 0x14;	/* invoke */
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_pause);
  enc = AMF_EncodeNumber(enc, pend, ++p_rtmp->m_numInvokes);
  *enc++ = AMF_NULL;
  enc = AMF_EncodeBoolean(enc, pend, DoPause);
  enc = AMF_EncodeNumber(enc, pend, (double)iTime);

  packet.m_nBodySize = enc - packet.m_body;

  DBG("%d, pauseTime=%d", DoPause, iTime);
  return push_back(packet);
}

/*
from http://jira.red5.org/confluence/display/docs/Ping:

Ping is the most mysterious message in RTMP and till now we haven't fully interpreted it yet. In summary, Ping message is used as a special command that are exchanged between client and server. This page aims to document all known Ping messages. Expect the list to grow.

The type of Ping packet is 0x4 and contains two mandatory parameters and two optional parameters. The first parameter is the type of Ping and in short integer. The second parameter is the target of the ping. As Ping is always sent in Channel 2 (control channel) and the target object in RTMP header is always 0 which means the Connection object, it's necessary to put an extra parameter to indicate the exact target object the Ping is sent to. The second parameter takes this responsibility. The value has the same meaning as the target object field in RTMP header. (The second value could also be used as other purposes, like RTT Ping/Pong. It is used as the timestamp.) The third and fourth parameters are optional and could be looked upon as the parameter of the Ping packet. Below is an unexhausted list of Ping messages.

    * type 0: Clear the stream. No third and fourth parameters. The second parameter could be 0. After the connection is established, a Ping 0,0 will be sent from server to client. The message will also be sent to client on the start of Play and in response of a Seek or Pause/Resume request. This Ping tells client to re-calibrate the clock with the timestamp of the next packet server sends.
    * type 1: Tell the stream to clear the playing buffer.
    * type 3: Buffer time of the client. The third parameter is the buffer time in millisecond.
    * type 4: Reset a stream. Used together with type 0 in the case of VOD. Often sent before type 0.
    * type 6: Ping the client from server. The second parameter is the current time.
    * type 7: Pong reply from client. The second parameter is the time the server sent with his ping request.
    * type 26: SWFVerification request
    * type 27: SWFVerification response
*/
int
RtmpSender::SendCtrl(short nType, unsigned int nObject, unsigned int nTime)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf + sizeof(pbuf);
  int nSize;
  char *buf;

  DBG("sending ctrl. type: 0x%04x", (unsigned short)nType);

  packet.m_nChannel = 0x02;	/* control channel (ping) */
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = 0x04;	/* ctrl */
  packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  switch(nType) {
  case 0x03: nSize = 10; break;	/* buffer time */
  case 0x1A: nSize = 3; break;	/* SWF verify request */
  case 0x1B: nSize = 44; break;	/* SWF verify response */
  default: nSize = 6; break;
  }

  packet.m_nBodySize = nSize;

  buf = packet.m_body;
  buf = AMF_EncodeInt16(buf, pend, nType);

  if (nType == 0x1B)
    {
#ifdef CRYPTO
      memcpy(buf, rtmp.Link.SWFVerificationResponse, 42);
      DBG("Sending SWFVerification response: ");
      RTMP_LogHex(RTMP_LOGDEBUG, (uint8_t *)packet.m_body, packet.m_nBodySize);
#endif
    }
  else if (nType == 0x1A)
    {
	  *buf = nObject & 0xff;
	}
  else
    {
      if (nSize > 2)
	buf = AMF_EncodeInt32(buf, pend, nObject);

      if (nSize > 6)
	buf = AMF_EncodeInt32(buf, pend, nTime);
    }

  return push_back(packet);
}

int RtmpSender::SendStreamBegin()
{
  return SendCtrl(0, 1, 0);
}

int RtmpSender::SendStreamEOF()
{
  return SendCtrl(1, 1, 0);
}

int RtmpSender::SendCallStatus(int status)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);

  *enc++ = AMF_NULL;//rco: needed!
  *enc++ = AMF_OBJECT;
  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_Sono_Call_Status);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_status_code, status);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return push_back(packet);
}

int RtmpSender::NotifyIncomingCall(const string& uri)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);

  *enc++ = AMF_NULL;//rco: needed!
  *enc++ = AMF_OBJECT;
  AVal tmp_uri = _AVC(uri.c_str());
  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_Sono_Call_Incoming);
  enc = AMF_EncodeNamedString(enc, pend, &av_uri, &tmp_uri);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return push_back(packet);
}

