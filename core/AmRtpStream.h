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
using std::pair;

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
struct SdpPayload;
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
  : public AmObject
{
protected:
  // get the next available port within configured range
  static int getNextPort();

  // payload collection
  struct Payload {
    unsigned char pt;
    string        name;
    unsigned int  clock_rate;
    int           codec_id;
  };

  typedef std::vector<Payload> PayloadCollection;
  
  // list of locally supported payloads
  PayloadCollection payloads;

  // current payload (index into @payloads)
  int payload;

  struct PayloadMapping {
    // remote payload type
    int8_t remote_pt;

    // index in payloads vector
    uint8_t index;
  };

  typedef std::map<unsigned char, PayloadMapping> PayloadMappingTable;

  // mapping from local payload type to PayloadMapping
  PayloadMappingTable pl_map;

  unsigned int sequence;

  /**
     Payload of last received packet.
     Usefull to detect talk spurt, looking 
     for comfort noise packets.
  */
  int         last_payload;

  string             r_host;
  unsigned short     r_port;

  /* local interface */
  int l_if;

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
  bool           passive;      // passive mode ?

  /** Payload type for telephone event */
  auto_ptr<const SdpPayload> remote_telephone_event_pt;
  auto_ptr<const SdpPayload> local_telephone_event_pt;

  PacketMem       mem;
  ReceiveBuffer   receive_buf;
  RtpEventQueue   rtp_ev_qu;
  AmMutex         receive_mut;

  /** if relay_stream is initialized, received RTP is relayed there */
  bool            relay_enabled;
  /** pointer to relay stream.
      NOTE: This may only be accessed in initialization
      or by the AmRtpReceiver thread while relaying!  */
  AmRtpStream*    relay_stream;

  /* get next packet in buffer */
  int nextPacket(AmRtpPacket*& p);
  
  AmSession*         session;

  int compile_and_send(const int payload, bool marker, 
		       unsigned int ts, unsigned char* buffer, 
		       unsigned int size);


  void sendDtmfPacket(unsigned int ts);
  //       event, duration
  std::queue<pair<int, unsigned int> > dtmf_send_queue;
  AmMutex dtmf_send_queue_mut;
  enum dtmf_sending_state_t {
    DTMF_SEND_NONE,      // not sending event
    DTMF_SEND_SENDING,   // sending event
    DTMF_SEND_ENDING     // sending end of event
  } dtmf_sending_state;
  pair<int, unsigned int> current_send_dtmf;
  unsigned int current_send_dtmf_ts;
  int send_dtmf_end_repeat;

  /** handle symmetric RTP - if in passive mode, update raddr from rp */
  void handleSymmetricRtp(AmRtpPacket* rp);

  void relay(AmRtpPacket* p);

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
  AmRtpStream(AmSession* _s, int _if);
  /** Stops the stream and frees all resources. */
  virtual ~AmRtpStream();

  /** returns the socket descriptor for local socket (initialized or not) */
  int hasLocalSocket();

  /** initializes and gets the socket descriptor for local socket */
  int getLocalSocket();

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
  void setPassiveMode(bool p);
  bool getPassiveMode() { return passive; }

  unsigned int get_ssrc() { return l_ssrc; }

  int getLocalTelephoneEventRate();

  /**
   * send a DTMF as RTP payload (RFC4733)
   * @param event event ID (e.g. key press), see rfc
   * @param duration_ms duration in milliseconds
   */
  void sendDtmf(int event, unsigned int duration_ms);

  /**
   * Enables RTP stream.
   * @param sdp_payload payload from the SDP message.
   * @warning start() must have been called so that play and record work.
   * @warning It should be called only if the stream has been completly initialized,
   * @warning and only once per session. Use resume() then.
   */
  // virtual int init(AmPayloadProviderInterface* payload_provider,
  // 		   const SdpMedia& remote_media, 
  // 		   const SdpConnection& conn, 
  // 		   bool remote_active);
  virtual int init(AmPayloadProviderInterface* payload_provider,
		   unsigned char media_i, 
		   const AmSdp& local,
		   const AmSdp& remote);

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

  virtual unsigned int bytes2samples(unsigned int) const;

  /** set relay stream for  RTP relaying */
  void setRelayStream(AmRtpStream* stream);

  /** ensable RTP relaying through relay stream */
  void enableRtpRelay();

  /** disable RTP relaying through relay stream */
  void disableRtpRelay();

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

