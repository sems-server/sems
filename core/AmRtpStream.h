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
/** @file AmRtpStream.h */
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
#include <queue>
#include <memory>
using std::string;
using std::auto_ptr;

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
typedef std::map<unsigned int, AmRtpPacket*, ts_less> ReceiveBuffer;
typedef std::queue<AmRtpPacket*>  RtpEventQueue;

/**
 * This provides the memory for the receive buffer.
 */
struct PacketMem {
#define MAX_PACKETS 32

  AmRtpPacket packets[MAX_PACKETS];
  bool        used[MAX_PACKETS];

  PacketMem();

  inline AmRtpPacket* newPacket();
  inline void freePacket(AmRtpPacket* p);
  inline void clear();
};


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

  unsigned int   l_ssrc;
  unsigned int   r_ssrc;
  bool           r_ssrc_i;

  /** symmetric RTP */
  bool              passive;      // passive mode ?

  /** Payload type for telephone event */
  auto_ptr<const SdpPayload> telephone_event_pt;


  PacketMem       mem;
  ReceiveBuffer   receive_buf;
  RtpEventQueue   rtp_ev_qu;
  AmMutex         receive_mut;


  /* get next packet in buffer */
  int nextPacket(AmRtpPacket*& p);
  
  AmSession*         session;


public:

  AmRtpPacket* newPacket();
  void freePacket(AmRtpPacket* p);
  
  /** Mute */
  bool mute;
  /** mute && port == 0 */
  bool hold;
  /** marker flag */
  bool begin_talk;
  /** do check rtp timeout */
  bool monitor_rtp_timeout;


  /** should we receive packets? if not -> drop */
  bool receiving;

  int send( unsigned int ts,
	    unsigned char* buffer,
	    unsigned int   size );

  int send_raw( char* packet, unsigned int length );

  int receive( unsigned char* buffer, unsigned int size,
	       unsigned int& ts, int& payload);

  /** ping the remote side, to open NATs and enable symmetric RTP */
  int ping();

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
	    
  /** Initializes a new random local port, and sets own attributes properly. */
  void setLocalPort();

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

  unsigned int get_ssrc() { return l_ssrc; }

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
  virtual void init(const vector<SdpPayload*>& sdp_payloads);

  /**
   * Stops RTP stream.
   */
  void pause();

  /**
   * Resume a paused RTP stream.
   */
  void resume();

  /** set the RTP stream on hold (mute && port = 0) */
  void setOnHold(bool on_hold);
  
  /** get the RTP stream on hold  */
  bool getOnHold();

  /** setter for monitor_rtp_timeout */
  void setMonitorRTPTimeout(bool m) { monitor_rtp_timeout = m; }
  /** getter for monitor_rtp_timeout */
  bool getMonitorRTPTimeout() { return monitor_rtp_timeout; }

  /**
   * Insert an RTP packet to the buffer.
   * Note: memory is owned by this instance.
   */
  void bufferPacket(AmRtpPacket* p);

  /*
   * clear RTP timeout at time recv_time 
   */
  void clearRTPTimeout(struct timeval* recv_time);

  virtual unsigned int bytes2samples(unsigned int) const = 0;
};

/** \brief event fired on RTP timeout */
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

