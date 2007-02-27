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

#ifndef _RtpStream_h_
#define _RtpStream_h_


#include "AmSdp.h"
#include "AmThread.h"
#include "SampleArray.h"
#include "AmRtpPacket.h"
#include "AmEvent.h"

#include <netinet/in.h>

#include <string>
#include <map>
#include <memory>
using std::string;
using std::map;
using std::auto_ptr;

#define DEAD_RTP_TIME (5*60) /* 5 minutes */

// return values of AmRtpStream::receive
#define RTP_EMPTY        0 // no rtp packet available
#define RTP_PARSE_ERROR -1 // error while parsing rtp packet
#define RTP_TIMEOUT     -2 // last received packet is too old
#define RTP_DTMF        -3 // dtmf packet has been received
#define RTP_BUFFER_SIZE -4 // buffer overrun
#define RTP_UNKNOWN_PL  -5 // unknown payload


struct amci_payload_t;

class AmAudio;
class AmSession;
class SdpPayload;
typedef map<unsigned int, AmRtpPacket, ts_less> JitterBuffer;

#ifdef USE_ADAPTIVE_JB
class AmJitterBuffer;
#endif

/**
 * \brief RTP implementation
 *
 * Rtp stream high level interface.
 */
class AmRtpStream 
{
protected:
    static int next_port;
    static AmMutex port_mut;

    static int getNextPort();

    AmSharedVar<bool> runcond;
    AmMutex           runmutex;

    /** 
	Internal payload (only different from 
	payload if using dynamic payloads).
    */
    int         int_payload;

    /**
       Remote payload (only different from 
       int_payload if using dynamic payloads)
     */
    int         payload;
    unsigned int   sequence;

    /**
       Payload of last received packet.
       Usefull to detect talk spurt, looking 
       for comfort noise packets.
     */
    int         last_payload;

    string      format_parameters;

    string             r_host;
    unsigned short     r_port;
#ifdef SUPPORT_IPV6
    struct sockaddr_storage r_saddr;
    struct sockaddr_storage l_saddr;
#else
    struct sockaddr_in r_saddr;
    struct sockaddr_in l_saddr;
#endif
    unsigned short     l_port;
    int                l_sd;

    struct timeval last_recv_time;

    /** the offset RTP receive TS <-> audio_buffer TS */ 
    unsigned int   recv_offset;
    /** the recv_offset initialized ?  */ 
    bool           recv_offset_i;

    unsigned int   l_ssrc;
    unsigned int   r_ssrc;
    bool           r_ssrc_i;

    /** symmetric RTP */
    bool              passive;      // passive mode ?

    /** Payload type for telephone event */
    auto_ptr<const SdpPayload> telephone_event_pt;


#ifndef USE_ADAPTIVE_JB
    JitterBuffer    jitter_buf;
    AmMutex         jitter_mut;

    /* get next packet in buffer */
    int nextPacket(AmRtpPacket& p);
#else
    AmJitterBuffer	*m_main_jb;
    AmJitterBuffer	*m_telephone_event_jb;

    /* get next packet in buffer */
    int nextAudioPacket(AmRtpPacket& p, unsigned int ts, unsigned int ms);
#endif

    AmSession*         session;

    /** Initializes a new random local port, and sets own attributes properly. */
    void setLocalPort();


public:

    /** Mute */
    bool mute;
    bool begin_talk;

    /** should we receive packets? if not -> drop */
    bool receiving;

    int send( unsigned int ts,
	      unsigned char* buffer,
	      unsigned int   size );

#ifndef USE_ADAPTIVE_JB

    int receive( unsigned char* buffer, unsigned int size,
		 unsigned int& ts, unsigned int audio_buffer_ts);
#else
    int receive( unsigned char* buffer, unsigned int size, unsigned int *ts,
		 unsigned int audio_buffer_ts, unsigned int ms);
#endif
    
    /** Allocates resources for future use of RTP. */
    AmRtpStream(AmSession* _s=0);
    /** Stops the stream and frees all resources. */
    virtual ~AmRtpStream();

    /**
     * This function must be called before setLocalPort, because
     * setLocalPort will bind the socket and it will be not
     * possible to change the IP later
     */
    void setLocalIP(const string& ip);
	    
    /** 
     * Gets RTP port number. If no RTP port in assigned, assigns a new one.
     * @return local RTP port. 
     */
    int getLocalPort();

    /** 
     * Gets remote RTP port.
     * @return remote RTP port.
     */
    int getRPort();
    
    /**
     * Gets remote host IP.
     * @return remote host IP.
     */
    string getRHost();

    /**
     * Set remote IP & port.
     */
    void setRAddr(const string& addr, unsigned short port);

    /** Symmetric RTP: passive mode ? */
    void setPassiveMode(bool p) { passive = p; }
    bool getPassiveMode() { return passive; }

    /** 
     * Set remote telephone event 
     * payload type 
     */
    void setTelephoneEventPT(const SdpPayload *pt) {
        telephone_event_pt.reset(pt);
    }

    int getTelephoneEventRate();

    /**
     * Enables RTP stream.
     * @param sdp_payload payload from the SDP message.
     * @warning start() must have been called so that play and record work.
     * @warning It should be called only if the stream has been completly initialized,
     * @warning and only once per session. Use resume() then.
     */
    virtual void init(const SdpPayload* sdp_payload);

    /**
     * Stops RTP stream.
     */
    void pause();

    /**
     * Resume a paused RTP stream.
     */
    void resume();

    /**
     * Report an ICMP error.
     */
    void icmpError();

    /**
     * Insert an RTP packet to the buffer.
     * Note: memory is owned by this instance.
     */
    void bufferPacket(const AmRtpPacket* p);

    virtual unsigned int bytes2samples(unsigned int) const = 0;
};
/** \brief represents info about an \ref AmRtpStream */
struct AmRtpStreamInfo
{
    enum StreamType { 
	ST_Receive=1, 
	ST_Send=2,
	ST_Duplex=3
    };

    StreamType   type;
    AmAudio*     audio_play;
    AmAudio*     audio_rec;
    bool         ts_offset_i;
    unsigned int ts_offset;

    AmRtpStreamInfo(StreamType type, 
		    AmAudio* audio_play = NULL, 
		    AmAudio* audio_rec = NULL);
};

class AmRtpTimeoutEvent
	: public AmEvent
{
	
public:
	AmRtpTimeoutEvent() 
		: AmEvent(0) { }
	~AmRtpTimeoutEvent() { }
};

#endif

// Local Variables:
// mode:C++
// End:





