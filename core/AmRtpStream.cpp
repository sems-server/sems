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
#include "AmIcmpWatcher.h"
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

#include <set>
using std::set;

#define MAX_DELAY 8000 /* 1 second */

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
    if((sd = socket(l_saddr.ss_family,SOCK_DGRAM,0)) == -1){
#else
    if((sd = socket(PF_INET,SOCK_DGRAM,0)) == -1){
#endif
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
		 sizeof(struct sockaddr_storage))){
	    break;
	}
	else {
		DBG("bind: %s\n",strerror(errno));
	}
#else	
	l_saddr.sin_port = htons(port);
	if(!bind(sd,(const struct sockaddr*)&l_saddr,
		 sizeof(struct sockaddr_in))){
	    break;
	}
	else {
		DBG("bind: %s\n",strerror(errno));		
	}
#endif
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
    //AmIcmpWatcher::instance()->addStream(l_port,this);
    AmRtpReceiver::instance()->addStream(l_sd,this);
    DBG("local rtp port set to %i\n",l_port);
}

int AmRtpStream::send( unsigned int ts, unsigned char* buffer, unsigned int size )
{
    if(!size)
	return -1;

    AmRtpPacket rp;
    rp.payload = payload;
    rp.marker = false;
    rp.sequence = sequence++;
    rp.timestamp = ts;   
    rp.ssrc = 0; //l_ssrc;
    rp.compile((unsigned char*)buffer,size);

    rp.setAddr(&r_saddr);
    if(rp.send(l_sd) < 0){
	ERROR("while sending RTP packet.\n");
	return -1;
    }
 
    return size;
}

#ifndef USE_ADAPTIVE_JB
// returns 
// @param ts              [out] timestamp of the received packet, 
//                              in audio buffer relative time
// @param audio_buffer_ts [in]  current ts at the audio_buffer 

int AmRtpStream::receive( unsigned char* buffer, unsigned int size,
			  unsigned int& ts, unsigned int audio_buffer_ts)
{
    AmRtpPacket rp;
    int err = nextPacket(rp);
    
    if(err <= 0)
	return err;

#ifdef SUPPORT_IPV6
    struct sockaddr_storage recv_addr;
#else
    struct sockaddr_in recv_addr;
#endif
    rp.getAddr(&recv_addr);

#ifndef SUPPORT_IPV6
    // symmetric RTP
    if( passive && ((recv_addr.sin_port != r_saddr.sin_port)
		    || (recv_addr.sin_addr.s_addr 
			!= r_saddr.sin_addr.s_addr)) ) {
	
	string addr_str = get_addr_str(recv_addr.sin_addr);
	int port = ntohs(recv_addr.sin_port);
	setRAddr(addr_str,port);
	DBG("Symmetric RTP: setting new remote address: %s:%i\n",addr_str.c_str(),port);
	// avoid comparing each time sender address
	passive = false;
    }
#endif

    if(rp.parse() == -1){
	ERROR("while parsing RTP packet.\n");
	return RTP_PARSE_ERROR;
    }

    /* do we have a new talk spurt? */
    begin_talk = ((last_payload == 13) || rp.marker);
    last_payload = rp.payload;
    
    if(!rp.getDataSize())
	return RTP_EMPTY;
    
    if(payload != rp.payload){
	
        if (telephone_event_pt.get() && rp.payload == telephone_event_pt->payload_type)
        {
	    dtmf_payload_t* dpl = (dtmf_payload_t*)rp.getData();

	    DBG("DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i\n",
		dpl->event,dpl->e,dpl->r,dpl->volume,ntohs(dpl->duration));
            session->postDtmfEvent(new AmRtpDtmfEvent(dpl, getTelephoneEventRate()));
            return RTP_DTMF;
	}
	else
	    return RTP_UNKNOWN_PL;
    }
    
    if(!recv_offset_i){

 	recv_offset = rp.timestamp - audio_buffer_ts;
 	recv_offset_i = true;
	DBG("initialized recv_offset with %i (%i - %i)\n", 
	    recv_offset,audio_buffer_ts,rp.timestamp);
	ts = audio_buffer_ts;// + jitter_delay;
    }
    else {
 	ts = rp.timestamp - recv_offset;// + jitter_delay;
	
	// resync
 	if( ts_less()(ts, audio_buffer_ts - MAX_DELAY/2) || 
 	    !ts_less()(ts, audio_buffer_ts + MAX_DELAY) ){

 	    DBG("resync needed: reference ts = %u; write ts = %u\n",
 		audio_buffer_ts,ts);
 	    recv_offset = rp.timestamp - audio_buffer_ts;
 	    ts = audio_buffer_ts;// + jitter_delay;
 	}
    }

    assert(rp.getData());
    if(rp.getDataSize() > size){
	ERROR("received too big RTP packet\n");
	return RTP_BUFFER_SIZE;
    }
    memcpy(buffer,rp.getData(),rp.getDataSize());

    return rp.getDataSize();    
}
#else
// returns 
// @param ts              [out] timestamp of the received packet, 
//                              in audio buffer relative time
// @param audio_buffer_ts [in]  current ts at the audio_buffer 

int AmRtpStream::receive( unsigned char* buffer, unsigned int buf_size,
			  unsigned int *ts,
			  unsigned int audio_buffer_ts, unsigned int ms)
{
    AmRtpPacket rp;
    AmRtpPacket dtmf_pkt;

    while (m_telephone_event_jb->get(dtmf_pkt, audio_buffer_ts, ms))
    {
	dtmf_payload_t* dpl = (dtmf_payload_t*)dtmf_pkt.getData();

	DBG("DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i\n",
	    dpl->event,dpl->e,dpl->r,dpl->volume,ntohs(dpl->duration));
	session->postDtmfEvent(new AmRtpDtmfEvent(dpl, getTelephoneEventRate()));
    }
    
    int err = nextAudioPacket(rp, audio_buffer_ts, ms);
    if(err <= 0)
	return err;
#ifdef SUPPORT_IPV6
    struct sockaddr_storage recv_addr;
#else
    struct sockaddr_in recv_addr;
#endif
    rp.getAddr(&recv_addr);

#ifndef SUPPORT_IPV6
    // symmetric RTP
    if( passive && ((recv_addr.sin_port != r_saddr.sin_port)
		    || (recv_addr.sin_addr.s_addr 
			!= r_saddr.sin_addr.s_addr)) ) {
	
	string addr_str = get_addr_str(recv_addr.sin_addr);
	int port = ntohs(recv_addr.sin_port);
	setRAddr(addr_str,port);
	DBG("Symmetric RTP: setting new remote address: %s:%i\n",addr_str.c_str(),port);
	// avoid comparing each time sender address
	passive = false;
    }
#endif

    /* do we have a new talk spurt? */
    begin_talk = ((last_payload == 13) || rp.marker);
    last_payload = rp.payload;
    
    if(!rp.getDataSize())
	return RTP_EMPTY;

    assert(rp.getData());
    if(rp.getDataSize() > buf_size){
	ERROR("received too big RTP packet\n");
	return RTP_BUFFER_SIZE;
    }
    memcpy(buffer,rp.getData(),rp.getDataSize());
    *ts = rp.timestamp;

    return rp.getDataSize();    
}
#endif // USE_ADAPTIVE_JB

AmRtpStream::AmRtpStream(AmSession* _s) 
    : runcond(0), 
      r_port(0),
      l_port(0),
      l_sd(0), 
      recv_offset_i(false), 
      r_ssrc_i(false),
      session(_s),
      passive(false),
      first_recved(false),
      telephone_event_pt(NULL),
      mute(false)
#ifdef USE_ADAPTIVE_JB
      , 
      m_main_jb(new AmJitterBuffer(this)),
      m_telephone_event_jb(new AmJitterBuffer(this))
#endif
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

}

AmRtpStream::~AmRtpStream()
{
    if(l_sd){
	AmRtpReceiver::instance()->removeStream(l_sd);
	close(l_sd);
    }
#ifdef USE_ADAPTIVE_JB
    if (m_main_jb) delete(m_main_jb);
    if (m_telephone_event_jb)
        delete (m_telephone_event_jb);
#endif
}

int AmRtpStream::getLocalPort()
{
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
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if(!inet_aton(addr.c_str(),&sa.sin_addr)){
 	ERROR("address not valid (host: %s)\n",addr.c_str());
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

void AmRtpStream::init(const SdpPayload* sdp_payload)
{
    payload = sdp_payload->payload_type;
    int_payload = sdp_payload->int_pt;
    last_payload = payload;
    if(!sdp_payload->sdp_format_parameters.empty())
	format_parameters = sdp_payload->sdp_format_parameters;
    else
	format_parameters = "";
    resume();
}

void AmRtpStream::pause()
{
    runcond.set(false);
}

void AmRtpStream::resume()
{
    gettimeofday(&last_recv_time,NULL);
    runcond.set(true);
}

void AmRtpStream::icmpError()
{
    if(!passive || first_recved.get()){
	AmIcmpWatcher::instance()->removeStream(l_port);
	if(session)
	    session->stop();
    }
}

#ifndef USE_ADAPTIVE_JB
void AmRtpStream::bufferPacket(const AmRtpPacket* p)
{
    jitter_mut.lock();
    gettimeofday(&last_recv_time,NULL);
    jitter_buf[p->timestamp].copy(p);
    jitter_mut.unlock();
}

int AmRtpStream::nextPacket(AmRtpPacket& p)
{
    struct timeval now;
    struct timeval diff;
    gettimeofday(&now,NULL);

    jitter_mut.lock();
    timersub(&now,&last_recv_time,&diff);
    if(diff.tv_sec > DEAD_RTP_TIME){
 	WARN("RTP Timeout detected. Last received packet is too old.\n");
	DBG("diff.tv_sec = %i\n",(unsigned int)diff.tv_sec);
	jitter_mut.unlock();
 	return RTP_TIMEOUT;
    }

    if(jitter_buf.empty()){
	jitter_mut.unlock();
	return RTP_EMPTY;
    }

    AmRtpPacket& pp = jitter_buf.begin()->second;
    p.copy(&pp);
    jitter_buf.erase(jitter_buf.begin());
    jitter_mut.unlock();

    return 1;
}
#else
void AmRtpStream::bufferPacket(const AmRtpPacket* p)
{
    gettimeofday(&last_recv_time,NULL);
    if (p->payload == payload && m_main_jb)
	m_main_jb->put(p);
    else if (telephone_event_pt.get() && p->payload == telephone_event_pt->payload_type)
	m_telephone_event_jb->put(p);
}

int AmRtpStream::nextAudioPacket(AmRtpPacket& p, unsigned int ts, unsigned int ms)
{
    if (m_main_jb && m_main_jb->get(p, ts, ms))
	return 1;

    struct timeval now;
    struct timeval diff;
    gettimeofday(&now,NULL);

    timersub(&now,&last_recv_time,&diff);
    if(diff.tv_sec > DEAD_RTP_TIME){
 	WARN("RTP Timeout detected. Last received packet is too old.\n");
	DBG("diff.tv_sec = %i\n",(unsigned int)diff.tv_sec);
 	return RTP_TIMEOUT;
    }
    return RTP_EMPTY;
}
#endif

int AmRtpStream::getTelephoneEventRate()
{
    int retval = 0;
    if (telephone_event_pt.get())
        retval = telephone_event_pt->clock_rate;
    return retval;
}
