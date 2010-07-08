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

#include "AmRtpStream.h"
#include "AmRtpPacket.h"
#include "AmRtpReceiver.h"
#include "AmConfig.h"
#include "AmPlugIn.h"
#include "AmAudio.h"
#include "AmUtils.h"
#include "AmSession.h"

#include "AmDtmfDetector.h"
#include "rtp/telephone_event.h"
#include "AmJitterBuffer.h"

#include "log.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>       
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifdef WITH_ZRTP
#include "zrtp/zrtp.h"
#endif

#include <set>
using std::set;

/*
 * This function must be called before setLocalPort, because
 * setLocalPort will bind the socket and it will be not
 * possible to change the IP later
 */
void AmRtpStream::setLocalIP(const string& ip)
{
#ifdef SUPPORT_IPV6
  if (!inet_aton_v6(ip.c_str(), &l_saddr)) {
    throw string ("Invalid IPv6 address.");
  }
#else
  if (!inet_aton(ip.c_str(), (in_addr*)&l_saddr.sin_addr.s_addr)) {
    throw string ("Invalid IPv4 address.");
  }
#endif
}

int AmRtpStream::next_port = -1;
AmMutex AmRtpStream::port_mut;

int AmRtpStream::getNextPort()
{
    
  int port=0;

  port_mut.lock();
  if(next_port < 0){
    next_port = AmConfig::RtpLowPort;
  }
    
  port = next_port & 0xfffe;
  next_port += 2;

  if(next_port >= AmConfig::RtpHighPort){
    next_port = AmConfig::RtpLowPort;
  }
  port_mut.unlock();
    
  return port;
}

void AmRtpStream::setLocalPort()
{
  if(l_port)
    return;
  
  int sd=0;
#ifdef SUPPORT_IPV6
  if((sd = socket(l_saddr.ss_family,SOCK_DGRAM,0)) == -1)
#else
    if((sd = socket(PF_INET,SOCK_DGRAM,0)) == -1)
#endif
      {
	ERROR("%s\n",strerror(errno));
	throw string ("while creating new socket.");
      } 
  int true_opt = 1;
  if(ioctl(sd, FIONBIO , &true_opt) == -1){
    ERROR("%s\n",strerror(errno));
    close(sd);
    throw string ("while setting socket non blocking.");
  }
  
  int retry = 10;
  unsigned short port = 0;
  for(;retry; --retry){

    port = getNextPort();
#ifdef SUPPORT_IPV6
    set_port_v6(&l_saddr,port);
    if(!bind(sd,(const struct sockaddr*)&l_saddr,
	     sizeof(struct sockaddr_storage)))
#else	
      l_saddr.sin_port = htons(port);
    if(!bind(sd,(const struct sockaddr*)&l_saddr,
	     sizeof(struct sockaddr_in)))
#endif
      {
	break;
      }
    else {
      DBG("bind: %s\n",strerror(errno));		
    }
      
  }

  if (!retry){
    ERROR("could not find a free RTP port\n");
    close(sd);
    throw string("could not find a free RTP port");
  }

  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&true_opt, sizeof (true_opt)) == -1) {

    ERROR("%s\n",strerror(errno));
    close(sd);
    throw string ("while setting local address reusable.");
  }
    
  l_sd = sd;
  l_port = port;
  AmRtpReceiver::instance()->addStream(l_sd,this);
  DBG("local rtp port set to %i\n",l_port);
}

int AmRtpStream::ping()
{
  unsigned char ping_chr[2];

  ping_chr[0] = 0;
  ping_chr[1] = 0;

  AmRtpPacket rp;
  rp.payload = payload;
  rp.marker = true;
  rp.sequence = sequence++;
  rp.timestamp = 0;   
  rp.ssrc = l_ssrc;
  rp.compile((unsigned char*)ping_chr,2);

  rp.setAddr(&r_saddr);
  if(rp.send(l_sd) < 0){
    ERROR("while sending RTP packet.\n");
    return -1;
  }
 
  return 2;
}

int AmRtpStream::send( unsigned int ts, unsigned char* buffer, unsigned int size )
{
  if ((mute) || (hold))
    return 0;

  if(!size)
    return -1;

  AmRtpPacket rp;
  rp.payload = payload;
  rp.marker = false;
  rp.sequence = sequence++;
  rp.timestamp = ts;   
  rp.ssrc = l_ssrc;
  rp.compile((unsigned char*)buffer,size);

  rp.setAddr(&r_saddr);

#ifdef WITH_ZRTP
  if (session->zrtp_audio) {
    zrtp_status_t status = zrtp_status_fail;
    unsigned int size = rp.getBufferSize();
    status = zrtp_process_rtp(session->zrtp_audio, (char*)rp.getBuffer(), &size);
    switch (status) {
    case zrtp_status_drop: {
      DBG("ZRTP says: drop packet! %u - %u\n", size, rp.getBufferSize());
      return 0;
    } 
    case zrtp_status_ok: {
      //      DBG("ZRTP says: ok!\n");
      if (rp.getBufferSize() != size)
//       DBG("SEND packet size before: %d, after %d\n", 
// 	   rp.getBufferSize(), size);
      rp.setBufferSize(size);
    } break;
    default:
    case zrtp_status_fail: {
      DBG("ZRTP says: fail!\n");
      //      DBG("(f)");
      return 0;
    }

    }
    
  }
#endif

  if(rp.send(l_sd) < 0){
    ERROR("while sending RTP packet.\n");
    return -1;
  }
 
  return size;
}

int AmRtpStream::send_raw( char* packet, unsigned int length )
{
  if ((mute) || (hold))
    return 0;

  AmRtpPacket rp;
  rp.compile_raw((unsigned char*)packet, length);
  rp.setAddr(&r_saddr);

  if(rp.send(l_sd) < 0){
    ERROR("while sending raw RTP packet.\n");
    return -1;
  }
 
  return length;
}


// returns 
// @param ts              [out] timestamp of the received packet, 
//                              in audio buffer relative time
// @param audio_buffer_ts [in]  current ts at the audio_buffer 

int AmRtpStream::receive( unsigned char* buffer, unsigned int size,
			  unsigned int& ts, int &out_payload)
{
  AmRtpPacket* rp = NULL;
  int err = nextPacket(rp);
    
  if(err <= 0)
    return err;

  if (!rp)
    return 0;

#ifndef SUPPORT_IPV6
  if(passive) {
    // #ifdef SUPPORT_IPV6
    //     struct sockaddr_storage recv_addr;
    // #else
    struct sockaddr_in recv_addr;
    rp->getAddr(&recv_addr);

    // symmetric RTP
    if ((recv_addr.sin_port != r_saddr.sin_port)
	|| (recv_addr.sin_addr.s_addr 
	    != r_saddr.sin_addr.s_addr)) {
		
      string addr_str = get_addr_str(recv_addr.sin_addr);
      int port = ntohs(recv_addr.sin_port);
      setRAddr(addr_str,port);
      DBG("Symmetric RTP: setting new remote address: %s:%i\n",
	  addr_str.c_str(),port);
      // avoid comparing each time sender address
    } else {
      DBG("Symmetric RTP: remote end sends RTP from advertised address. Leaving passive mode.\n");
    }
    passive = false;
  }
#endif

  /* do we have a new talk spurt? */
  begin_talk = ((last_payload == 13) || rp->marker);
  last_payload = rp->payload;

  if(!rp->getDataSize()) {
    mem.freePacket(rp);
    return RTP_EMPTY;
  }

  if (telephone_event_pt.get() && rp->payload == telephone_event_pt->payload_type)
    {
      dtmf_payload_t* dpl = (dtmf_payload_t*)rp->getData();

      DBG("DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u\n",
	  dpl->event,dpl->e,dpl->r,dpl->volume,ntohs(dpl->duration),rp->timestamp);
      session->postDtmfEvent(new AmRtpDtmfEvent(dpl, getTelephoneEventRate(), rp->timestamp));
      mem.freePacket(rp);
      return RTP_DTMF;
    }

  assert(rp->getData());
  if(rp->getDataSize() > size){
    ERROR("received too big RTP packet\n");
    mem.freePacket(rp);
    return RTP_BUFFER_SIZE;
  }

  memcpy(buffer,rp->getData(),rp->getDataSize());
  ts = rp->timestamp;
  out_payload = rp->payload;

  int res = rp->getDataSize();
  mem.freePacket(rp);
  return res;
}

AmRtpStream::AmRtpStream(AmSession* _s) 
  : r_port(0),
    l_port(0),
    l_sd(0), 
    r_ssrc_i(false),
    session(_s),
    passive(false),
    telephone_event_pt(NULL),
    mute(false),
    hold(false),
    receiving(true),
    monitor_rtp_timeout(true)
{

#ifdef SUPPORT_IPV6
  memset(&r_saddr,0,sizeof(struct sockaddr_storage));
  memset(&l_saddr,0,sizeof(struct sockaddr_storage));
#else
  memset(&r_saddr,0,sizeof(struct sockaddr_in));
  memset(&l_saddr,0,sizeof(struct sockaddr_in));
#endif
  //sched = AmRtpScheduler::instance();

#ifndef SUPPORT_IPV6
  /* By default we listen on all interfaces */
  l_saddr.sin_family = AF_INET;
  l_saddr.sin_addr.s_addr = INADDR_ANY;
#endif

  l_ssrc = get_random();
  sequence = get_random();
  gettimeofday(&last_recv_time,NULL);
}

AmRtpStream::~AmRtpStream()
{
  if(l_sd){
    if (AmRtpReceiver::haveInstance())
	AmRtpReceiver::instance()->removeStream(l_sd);
    close(l_sd);
  }
}

int AmRtpStream::getLocalPort()
{
  if (hold)
    return 0;

  if(!l_port)
    setLocalPort();

  return l_port;
}

int AmRtpStream::getRPort()
{
  return r_port;
}

string AmRtpStream::getRHost()
{
  return r_host;
}

void AmRtpStream::setRAddr(const string& addr, unsigned short port)
{
  DBG("RTP remote address set to %s:%u\n",addr.c_str(),port);

#ifdef SUPPORT_IPV6
  struct sockaddr_storage ss;
  memset (&ss, 0, sizeof (ss));
  if(!inet_aton_v6(addr.c_str(),&ss)){
    ERROR("address not valid (host: %s)\n",addr.c_str());
    throw string("invalid address");
  }
    
  memcpy(&r_saddr,&ss,sizeof(struct sockaddr_storage));
  set_port_v6(&r_saddr,port);

#else
  struct sockaddr_in sa;
  memset (&sa, 0, sizeof (sa));

#ifdef BSD44SOCKETS
  sa.sin_len = sizeof(sockaddr_in);
#endif
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
    
  /* inet_aton only supports dot-notation IP address strings... but an RFC
   * 4566 unicast-address, as found in c=, can be an FQDN (or other!).
   * We need to do more sophisticated parsing -- hence p_s_i_f_n().
   */
  if (!populate_sockaddr_in_from_name(addr, &sa)) {
    WARN("Address not valid (host: %s).\n", addr.c_str());
    throw string("invalid address");
  }

  memcpy(&r_saddr,&sa,sizeof(struct sockaddr_in));
#endif

  r_host = addr;
  r_port = port;

#ifndef SUPPORT_IPV6
  mute = (sa.sin_addr.s_addr == 0);
#endif
}

void AmRtpStream::setPassiveMode(bool p)
{
  passive = p;
  DBG("The other UA is NATed: switched to passive mode.\n");
}

int AmRtpStream::init(AmPayloadProviderInterface* payload_provider,
		      const SdpMedia& remote_media, 
		      const SdpConnection& conn, 
		      bool remote_active)
{
  if (remote_media.payloads.empty()) {
    ERROR("can not initialize RTP stream without payloads.\n");
    return -1;
  }

  //TODO: support mute & on_hold

  setLocalIP(AmConfig::LocalIP);
  setPassiveMode(remote_active);
  setRAddr(conn.address, remote_media.port);

  // TODO: take the first *supported* payload
  //       (align with the SDP answer algo)
  payload = remote_media.payloads[0].payload_type;
  last_payload = payload;

  resume();

#ifdef WITH_ZRTP  
  if( session->zrtp_audio  ) {
    DBG("now starting zrtp stream...\n");
    zrtp_start_stream( session->zrtp_audio );
  }
#endif

  return 0;
}

void AmRtpStream::pause()
{
}

void AmRtpStream::resume()
{
  gettimeofday(&last_recv_time,NULL);
  mem.clear();
}

void AmRtpStream::setOnHold(bool on_hold) {
  mute = hold = on_hold;
}

bool AmRtpStream::getOnHold() {
  return hold;
}

AmRtpPacket* AmRtpStream::newPacket() {
  return mem.newPacket();
}

void AmRtpStream::freePacket(AmRtpPacket* p) {
  return mem.freePacket(p);
}

void AmRtpStream::bufferPacket(AmRtpPacket* p)
{
  memcpy(&last_recv_time, &p->recv_time, sizeof(struct timeval));

  if (!receiving && !passive) {
    mem.freePacket(p);
    return;
  }

  receive_mut.lock();
  // free packet on double packet for TS received
  if(!(telephone_event_pt.get() && 
       (p->payload == telephone_event_pt->payload_type))) {
    if (receive_buf.find(p->timestamp) != receive_buf.end()) {
      mem.freePacket(receive_buf[p->timestamp]);
    }
  }  

#ifdef WITH_ZRTP
  if (session->zrtp_audio) {

    zrtp_status_t status = zrtp_status_fail;
    unsigned int size = p->getBufferSize();
    
    status = zrtp_process_srtp(session->zrtp_audio, (char*)p->getBuffer(), &size);
    switch (status)
      {
      case zrtp_status_forward:
      case zrtp_status_ok: {
	p->setBufferSize(size);
	if (p->parse() < 0) {
	  ERROR("parsing decoded packet!\n");
	  mem.freePacket(p);
	} else {
          if(telephone_event_pt.get() && 
	     (p->payload == telephone_event_pt->payload_type)) {
            rtp_ev_qu.push(p);
          } else {
	    receive_buf[p->timestamp] = p;
          }
	}
      }	break;

      case zrtp_status_drop: {
	//
	// This is a protocol ZRTP packet or masked RTP media.
	// In either case the packet must be dropped to protect your 
	// media codec
	mem.freePacket(p);
	
      } break;

      case zrtp_status_fail:
      default: {
	ERROR("zrtp_status_fail!\n");
        //
        // This is some kind of error - see logs for more information
        //
	mem.freePacket(p);
      } break;
      }
  } else {
#endif // WITH_ZRTP

    if(telephone_event_pt.get() && 
       (p->payload == telephone_event_pt->payload_type)) {
      rtp_ev_qu.push(p);
    } else {
      receive_buf[p->timestamp] = p;
    }

#ifdef WITH_ZRTP
  }
#endif
  receive_mut.unlock();
}

void AmRtpStream::clearRTPTimeout(struct timeval* recv_time) {
 memcpy(&last_recv_time, recv_time, sizeof(struct timeval));
}
 
int AmRtpStream::nextPacket(AmRtpPacket*& p)
{
  if (!receiving && !passive)
    return RTP_EMPTY;

  struct timeval now;
  struct timeval diff;
  gettimeofday(&now,NULL);

  receive_mut.lock();
  timersub(&now,&last_recv_time,&diff);
  if(monitor_rtp_timeout &&
     AmConfig::DeadRtpTime && 
     (diff.tv_sec > 0) &&
     ((unsigned int)diff.tv_sec > AmConfig::DeadRtpTime)){
    WARN("RTP Timeout detected. Last received packet is too old.\n");
    DBG("diff.tv_sec = %i\n",(unsigned int)diff.tv_sec);
    receive_mut.unlock();
    return RTP_TIMEOUT;
  }

  if(!rtp_ev_qu.empty()) {
    // first return RTP telephone event payloads
    p = rtp_ev_qu.front();
    rtp_ev_qu.pop();
    receive_mut.unlock();
    return 1;
  }

  if(receive_buf.empty()){
    receive_mut.unlock();
    return RTP_EMPTY;
  }

  p = receive_buf.begin()->second;
  receive_buf.erase(receive_buf.begin());
  receive_mut.unlock();

  return 1;
}

int AmRtpStream::getTelephoneEventRate()
{
  int retval = 0;
  if (telephone_event_pt.get())
    retval = telephone_event_pt->clock_rate;
  return retval;
}

PacketMem::PacketMem() {
  memset(used, 0, sizeof(used));
}

inline AmRtpPacket* PacketMem::newPacket() {
  for (int i=0;i<MAX_PACKETS;i++)
    if (!used[i]) {
      used[i]=true;
      return &packets[i];
    }
  
  return NULL;
}

inline void PacketMem::freePacket(AmRtpPacket* p) {
  if (!p)  return;

  used[p-packets] = false;
}

inline void PacketMem::clear() {
  memset(used, 0, sizeof(used));
}
 
