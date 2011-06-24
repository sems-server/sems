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

#include "sip/resolver.h"

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

int AmRtpStream::hasLocalSocket() {
  return l_sd;
}

int AmRtpStream::getLocalSocket()
{
  if (l_sd)
    return l_sd;

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

  l_sd = sd;
  return l_sd;
}

void AmRtpStream::setLocalPort()
{
  if(l_port)
    return;
  
  assert(l_if >= 0);
  assert(l_if < (int)AmConfig::Ifs.size());

  if (!getLocalSocket())
    return;
  
  int retry = 10;
  unsigned short port = 0;
  for(;retry; --retry){

    port = AmConfig::Ifs[l_if].getNextRtpPort();
#ifdef SUPPORT_IPV6
    set_port_v6(&l_saddr,port);
    if(!bind(l_sd,(const struct sockaddr*)&l_saddr,
	     sizeof(struct sockaddr_storage)))
#else	
    l_saddr.sin_port = htons(port);
    if(!bind(l_sd,(const struct sockaddr*)&l_saddr,
	     sizeof(struct sockaddr_in)))
#endif
      {
	break;
      }
    else {
      DBG("bind: %s\n",strerror(errno));		
    }
  }

  int true_opt = 1;
  if (!retry){
    ERROR("could not find a free RTP port\n");
    close(l_sd);
    l_sd = 0;
    throw string("could not find a free RTP port");
  }

  if(setsockopt(l_sd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&true_opt, sizeof (true_opt)) == -1) {

    ERROR("%s\n",strerror(errno));
    close(l_sd);
    l_sd = 0;
    throw string ("while setting local address reusable.");
  }

  l_port = port;
  AmRtpReceiver::instance()->addStream(l_sd,this);
  DBG("added to RTP receiver (%s:%i)\n",get_addr_str(l_saddr.sin_addr).c_str(),l_port);
}

int AmRtpStream::ping()
{
  // TODO:
  //  - we'd better send an empty UDP packet 
  //    for this purpose.

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

void AmRtpStream::sendDtmfPacket(unsigned int ts) {
  while (true) {
    switch(dtmf_sending_state) {
    case DTMF_SEND_NONE: {
      dtmf_send_queue_mut.lock();
      if (dtmf_send_queue.empty()) {
	dtmf_send_queue_mut.unlock();
	return;
      }
      current_send_dtmf = dtmf_send_queue.front();
      current_send_dtmf_ts = ts;
      dtmf_send_queue.pop();
      dtmf_send_queue_mut.unlock();
      dtmf_sending_state = DTMF_SEND_SENDING;
      current_send_dtmf_ts = ts;
      DBG("starting to send DTMF\n");
    } break;
      
    case DTMF_SEND_SENDING: {
      if (ts_less()(ts, current_send_dtmf_ts + current_send_dtmf.second)) {
	// send packet
	if (!remote_telephone_event_pt.get())
	  return;

	dtmf_payload_t dtmf;
	dtmf.event = current_send_dtmf.first;
	dtmf.e = dtmf.r = 0;
	dtmf.duration = htons(ts - current_send_dtmf_ts);
	dtmf.volume = 20;

	DBG("sending DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u\n",
	    dtmf.event,dtmf.e,dtmf.r,dtmf.volume,ntohs(dtmf.duration),current_send_dtmf_ts);

	compile_and_send(remote_telephone_event_pt->payload_type, dtmf.duration == 0, 
			 current_send_dtmf_ts, 
			 (unsigned char*)&dtmf, sizeof(dtmf_payload_t)); 
	return;

      } else {
	DBG("ending DTMF\n");
	dtmf_sending_state = DTMF_SEND_ENDING;
	send_dtmf_end_repeat = 0;
      }
    } break;
      
    case DTMF_SEND_ENDING:  {
      if (send_dtmf_end_repeat >= 3) {
	DBG("DTMF send complete\n");
	dtmf_sending_state = DTMF_SEND_NONE;
      } else {
	send_dtmf_end_repeat++;
	// send packet with end bit set, duration = event duration
	if (!remote_telephone_event_pt.get())
	  return;

	dtmf_payload_t dtmf;
	dtmf.event = current_send_dtmf.first;
	dtmf.e = 1; 
	dtmf.r = 0;
	dtmf.duration = htons(current_send_dtmf.second);
	dtmf.volume = 20;

	DBG("sending DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u\n",
	    dtmf.event,dtmf.e,dtmf.r,dtmf.volume,ntohs(dtmf.duration),current_send_dtmf_ts);

	compile_and_send(remote_telephone_event_pt->payload_type, false, 
			 current_send_dtmf_ts, 
			 (unsigned char*)&dtmf, sizeof(dtmf_payload_t)); 
	return;
      }
    } break;
    };
  }

}

int AmRtpStream::compile_and_send(const int payload, bool marker, unsigned int ts, 
				  unsigned char* buffer, unsigned int size) {
  AmRtpPacket rp;
  rp.payload = payload;
  rp.timestamp = ts;
  rp.marker = marker;
  rp.sequence = sequence++;
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

int AmRtpStream::send( unsigned int ts, unsigned char* buffer, unsigned int size )
{
  if ((mute) || (hold))
    return 0;

  sendDtmfPacket(ts);

  if(!size)
    return -1;

  PayloadMappingTable::iterator it = pl_map.find(payload);
  if ((it == pl_map.end()) || (it->second.remote_pt < 0)) {
    ERROR("sending packet with unsupported remote payload type\n");
    return -1;
  }
  
  return compile_and_send(it->second.remote_pt, false, ts, buffer, size);
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

  handleSymmetricRtp(rp);

  /* do we have a new talk spurt? */
  begin_talk = ((last_payload == 13) || rp->marker);
  last_payload = rp->payload;

  if(!rp->getDataSize()) {
    mem.freePacket(rp);
    return RTP_EMPTY;
  }

  if (local_telephone_event_pt.get() &&
      rp->payload == local_telephone_event_pt->payload_type)
    {
      dtmf_payload_t* dpl = (dtmf_payload_t*)rp->getData();

      DBG("DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u\n",
	  dpl->event,dpl->e,dpl->r,dpl->volume,ntohs(dpl->duration),rp->timestamp);
      session->postDtmfEvent(new AmRtpDtmfEvent(dpl, getLocalTelephoneEventRate(), rp->timestamp));
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

AmRtpStream::AmRtpStream(AmSession* _s, int _if) 
  : r_port(0),
    l_if(_if),
    l_port(0),
    l_sd(0), 
    r_ssrc_i(false),
    session(_s),
    passive(false),
    mute(false),
    hold(false),
    receiving(true),
    monitor_rtp_timeout(true),
    dtmf_sending_state(DTMF_SEND_NONE),
    relay_stream(NULL),
    relay_enabled(false)
{

#ifdef SUPPORT_IPV6
  memset(&r_saddr,0,sizeof(struct sockaddr_storage));
  memset(&l_saddr,0,sizeof(struct sockaddr_storage));
#else
  memset(&r_saddr,0,sizeof(struct sockaddr_in));
  memset(&l_saddr,0,sizeof(struct sockaddr_in));
#endif

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

  struct sockaddr_storage ss;
  memset (&ss, 0, sizeof (ss));

  /* inet_aton only supports dot-notation IP address strings... but an RFC
   * 4566 unicast-address, as found in c=, can be an FQDN (or other!).
   */
  dns_handle dh;
  if (resolver::instance()->resolve_name(addr.c_str(),&dh,&ss,IPv4) < 0) {
    WARN("Address not valid (host: %s).\n", addr.c_str());
    throw string("invalid address");
  }

#ifdef SUPPORT_IPV6

  memcpy(&r_saddr,&ss,sizeof(struct sockaddr_storage));
  set_port_v6(&r_saddr,port);

#else

  struct sockaddr_in sa;
  memset (&sa, 0, sizeof(sa));

#ifdef BSD44SOCKETS
  sa.sin_len = sizeof(sockaddr_in);
#endif
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);

  memcpy(&r_saddr,&sa,sizeof(struct sockaddr_in));
  memcpy(&r_saddr.sin_addr,&((sockaddr_in*)&ss)->sin_addr,sizeof(in_addr));
#endif

  r_host = addr;
  r_port = port;

#ifndef SUPPORT_IPV6
  mute = (r_saddr.sin_addr.s_addr == 0);
#endif
}

void AmRtpStream::handleSymmetricRtp(AmRtpPacket* rp) {
#ifndef SUPPORT_IPV6
  if(passive) {
    // #ifdef SUPPORT_IPV6
    //     struct sockaddr_storage recv_addr;
    // #else
    struct sockaddr_in recv_addr;
    rp->getAddr(&recv_addr);

    // symmetric RTP
    if ((recv_addr.sin_port != r_saddr.sin_port)
	|| (recv_addr.sin_addr.s_addr != r_saddr.sin_addr.s_addr)) {
      string addr_str = get_addr_str(recv_addr.sin_addr);
      int port = ntohs(recv_addr.sin_port);
      setRAddr(addr_str,port);
      DBG("Symmetric RTP: setting new remote address: %s:%i\n",
	  addr_str.c_str(),port);
    } else {
      DBG("Symmetric RTP: remote end sends RTP from advertised address."
	  " Leaving passive mode.\n");
    }
    // avoid comparing each time sender address
    passive = false;
  }
#endif
}

void AmRtpStream::setPassiveMode(bool p)
{
  passive = p;
  DBG("The other UA is NATed: switched to passive mode.\n");
}

int AmRtpStream::init(AmPayloadProviderInterface* payload_provider,
		      unsigned char media_i, 
		      const AmSdp& local,
		      const AmSdp& remote)
{
  if((media_i >= local.media.size()) ||
     (media_i >= remote.media.size()) ) {

    ERROR("Media index %i is invalid, either within local or remote SDP (or both)",media_i);
    return -1;
  }

  const SdpMedia& local_media = local.media[media_i];
  const SdpMedia& remote_media = remote.media[media_i];

  payloads.clear();
  pl_map.clear();
  payloads.resize(local_media.payloads.size());

  int i=0;
  vector<SdpPayload>::const_iterator sdp_it = local_media.payloads.begin();
  vector<Payload>::iterator p_it = payloads.begin();

  // first pass on local SDP
  while(p_it != payloads.end()) {

    amci_payload_t* a_pl = payload_provider->payload(sdp_it->payload_type);
    if(a_pl == NULL){
      ERROR("No internal payload corresponding to type %i\n",
	    sdp_it->payload_type);
      return -1;//TODO
    };
    
    p_it->pt         = sdp_it->payload_type;
    p_it->name       = a_pl->name;
    p_it->codec_id   = a_pl->codec_id;
    p_it->clock_rate = a_pl->sample_rate;

    pl_map[sdp_it->payload_type].index     = i;
    pl_map[sdp_it->payload_type].remote_pt = -1;
    
    ++p_it;
    ++sdp_it;
    ++i;
  }

  // second pass on remote SDP
  sdp_it = remote_media.payloads.begin();
  while(sdp_it != remote_media.payloads.end()) {

    PayloadMappingTable::iterator pmt_it = pl_map.end();
    if(sdp_it->encoding_name.empty()){ // must be a static payload

      pmt_it = pl_map.find(sdp_it->payload_type);
    }
    else {
      for(p_it = payloads.begin(); p_it != payloads.end(); ++p_it){

	if(!strcasecmp(p_it->name.c_str(),sdp_it->encoding_name.c_str()) && 
	   (p_it->clock_rate == (unsigned int)sdp_it->clock_rate)){
	  pmt_it = pl_map.find(p_it->pt);
	  break;
	}
      }
    }

    if(pmt_it != pl_map.end()){
      pmt_it->second.remote_pt = sdp_it->payload_type;
    }
    ++sdp_it;
  }

  //TODO: support mute, on_hold & sendrecv/sendonly/recvonly/inactive
  //      support for passive-mode

  setLocalIP(local.conn.address);
  //setPassiveMode(remote_active);
  setRAddr(remote.conn.address, remote_media.port);

  // TODO: take the first *supported* payload
  //       (align with the SDP answer algo)
  if(local_media.payloads.empty()) {

    ERROR("local_media.payloads.empty()\n");
    return -1;
  }

  payload = local_media.payloads[0].payload_type;
  last_payload = payload;

  remote_telephone_event_pt.reset(remote.telephoneEventPayload());
  if (remote_telephone_event_pt.get()) {
      DBG("remote party supports telephone events (pt=%i)\n",
	  remote_telephone_event_pt->payload_type);
  } else {
    DBG("remote party doesn't support telephone events\n");
  }

  local_telephone_event_pt.reset(local.telephoneEventPayload());

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
  receiving = false;
}

void AmRtpStream::resume()
{
  gettimeofday(&last_recv_time,NULL);
  mem.clear();
  receiving = true;
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

  if (relay_enabled) {
    handleSymmetricRtp(p);

    if (NULL != relay_stream) {
      relay_stream->relay(p);
    }
    mem.freePacket(p);
    return;
  }

  receive_mut.lock();
  // free packet on double packet for TS received
  if(!(local_telephone_event_pt.get() &&
       (p->payload == local_telephone_event_pt->payload_type))) {
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
          if(local_telephone_event_pt.get() && 
	     (p->payload == local_telephone_event_pt->payload_type)) {
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

    if(local_telephone_event_pt.get() &&
       (p->payload == local_telephone_event_pt->payload_type)) {
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

unsigned int AmRtpStream::bytes2samples(unsigned int) const {
  ERROR("bytes2samples called on AmRtpStream\n");
  return 0;
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
    WARN("RTP Timeout detected. Last received packet is too old "
	 "(diff.tv_sec = %i\n",(unsigned int)diff.tv_sec);
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

#include "rtp/rtp.h"

void AmRtpStream::relay(AmRtpPacket* p) {
  if (!l_port) // not yet initialized
    return;

  rtp_hdr_t* hdr = (rtp_hdr_t*)p->getBuffer();
  hdr->seq = htons(sequence++);
  hdr->ssrc = htonl(l_ssrc);
  p->setAddr(&r_saddr);

  if(p->send(l_sd) < 0){
    ERROR("while sending RTP packet.\n");
  }
}

int AmRtpStream::getLocalTelephoneEventRate()
{
  if (local_telephone_event_pt.get())
    return local_telephone_event_pt->clock_rate;
  return 0;
}

void AmRtpStream::sendDtmf(int event, unsigned int duration_ms) {
  dtmf_send_queue_mut.lock();
  dtmf_send_queue.push(std::make_pair(event, duration_ms * getLocalTelephoneEventRate() 
				      / 1000));
  dtmf_send_queue_mut.unlock();
  DBG("enqueued DTMF event %i duration %u\n", event, duration_ms);
}

void AmRtpStream::setRelayStream(AmRtpStream* stream) {
  relay_stream = stream;
  DBG("set relay stream [%p] for RTP instance [%p]\n",
      stream, this);
}

void AmRtpStream::enableRtpRelay() {
  relay_enabled = true;
}

void AmRtpStream::disableRtpRelay() {
  relay_enabled = false;
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
 
