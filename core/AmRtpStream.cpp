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
#include "amci/codecs.h"
#include "AmJitterBuffer.h"

#include "sip/resolver.h"
#include "sip/ip_util.h"
#include "sip/raw_sender.h"
#include "sip/msg_logger.h"

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
#include "libzrtp/zrtp.h"
#endif

#include "rtp/rtp.h"

#include <set>
using std::set;

void PayloadMask::clear()
{
  memset(bits, 0, sizeof(bits));
}

void PayloadMask::set_all()
{
  memset(bits, 0xFF, sizeof(bits));
}

void PayloadMask::invert()
{
  // assumes that bits[] contains 128 bits
  unsigned long long* ull = (unsigned long long*)bits;
  ull[0] = ~ull[0];
  ull[1] = ~ull[1];
}

PayloadMask::PayloadMask(const PayloadMask &src)
{
  memcpy(bits, src.bits, sizeof(bits));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * This function must be called before setLocalPort, because
 * setLocalPort will bind the socket and it will be not
 * possible to change the IP later
 */
void AmRtpStream::setLocalIP(const string& ip)
{
  if (!am_inet_pton(ip.c_str(), &l_saddr)) {
    throw string ("AmRtpStream::setLocalIP: Invalid IP address: ") + ip;
  }
  DBG("ip = %s\n",ip.c_str());
}

int AmRtpStream::hasLocalSocket() {
  return l_sd;
}

int AmRtpStream::getLocalSocket()
{
  if (l_sd)
    return l_sd;

  int sd=0, rtcp_sd=0;
  if((sd = socket(l_saddr.ss_family,SOCK_DGRAM,0)) == -1) {
    ERROR("%s\n",strerror(errno));
    throw string ("while creating new socket.");
  } 

  if((rtcp_sd = socket(l_saddr.ss_family,SOCK_DGRAM,0)) == -1) {
    ERROR("%s\n",strerror(errno));
    throw string ("while creating new socket.");
  } 

  int true_opt = 1;
  if(ioctl(sd, FIONBIO , &true_opt) == -1){
    ERROR("%s\n",strerror(errno));
    close(sd);
    throw string ("while setting socket non blocking.");
  }

  if(ioctl(rtcp_sd, FIONBIO , &true_opt) == -1){
    ERROR("%s\n",strerror(errno));
    close(sd);
    throw string ("while setting socket non blocking.");
  }

  l_sd = sd;
  l_rtcp_sd = rtcp_sd;

  return l_sd;
}

void AmRtpStream::setLocalPort(unsigned short p)
{
  if(l_port)
    return;
  
  if(l_if < 0) {
    if (session) l_if = session->getRtpInterface();
    else {
      ERROR("BUG: no session when initializing RTP stream, invalid interface can be used\n");
      l_if = 0;
    }
  }
  
  int retry = 10;
  unsigned short port = 0;
  for(;retry; --retry){

    if (!getLocalSocket())
      return;

    if(!p)
      port = AmConfig::RTP_Ifs[l_if].getNextRtpPort();
    else
      port = p;

    am_set_port(&l_saddr,port+1);
    if(bind(l_rtcp_sd,(const struct sockaddr*)&l_saddr,SA_len(&l_saddr))) {
      DBG("bind: %s\n",strerror(errno));		
      goto try_another_port;
    }

    am_set_port(&l_saddr,port);
    if(bind(l_sd,(const struct sockaddr*)&l_saddr,SA_len(&l_saddr))) {
      DBG("bind: %s\n",strerror(errno));		
      goto try_another_port;
    }

    // both bind() succeeded!
    break;

  try_another_port:
      close(l_sd);
      l_sd = 0;
      close(l_rtcp_sd);
      l_rtcp_sd = 0;
  }

  int true_opt = 1;
  if (!retry){
    ERROR("could not find a free RTP port\n");
    throw string("could not find a free RTP port");
  }

  // rco: does that make sense after bind() ????
  if(setsockopt(l_sd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&true_opt, sizeof (true_opt)) == -1) {

    ERROR("%s\n",strerror(errno));
    close(l_sd);
    l_sd = 0;
    throw string ("while setting local address reusable.");
  }

  l_port = port;
  l_rtcp_port = port+1;

  if(!p) {
    AmRtpReceiver::instance()->addStream(l_sd, this);
    AmRtpReceiver::instance()->addStream(l_rtcp_sd, this);
    DBG("added stream [%p] to RTP receiver (%s:%i/%i)\n", this,
	get_addr_str((sockaddr_storage*)&l_saddr).c_str(),l_port,l_rtcp_port);
  }

  memcpy(&l_rtcp_saddr, &l_saddr, sizeof(l_saddr));
  am_set_port(&l_rtcp_saddr, l_rtcp_port);
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
  if(rp.send(l_sd, AmConfig::RTP_Ifs[l_if].NetIfIdx,&l_saddr) < 0){
    ERROR("while sending RTP packet.\n");
    return -1;
  }
 
  return 2;
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
  if (session && session->enable_zrtp){
    if (NULL == session->zrtp_session_state.zrtp_audio) {
      ERROR("ZRTP enabled on session, but no audio stream created\n");
      return -1;
    }

    unsigned int size = rp.getBufferSize();
    zrtp_status_t status = zrtp_process_rtp(session->zrtp_session_state.zrtp_audio,
					    (char*)rp.getBuffer(), &size);
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

  if(rp.send(l_sd, AmConfig::RTP_Ifs[l_if].NetIfIdx, &l_saddr) < 0){
    ERROR("while sending RTP packet.\n");
    return -1;
  }
 
  if (logger) rp.logSent(logger, &l_saddr);
 
  return size;
}

int AmRtpStream::send( unsigned int ts, unsigned char* buffer, unsigned int size )
{
  if ((mute) || (hold))
    return 0;

  if(remote_telephone_event_pt.get())
    dtmf_sender.sendPacket(ts,remote_telephone_event_pt->payload_type,this);

  if(!size)
    return -1;

  PayloadMappingTable::iterator it = pl_map.find(payload);
  if ((it == pl_map.end()) || (it->second.remote_pt < 0)) {
    ERROR("sending packet with unsupported remote payload type %d\n", payload);
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

  if(rp.send(l_sd, AmConfig::RTP_Ifs[l_if].NetIfIdx, &l_saddr) < 0){
    ERROR("while sending raw RTP packet.\n");
    return -1;
  }

  if (logger) rp.logSent(logger, &l_saddr);

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

  handleSymmetricRtp(&rp->addr,false);

  /* do we have a new talk spurt? */
  begin_talk = ((last_payload == 13) || rp->marker);
  last_payload = rp->payload;

  if(!rp->getDataSize()) {
    mem.freePacket(rp);
    return RTP_EMPTY;
  }

  if (rp->payload == getLocalTelephoneEventPT())
    {
      recvDtmfPacket(rp);
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
    logger(NULL),
    passive(false),
    passive_rtcp(false),
    offer_answer_used(true),
    active(false), // do not return any data unless something really received
    mute(false),
    hold(false),
    receiving(true),
    monitor_rtp_timeout(true),
    relay_stream(NULL),
    relay_enabled(false),
    relay_raw(false),
    sdp_media_index(-1),
    relay_transparent_ssrc(true),
    relay_transparent_seqno(true),
    relay_filter_dtmf(false),
    force_receive_dtmf(false)
{

  memset(&r_saddr,0,sizeof(struct sockaddr_storage));
  memset(&l_saddr,0,sizeof(struct sockaddr_storage));

  l_ssrc = get_random();
  sequence = get_random();
  clearRTPTimeout();

  // by default the system codecs
  payload_provider = AmPlugIn::instance();
}

AmRtpStream::~AmRtpStream()
{
  if(l_sd){
    if (AmRtpReceiver::haveInstance()){
      AmRtpReceiver::instance()->removeStream(l_sd);
      AmRtpReceiver::instance()->removeStream(l_rtcp_sd);
    }
    close(l_sd);
    close(l_rtcp_sd);
  }
  if (logger) dec_ref(logger);
}

int AmRtpStream::getLocalPort()
{
  //  if (hold)
  //    return 0;

  if(!l_port)
    setLocalPort();

  return l_port;
}

int AmRtpStream::getLocalRtcpPort()
{
  if(!l_rtcp_port)
    setLocalPort();

  return l_rtcp_port;
}

int AmRtpStream::getRPort()
{
  return r_port;
}

string AmRtpStream::getRHost()
{
  return r_host;
}

void AmRtpStream::setRAddr(const string& addr, unsigned short port,
			   unsigned short rtcp_port)
{
  DBG("RTP remote address set to %s:(%u/%u)\n",
      addr.c_str(),port,rtcp_port);

  struct sockaddr_storage ss;
  memset (&ss, 0, sizeof (ss));

  /* inet_aton only supports dot-notation IP address strings... but an RFC
   * 4566 unicast-address, as found in c=, can be an FQDN (or other!).
   */
  dns_handle dh;
  if (resolver::instance()->resolve_name(addr.c_str(),&dh,&ss,IPv4) < 0) {
    WARN("Address not valid (host: %s).\n", addr.c_str());
    throw string("invalid address") + addr;
  }

  r_host = addr;
  if(port)      r_port      = port;
  if(rtcp_port) r_rtcp_port = rtcp_port;

  memcpy(&r_saddr,&ss,sizeof(struct sockaddr_storage));
  am_set_port(&r_saddr,r_port);

  mute = ((r_saddr.ss_family == AF_INET) && 
	  (SAv4(&r_saddr)->sin_addr.s_addr == INADDR_ANY)) ||
    ((r_saddr.ss_family == AF_INET6) && 
     IN6_IS_ADDR_UNSPECIFIED(&SAv6(&r_saddr)->sin6_addr));
}

void AmRtpStream::handleSymmetricRtp(struct sockaddr_storage* recv_addr, bool rtcp) {

  if((!rtcp && passive) || (rtcp && passive_rtcp)) {

    struct sockaddr_in* in_recv = (struct sockaddr_in*)recv_addr;
    struct sockaddr_in6* in6_recv = (struct sockaddr_in6*)recv_addr;

    struct sockaddr_in* in_addr = (struct sockaddr_in*)&r_saddr;
    struct sockaddr_in6* in6_addr = (struct sockaddr_in6*)&r_saddr;

    unsigned short port = am_get_port(recv_addr);

    // symmetric RTP
    if ( (!rtcp && (port != r_port)) || (rtcp && (port != r_rtcp_port)) ||
	 ((recv_addr->ss_family == AF_INET) &&
	  (in_addr->sin_addr.s_addr != in_recv->sin_addr.s_addr)) ||
	 ((recv_addr->ss_family == AF_INET6) &&
	  (memcmp(&in6_addr->sin6_addr,
		      &in6_recv->sin6_addr,
		      sizeof(struct in6_addr))))
	 ) {

      string addr_str = get_addr_str(recv_addr);
      setRAddr(addr_str, !rtcp ? port : 0, rtcp ? port : 0);
      DBG("Symmetric %s: setting new remote address: %s:%i\n",
	  !rtcp ? "RTP" : "RTCP", addr_str.c_str(),port);

    } else {
      const char* prot = rtcp ? "RTCP" : "RTP";
      DBG("Symmetric %s: remote end sends %s from advertised address."
	  " Leaving passive mode.\n",prot,prot);
    }

    // avoid comparing each time sender address
    if(!rtcp)
      passive = false;
    else
      passive_rtcp = false;
  }
}

void AmRtpStream::setPassiveMode(bool p)
{
  passive_rtcp = passive = p;
  if (p) {
    DBG("The other UA is NATed or passive mode forced: switched to passive mode.\n");
  } else {
    DBG("Passive mode not activated.\n");
  }
}

void AmRtpStream::getSdp(SdpMedia& m)
{
  m.port = getLocalPort();
  m.nports = 0;
  m.transport = TP_RTPAVP;
  m.send = !hold;
  m.recv = receiving;
  m.dir = SdpMedia::DirBoth;
}

void AmRtpStream::getSdpOffer(unsigned int index, SdpMedia& offer)
{
  sdp_media_index = index;
  getSdp(offer);
  offer.payloads.clear();
  payload_provider->getPayloads(offer.payloads);
}

void AmRtpStream::getSdpAnswer(unsigned int index, const SdpMedia& offer, SdpMedia& answer)
{
  sdp_media_index = index;
  getSdp(answer);
  offer.calcAnswer(payload_provider,answer);
}

int AmRtpStream::init(const AmSdp& local,
		      const AmSdp& remote,
                      bool force_passive_mode)
{
  if((sdp_media_index < 0) ||
     ((unsigned)sdp_media_index >= local.media.size()) ||
     ((unsigned)sdp_media_index >= remote.media.size()) ) {

    ERROR("Media index %i is invalid, either within local or remote SDP (or both)",sdp_media_index);
    return -1;
  }

  const SdpMedia& local_media = local.media[sdp_media_index];
  const SdpMedia& remote_media = remote.media[sdp_media_index];

  payloads.clear();
  pl_map.clear();
  payloads.resize(local_media.payloads.size());

  int i=0;
  vector<SdpPayload>::const_iterator sdp_it = local_media.payloads.begin();
  vector<Payload>::iterator p_it = payloads.begin();

  // first pass on local SDP - fill pl_map with intersection of codecs
  while(sdp_it != local_media.payloads.end()) {

    int int_pt;

    if (local_media.transport == TP_RTPAVP && sdp_it->payload_type < 20) int_pt = sdp_it->payload_type;
    else int_pt = payload_provider->getDynPayload(sdp_it->encoding_name,
        sdp_it->clock_rate,
        sdp_it->encoding_param);

    amci_payload_t* a_pl = NULL;
    if(int_pt >= 0) 
      a_pl = payload_provider->payload(int_pt);

    if(a_pl == NULL){
      if (relay_payloads.get(sdp_it->payload_type)) {
        // this payload should be relayed, ignore
        ++sdp_it;
        continue;
      } else {
        DBG("No internal payload corresponding to type %s/%i (ignoring)\n",
              sdp_it->encoding_name.c_str(),
              sdp_it->clock_rate);
	// ignore this payload
        ++sdp_it;
        continue;
      }
    };
    
    p_it->pt         = sdp_it->payload_type;
    p_it->name       = sdp_it->encoding_name;
    p_it->codec_id   = a_pl->codec_id;
    p_it->clock_rate = a_pl->sample_rate;
    p_it->advertised_clock_rate = sdp_it->clock_rate;

    pl_map[sdp_it->payload_type].index     = i;
    pl_map[sdp_it->payload_type].remote_pt = -1;
    
    ++p_it;
    ++sdp_it;
    ++i;
  }

  // remove payloads which were not initialised (because of unknown payloads
  // which are to be relayed)
  if (p_it != payloads.end()) payloads.erase(p_it, payloads.end());

  // second pass on remote SDP - initialize payload IDs used by remote (remote_pt)
  sdp_it = remote_media.payloads.begin();
  while(sdp_it != remote_media.payloads.end()) {

    // TODO: match not only on encoding name
    //       but also on parameters, if necessary
    //       Some codecs define multiple payloads
    //       with different encoding parameters
    PayloadMappingTable::iterator pmt_it = pl_map.end();
    if(sdp_it->encoding_name.empty() || (local_media.transport == TP_RTPAVP && sdp_it->payload_type < 20)){
      // must be a static payload
      pmt_it = pl_map.find(sdp_it->payload_type);
    }
    else {
      for(p_it = payloads.begin(); p_it != payloads.end(); ++p_it){

	if(!strcasecmp(p_it->name.c_str(),sdp_it->encoding_name.c_str()) && 
	   (p_it->advertised_clock_rate == (unsigned int)sdp_it->clock_rate)){
	  pmt_it = pl_map.find(p_it->pt);
	  break;
	}
      }
    }

    // TODO: remove following code once proper 
    //       payload matching is implemented
    //
    // initialize remote_pt if not already there
    if(pmt_it != pl_map.end() && (pmt_it->second.remote_pt < 0)){
      pmt_it->second.remote_pt = sdp_it->payload_type;
    }
    ++sdp_it;
  }

  if(!l_port){
    // only if socket not yet bound:
    if(session) {
      setLocalIP(session->localMediaIP());
    }
    else {
      // set local address - media c-line having precedence over session c-line
      if (local_media.conn.address.empty())
	setLocalIP(local.conn.address);
      else
	setLocalIP(local_media.conn.address);
    }

    DBG("setting local port to %i",local_media.port);
    setLocalPort(local_media.port);
  }

  setPassiveMode(remote_media.dir == SdpMedia::DirActive || force_passive_mode);

  // set remote address - media c-line having precedence over session c-line
  if (remote.conn.address.empty() && remote_media.conn.address.empty()) {
    WARN("no c= line given globally or in m= section in remote SDP\n");
    return -1;
  }
  if (remote_media.conn.address.empty())
    setRAddr(remote.conn.address, remote_media.port, remote_media.port+1);
  else
    setRAddr(remote_media.conn.address, remote_media.port, remote_media.port+1);

  if(local_media.payloads.empty()) {
    DBG("local_media.payloads.empty()\n");
    return -1;
  }

  remote_telephone_event_pt.reset(remote.telephoneEventPayload());
  if (remote_telephone_event_pt.get()) {
      DBG("remote party supports telephone events (pt=%i)\n",
	  remote_telephone_event_pt->payload_type);
  } else {
    DBG("remote party doesn't support telephone events\n");
  }

  local_telephone_event_pt.reset(local.telephoneEventPayload());

  if(local_media.recv) {
    resume();
  } else {
    pause();
  }

  if(local_media.send && !hold
     && (remote_media.port != 0)
     && (((r_saddr.ss_family == AF_INET) 
	  && (SAv4(&r_saddr)->sin_addr.s_addr != 0)) ||
	 ((r_saddr.ss_family == AF_INET6) 
	  && (!IN6_IS_ADDR_UNSPECIFIED(&SAv6(&r_saddr)->sin6_addr))))
     ) {
    mute = false;
  } else {
    mute = true;
  }

  payload = getDefaultPT();
  if(payload < 0) {
    DBG("could not set a default payload\n");
    return -1;
  }
  DBG("default payload selected = %i\n",payload);
  last_payload = payload;

  active = false; // mark as nothing received yet
  return 0;
}

void AmRtpStream::setReceiving(bool r) {
  DBG("RTP stream instance [%p] set receiving=%s\n", this, r?"true":"false");
  receiving = r;
}

void AmRtpStream::pause()
{
  DBG("RTP Stream instance [%p] pausing (receiving=false)\n", this);
  receiving = false;

#ifdef WITH_ZRTP
  if (session && session->enable_zrtp) {
    session->zrtp_session_state.stopStreams();
  }
#endif

}

void AmRtpStream::resume()
{
  DBG("RTP Stream instance [%p] resuming (receiving=true, clearing biffers/TS/TO)\n", this);
  clearRTPTimeout();
  receive_mut.lock();
  mem.clear();
  receive_buf.clear();
  while (!rtp_ev_qu.empty())
    rtp_ev_qu.pop();
  receive_mut.unlock();
  receiving = true;

#ifdef WITH_ZRTP
  if (session && session->enable_zrtp) {
    session->zrtp_session_state.startStreams(get_ssrc());
  }
#endif
}

void AmRtpStream::setOnHold(bool on_hold) {
  hold = on_hold;
}

bool AmRtpStream::getOnHold() {
  return hold;
}

void AmRtpStream::recvDtmfPacket(AmRtpPacket* p) {
  if (p->payload == getLocalTelephoneEventPT()) {
    dtmf_payload_t* dpl = (dtmf_payload_t*)p->getData();

    DBG("DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u session = [%p]\n",
	dpl->event,dpl->e,dpl->r,dpl->volume,ntohs(dpl->duration),p->timestamp, session);
    if (session) 
      session->postDtmfEvent(new AmRtpDtmfEvent(dpl, getLocalTelephoneEventRate(), p->timestamp));
  }
}

void AmRtpStream::bufferPacket(AmRtpPacket* p)
{
  clearRTPTimeout(&p->recv_time);

  if (!receiving) {

    if (passive) {
      handleSymmetricRtp(&p->addr,false);
    }

    if (force_receive_dtmf) {
      recvDtmfPacket(p);
    }

    mem.freePacket(p);
    return;
  }

  if (relay_enabled) { // todo: ZRTP
    if (force_receive_dtmf) {
      recvDtmfPacket(p);
    }

    // Relay DTMF packets if current audio payload
    // is also relayed.
    // Else, check whether or not we should relay this payload

    bool is_dtmf_packet = (p->payload == getLocalTelephoneEventPT()); 

      if (relay_raw || (is_dtmf_packet && !active) ||
	  relay_payloads.get(p->payload)) {

      if(active){
	DBG("switching to relay-mode\t(ts=%u;stream=%p)\n",
	    p->timestamp,this);
	active = false;
      }
      handleSymmetricRtp(&p->addr,false);

      if (NULL != relay_stream &&
	  (!(relay_filter_dtmf && is_dtmf_packet))) {
        relay_stream->relay(p);
      }

      mem.freePacket(p);
      return;
    }
  }

#ifndef WITH_ZRTP
  // throw away ZRTP packets 
  if(p->version != RTP_VERSION) {
      mem.freePacket(p);
      return;
  }
#endif

  receive_mut.lock();

#ifdef WITH_ZRTP
  if (session && session->enable_zrtp) {

    if (NULL == session->zrtp_session_state.zrtp_audio) {
      WARN("dropping received packet, as there's no ZRTP stream initialized\n");
      receive_mut.unlock();
      mem.freePacket(p);
      return;      
    }
 
    unsigned int size = p->getBufferSize();    
    zrtp_status_t status = zrtp_process_srtp(session->zrtp_session_state.zrtp_audio, (char*)p->getBuffer(), &size);
    switch (status)
      {
      case zrtp_status_ok: {
	p->setBufferSize(size);
	if (p->parse() < 0) {
	  ERROR("parsing decoded packet!\n");
	  mem.freePacket(p);
	} else {

          if(p->payload == getLocalTelephoneEventPT()) {
            rtp_ev_qu.push(p);
          } else {
	    if(!receive_buf.insert(ReceiveBuffer::value_type(p->timestamp,p)).second) {
	      // insert failed
	      mem.freePacket(p);
	    }
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

    if(p->payload == getLocalTelephoneEventPT()) {
      rtp_ev_qu.push(p);
    } else {
      if(!receive_buf.insert(ReceiveBuffer::value_type(p->timestamp,p)).second) {
	// insert failed
	mem.freePacket(p);
      }
    }

#ifdef WITH_ZRTP
  }
#endif
  receive_mut.unlock();
}

void AmRtpStream::clearRTPTimeout(struct timeval* recv_time) {
  memcpy(&last_recv_time, recv_time, sizeof(struct timeval));
}

void AmRtpStream::clearRTPTimeout() {
  gettimeofday(&last_recv_time,NULL);
}

int AmRtpStream::getDefaultPT()
{
  for(PayloadCollection::iterator it = payloads.begin();
      it != payloads.end(); ++it){

    // skip telephone-events payload
    if(it->codec_id == CODEC_TELEPHONE_EVENT)
      continue;

    // skip incompatible payloads
    PayloadMappingTable::iterator pl_it = pl_map.find(it->pt);
    if ((pl_it == pl_map.end()) || (pl_it->second.remote_pt < 0))
      continue;

    return it->pt;
  }

  return -1;
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

AmRtpPacket *AmRtpStream::reuseBufferedPacket()
{
  AmRtpPacket *p = NULL;

  receive_mut.lock();
  if(!receive_buf.empty()) {
    p = receive_buf.begin()->second;
    receive_buf.erase(receive_buf.begin());
  }
  receive_mut.unlock();
  return p;
}

void AmRtpStream::recvPacket(int fd)
{
  if(fd == l_rtcp_sd){
    recvRtcpPacket();
    return;
  }

  AmRtpPacket* p = mem.newPacket();
  if (!p) p = reuseBufferedPacket();
  if (!p) {
    DBG("out of buffers for RTP packets, dropping (stream [%p])\n",
	this);
    // drop received data
    AmRtpPacket dummy;
    dummy.recv(l_sd);
    return;
  }
  
  if(p->recv(l_sd) > 0){
    int parse_res = 0;

    if (logger) p->logReceived(logger, &l_saddr);

    gettimeofday(&p->recv_time,NULL);
    
    if(!relay_raw
#ifdef WITH_ZRTP
       && !(session && session->enable_zrtp)
#endif
       ) {
      parse_res = p->parse();
    }

    if (parse_res == -1) {
      DBG("error while parsing RTP packet.\n");
      clearRTPTimeout(&p->recv_time);
      mem.freePacket(p);	  
    } else {
      bufferPacket(p);
    }
  } else {
    mem.freePacket(p);
  }
}

void AmRtpStream::recvRtcpPacket()
{
  struct sockaddr_storage recv_addr;
  socklen_t recv_addr_len = sizeof(recv_addr);
  unsigned char buffer[4096];

  int recved_bytes = recvfrom(l_rtcp_sd,buffer,sizeof(buffer),0,
			      (struct sockaddr*)&recv_addr,
			      &recv_addr_len);

  if(recved_bytes < 0) {
    if((errno != EINTR) && 
       (errno != EAGAIN)) {
      ERROR("rtcp recv(%d): %s",l_rtcp_sd,strerror(errno));
    }
    return;
  }
  else
    if(!recved_bytes) return;

  static const cstring empty;
  if (logger)
    logger->log((const char *)buffer, recved_bytes, &recv_addr, &l_rtcp_saddr, empty);

  // clear RTP timer
  clearRTPTimeout();

  handleSymmetricRtp(&recv_addr,true);

  if(!relay_enabled || !relay_stream ||
     !relay_stream->l_sd)
    return;

  if((size_t)recved_bytes > sizeof(buffer)) {
    ERROR("recved huge RTCP packet (%d)",recved_bytes);
    return;
  }

  struct sockaddr_storage rtcp_raddr;
  memcpy(&rtcp_raddr,&relay_stream->r_saddr,sizeof(rtcp_raddr));
  am_set_port(&rtcp_raddr, relay_stream->r_rtcp_port);

  int err;
  if(AmConfig::UseRawSockets) {
    err = raw_sender::send((char*)buffer,recved_bytes,
			   AmConfig::RTP_Ifs[l_if].NetIfIdx,
			   &relay_stream->l_saddr,
			   &rtcp_raddr);
  }
  else {
    err = sendto(relay_stream->l_rtcp_sd,buffer,recved_bytes,0,
		 (const struct sockaddr *)&rtcp_raddr,
		 SA_len(&rtcp_raddr));
  }
  
  if(err < 0){
    ERROR("could not relay RTCP packet: %s\n",strerror(errno));
    return;
  }

  if (logger)
    logger->log((const char *)buffer, recved_bytes, &relay_stream->l_rtcp_saddr, &rtcp_raddr, empty);

}

void AmRtpStream::relay(AmRtpPacket* p)
{
  // not yet initialized
  // or muted/on-hold
  if (!l_port || mute || hold) 
    return;

  if(session && !session->onBeforeRTPRelay(p,&r_saddr))
    return;

  rtp_hdr_t* hdr = (rtp_hdr_t*)p->getBuffer();
  if (!relay_raw && !relay_transparent_seqno)
    hdr->seq = htons(sequence++);
  if (!relay_raw && !relay_transparent_ssrc)
    hdr->ssrc = htonl(l_ssrc);
  p->setAddr(&r_saddr);

  if(p->send(l_sd, AmConfig::RTP_Ifs[l_if].NetIfIdx, &l_saddr) < 0){
    ERROR("while sending RTP packet to '%s':%i\n",
	  get_addr_str(&r_saddr).c_str(),am_get_port(&r_saddr));
  }
  else {
    if (logger) p->logSent(logger, &l_saddr);
    if(session) session->onAfterRTPRelay(p,&r_saddr);
  }
}

int AmRtpStream::getLocalTelephoneEventRate()
{
  if (local_telephone_event_pt.get())
    return local_telephone_event_pt->clock_rate;
  return 0;
}

int AmRtpStream::getLocalTelephoneEventPT()
{
  if(local_telephone_event_pt.get())
    return local_telephone_event_pt->payload_type;
  return -1;
}

void AmRtpStream::setPayloadProvider(AmPayloadProvider* pl_prov)
{
  payload_provider = pl_prov;
}

void AmRtpStream::sendDtmf(int event, unsigned int duration_ms) {
  dtmf_sender.queueEvent(event,duration_ms,getLocalTelephoneEventRate());
}

void AmRtpStream::setRelayStream(AmRtpStream* stream) {
  relay_stream = stream;
  DBG("set relay stream [%p] for RTP instance [%p]\n",
      stream, this);
}

void AmRtpStream::setRelayPayloads(const PayloadMask &_relay_payloads)
{
  relay_payloads = _relay_payloads;
}

void AmRtpStream::enableRtpRelay() {
  DBG("enabled RTP relay for RTP stream instance [%p]\n", this);
  relay_enabled = true;
}

void AmRtpStream::disableRtpRelay() {
  DBG("disabled RTP relay for RTP stream instance [%p]\n", this);
  relay_enabled = false;
}

void AmRtpStream::enableRawRelay()
{
  DBG("enabled RAW relay for RTP stream instance [%p]\n", this);
  relay_raw = true;
}

void AmRtpStream::disableRawRelay()
{
  DBG("disabled RAW relay for RTP stream instance [%p]\n", this);
  relay_raw = false;
}
 
void AmRtpStream::setRtpRelayTransparentSeqno(bool transparent) {
  DBG("%sabled RTP relay transparent seqno for RTP stream instance [%p]\n",
      transparent ? "en":"dis", this);
  relay_transparent_seqno = transparent;
}

void AmRtpStream::setRtpRelayTransparentSSRC(bool transparent) {
  DBG("%sabled RTP relay transparent SSRC for RTP stream instance [%p]\n",
      transparent ? "en":"dis", this);
  relay_transparent_ssrc = transparent;
}

void AmRtpStream::setRtpRelayFilterRtpDtmf(bool filter) {
  DBG("%sabled RTP relay filtering of RTP DTMF (2833 / 3744) for RTP stream instance [%p]\n",
      filter ? "en":"dis", this);
  relay_filter_dtmf = filter;
}

void AmRtpStream::stopReceiving()
{
  if (hasLocalSocket()){
    DBG("remove stream [%p] from RTP receiver\n", this);
    AmRtpReceiver::instance()->removeStream(getLocalSocket());
    if (l_rtcp_sd > 0) AmRtpReceiver::instance()->removeStream(l_rtcp_sd);
  }
}

void AmRtpStream::resumeReceiving()
{
  if (hasLocalSocket()){
    DBG("add/resume stream [%p] into RTP receiver\n",this);
    AmRtpReceiver::instance()->addStream(getLocalSocket(), this);
    if (l_rtcp_sd > 0) AmRtpReceiver::instance()->addStream(l_rtcp_sd, this);
  }
}

string AmRtpStream::getPayloadName(int payload_type)
{
  for(PayloadCollection::iterator it = payloads.begin();
      it != payloads.end(); ++it){

    if (it->pt == payload_type) return it->name;
  }

  return string("");
}

PacketMem::PacketMem()
  : cur_idx(0), n_used(0)
{
  memset(used, 0, sizeof(used));
}

inline AmRtpPacket* PacketMem::newPacket() 
{
  if(n_used >= MAX_PACKETS)
    return NULL; // full

  while(used[cur_idx])
    cur_idx = (cur_idx + 1) & MAX_PACKETS_MASK;

  used[cur_idx] = true;
  n_used++;

  AmRtpPacket* p = &packets[cur_idx];
  cur_idx = (cur_idx + 1) & MAX_PACKETS_MASK;

  return p;
}

inline void PacketMem::freePacket(AmRtpPacket* p) 
{
  if (!p)  return;

  int idx = p-packets;
  assert(idx >= 0);
  assert(idx < MAX_PACKETS);

  if(!used[idx]) {
    ERROR("freePacket() double free: n_used = %d, idx = %d",n_used,idx);
    return;
  }

  used[p-packets] = false;
  n_used--;
}

inline void PacketMem::clear() 
{
  memset(used, 0, sizeof(used));
  n_used = cur_idx = 0;
}

void AmRtpStream::setLogger(msg_logger* _logger)
{
  if (logger) dec_ref(logger);
  logger = _logger;
  if (logger) inc_ref(logger);
}

void AmRtpStream::debug()
{
#define BOOL_STR(b) ((b) ? "yes" : "no")

  if(hasLocalSocket() > 0) {
    DBG("\t<%i> <-> <%s:%i>", getLocalPort(),
        getRHost().c_str(), getRPort());
  } else {
    DBG("\t<unbound> <-> <%s:%i>",
        getRHost().c_str(), getLocalPort());
  }

  if (relay_stream) {
    DBG("\tinternal relay to stream %p (local port %i)",
      relay_stream, relay_stream->getLocalPort());
  }
  else DBG("\tno relay");

  DBG("\tmute: %s, hold: %s, receiving: %s",
      BOOL_STR(mute), BOOL_STR(hold), BOOL_STR(receiving));

#undef BOOL_STR
}
