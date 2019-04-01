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

#include "RtmpAudio.h"
#include "RtmpSender.h"

#include "amci/codecs.h"

#define SPEEX_WB_SAMPLES_PER_FRAME 320
#define SPEEX_WB_SAMPLE_RATE 16000

#include <fcntl.h>
#include <sys/stat.h>

static void dump_audio(RTMPPacket *packet)
{
  static int dump_fd=0;
  if(dump_fd == 0){
    dump_fd = open("speex_in.raw",O_WRONLY|O_CREAT|O_TRUNC,
		   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if(dump_fd < 0)
      ERROR("could not open speex_in.raw: %s\n",strerror(errno));
  }
  if(dump_fd < 0) return;

  uint32_t pkg_size = packet->m_nBodySize-1;
  write(dump_fd,&pkg_size,sizeof(uint32_t));
  write(dump_fd,packet->m_body+1,pkg_size);
}

static void dump_audio(unsigned char* buffer, unsigned int size)
{
  static int dump_fd=0;
  if(dump_fd == 0){
    dump_fd = open("pcm_in.raw",O_WRONLY|O_CREAT|O_TRUNC,
		   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if(dump_fd < 0)
      ERROR("could not open pcm_in.raw: %s\n",strerror(errno));
  }
  if(dump_fd < 0) return;

  write(dump_fd,buffer,size);
}


RtmpAudio::RtmpAudio(RtmpSender* s)
  : AmAudio(new AmAudioFormat(CODEC_SPEEX_WB,SPEEX_WB_SAMPLE_RATE)),
    sender(s),
    playout_buffer(this,SPEEX_WB_SAMPLE_RATE), play_stream_id(0),
    recv_offset_i(false), recv_rtp_offset(0), recv_rtmp_offset(0),
    send_offset_i(false), send_rtmp_offset(0)
{
}

RtmpAudio::~RtmpAudio()
{
}

// returns bytes read, else -1 if error (0 is OK)
int RtmpAudio::get(unsigned long long system_ts, unsigned char* buffer, 
		   int output_sample_rate, unsigned int nb_samples)
{
  // - buffer RTMP audio
  // - read from RTMP recv buffer

  unsigned int user_ts = scaleSystemTS(system_ts);

  //DBG("get(%u, %u)\n",user_ts,nb_samples);
  process_recv_queue(user_ts);

  nb_samples = (unsigned int)((float)nb_samples * (float)getSampleRate()
			     / (float)output_sample_rate);

  u_int32_t size =
    PCM16_S2B(playout_buffer.read(user_ts,
				  (ShortSample*)((unsigned char*)samples),
				  nb_samples));

  if(output_sample_rate != getSampleRate()) {
    size = resampleOutput((unsigned char*)samples, size,
			  getSampleRate(), output_sample_rate);
  }
  
  memcpy(buffer,(unsigned char*)samples,size);

  return size;
}

// returns bytes written, else -1 if error (0 is OK)
int RtmpAudio::put(unsigned long long system_ts, unsigned char* buffer, 
		   int input_sample_rate, unsigned int size)
{
  if(!size){
    return 0;
  }

  //dump_audio((unsigned char*)buffer,size);

  // copy into internal buffer
  memcpy((unsigned char*)samples,buffer,size);
  size = resampleInput((unsigned char*)samples, size, 
		       input_sample_rate, getSampleRate());

  int s = encode(size);
  //DBG("s = %i\n",s);
  
  if(s<=0){
    return s;
  }

  return send(scaleSystemTS(system_ts),(unsigned int)s);
}

int RtmpAudio::send(unsigned int user_ts, unsigned int size)
{
  m_sender.lock();

  if(!sender || !play_stream_id) {
    //DBG("!sender || !play_stream_id");
    m_sender.unlock();
    return 0;
  }

  // - generate a new RTMP audio packet
  // - send packet

  RTMPPacket packet;
  RTMPPacket_Reset(&packet);

  packet.m_headerType  = send_offset_i ? 
    RTMP_PACKET_SIZE_MEDIUM 
    : RTMP_PACKET_SIZE_LARGE;

  packet.m_packetType  = RTMP_PACKET_TYPE_AUDIO;
  packet.m_nChannel    = 4;//TODO
  packet.m_nInfoField2 = play_stream_id;

  if(!send_offset_i){
    send_rtmp_offset = user_ts;
    send_offset_i = true;
  }
  
  unsigned int rtmp_ts = (user_ts - send_rtmp_offset) 
    / (SPEEX_WB_SAMPLE_RATE/1000);
  packet.m_nTimeStamp  = rtmp_ts;

  RTMPPacket_Alloc(&packet,size+1);
  packet.m_nBodySize = size+1;

// soundType 	(byte & 0x01) » 0 	
//   0: mono, 1: stereo
// soundSize 	(byte & 0x02) » 1 	
//   0: 8-bit, 1: 16-bit
// soundRate 	(byte & 0x0C) » 2 	
//   0: 5.5 kHz, 1: 11 kHz, 2: 22 kHz, 3: 44 kHz
// soundFormat 	(byte & 0xf0) » 4 	
//   0: Uncompressed, 1: ADPCM, 2: MP3, 5: Nellymoser 8kHz mono, 6: Nellymoser, 11: Speex 


  // 0xB2: speex, 16kHz
  packet.m_body[0] = 0xB2;
  memcpy(packet.m_body+1,(unsigned char*)samples,size);

  //DBG("sending audio packet: size=%u rtmp_ts=%u StreamID=%u (rtp_ts=%u)\n",
  //    size+1,rtmp_ts,play_stream_id,user_ts);

  sender->push_back(packet);
  m_sender.unlock();

  //dump_audio(&packet);
  RTMPPacket_Free(&packet);

  return size;
}

void RtmpAudio::bufferPacket(const RTMPPacket& p)
{
  RTMPPacket np = p;
  if(!RTMPPacket_Alloc(&np,np.m_nBodySize)){
    ERROR("could not allocate packet.\n");
    return;
  }
  memcpy(np.m_body,p.m_body,p.m_nBodySize);

  m_q_recv.lock();
  q_recv.push(np);
  m_q_recv.unlock();
}

void RtmpAudio::process_recv_queue(unsigned int ref_ts)
{
  int size;

  // flush the recv queue into the playout buffer
  m_q_recv.lock();
  while(!q_recv.empty()){

    RTMPPacket p = q_recv.front();
    q_recv.pop();
    m_q_recv.unlock();

    //TODO:
    // - copy RTMP payload into this->samples
    // - decode
    // - put packet in playout buffer

    if(p.m_nBodySize <= (unsigned int)AUDIO_BUFFER_SIZE){
      

      size = p.m_nBodySize-1;
      memcpy((unsigned char*)samples, p.m_body+1, size);

      size = decode(size);
      if(size <= 0){
	ERROR("decode() returned %i\n",size);
	return;
      }
      
      // TODO: generate some reasonable RTP timestamp
      //
      bool begin_talk = false;
      if(!recv_offset_i){
	recv_rtp_offset  = ref_ts;
	recv_rtmp_offset = p.m_nTimeStamp;
	recv_offset_i = true;
	begin_talk = true;
      }

      unsigned int rtp_ts = (p.m_nTimeStamp - recv_rtmp_offset) * (SPEEX_WB_SAMPLE_RATE/1000);

      playout_buffer.write(ref_ts, rtp_ts, (ShortSample*)((unsigned char *)samples),
			   PCM16_B2S(size), begin_talk);
      
      RTMPPacket_Free(&p);    
    }

    m_q_recv.lock();
  }
  m_q_recv.unlock();
}

void RtmpAudio::add_to_history(int16_t *, unsigned int)
{
  return;
}

unsigned int RtmpAudio::conceal_loss(unsigned int, unsigned char *)
{
  return 0;
}

void RtmpAudio::setSenderPtr(RtmpSender* s)
{
  m_sender.lock();
  DBG("sender ptr = %p\n",s);
  sender = s;
  m_sender.unlock();
}

void RtmpAudio::setPlayStreamID(unsigned int stream_id)
{
  m_sender.lock();
  DBG("play_stream_id = %i\n",stream_id);
  play_stream_id = stream_id;
  m_sender.unlock();
}
