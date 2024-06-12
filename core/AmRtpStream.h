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
#include "AmDtmfSender.h"

#include <netinet/in.h>

#include <string>
#include <map>
#include <queue>
#include <memory>
using std::string;
using std::unique_ptr;
using std::pair;

// return values of AmRtpStream::receive
#define RTP_EMPTY        0 // no rtp packet available
#define RTP_ERROR       -1 // generic error
#define RTP_PARSE_ERROR -2 // error while parsing rtp packet
#define RTP_TIMEOUT     -3 // last received packet is too old
#define RTP_DTMF        -4 // dtmf packet has been received
#define RTP_BUFFER_SIZE -5 // buffer overrun
#define RTP_UNKNOWN_PL  -6 // unknown payload


/**
 * Forward declarations
 */
class  AmAudio;
class  AmSession;
struct SdpPayload;
struct amci_payload_t;
class msg_logger;

/**
 * This provides the memory for the receive buffer.
 */
struct PacketMem {
#define MAX_PACKETS_BITS 5
#define MAX_PACKETS (1<<MAX_PACKETS_BITS)
#define MAX_PACKETS_MASK (MAX_PACKETS-1)

  AmRtpPacket packets[MAX_PACKETS];
  bool        used[MAX_PACKETS];

  PacketMem();

  inline AmRtpPacket* newPacket();
  inline void freePacket(AmRtpPacket* p);
  inline void clear();

private:
  unsigned int cur_idx;
  unsigned int n_used;
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

/** helper class for assigning boolean floag to a payload ID
 * it is used to check if the payload should be relayed or not */
class PayloadMask
{
  private:
    unsigned char bits[16];

  public:
    // clear flag for all payloads
    void clear();

    // set given flag (TODO: once it shows to be working, change / and % to >> and &)
    void set(unsigned char payload_id) { if (payload_id < 128) bits[payload_id / 8] |= 1 << (payload_id % 8); }

    // set all flags to 'true'
    void set_all();

    // invert all flags
    void invert();

    // get given flag
    bool get(unsigned char payload_id) { if (payload_id > 127) { ERROR("BUG: payload_id out of range\n"); return false; } return (bits[payload_id / 8] & (1 << (payload_id % 8))); }
    
    PayloadMask() { clear(); }
    PayloadMask(bool _set_all) { if (_set_all) set_all(); else clear(); }
    PayloadMask(const PayloadMask &src);
};

/**
 * \brief represents one admissible payload type
 *
 *
 */
struct Payload {
  unsigned char pt;
  string        name;
  unsigned int  clock_rate;
  unsigned int  advertised_clock_rate; // differs for G722
  int           codec_id;
  string        format_parameters;
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

  // payload collection
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

  typedef std::map<unsigned int, AmRtpPacket*, ts_less> ReceiveBuffer;
  typedef std::queue<AmRtpPacket*>                      RtpEventQueue;
  typedef std::map<unsigned char, PayloadMapping>       PayloadMappingTable;
  
  // mapping from local payload type to PayloadMapping
  PayloadMappingTable pl_map;

  /** SDP media slot number (n-th media line) */
  int sdp_media_index;

  /** RTP sequence number */
  unsigned int sequence;

  /**
     Payload of last received packet.
     Usefull to detect talk spurt, looking 
     for comfort noise packets.
  */
  int         last_payload;

  /** Remote host information */
  string             r_host;
  unsigned short     r_port;
  unsigned short     r_rtcp_port;

  /** 
   * Local interface used for this stream
   * (index into @AmConfig::Ifs)
   */
  int l_if;

  /**
   * Local and remote host addresses
   */
  struct sockaddr_storage r_saddr;
  struct sockaddr_storage l_saddr;
  struct sockaddr_storage l_rtcp_saddr;

  /** Local port */
  unsigned short     l_port;

  /** Local socket */
  int                l_sd;

  /** Local RTCP port */
  unsigned int l_rtcp_port;

  /** Local RTCP socket */
  int          l_rtcp_sd;

  /** Timestamp of the last received RTP packet */
  struct timeval last_recv_time;

  /** Local and remote SSRC information */
  unsigned int   l_ssrc;
  unsigned int   r_ssrc;
  bool           r_ssrc_i;

  /** symmetric RTP & RTCP */
  bool           passive;
  bool           passive_rtcp;

  /** mute && port == 0 */
  bool           hold;

  bool           remotehold;

  /** marker flag */
  bool           begin_talk;

  /** do check rtp timeout */
  bool           monitor_rtp_timeout;

  /** Payload type for telephone event */
  unique_ptr<const SdpPayload> remote_telephone_event_pt;
  unique_ptr<const SdpPayload> local_telephone_event_pt;

  /** DTMF sender */
  AmDtmfSender   dtmf_sender;

  /**
   * Receive buffer, queue and mutex
   */
  PacketMem       mem;
  ReceiveBuffer   receive_buf;
  RtpEventQueue   rtp_ev_qu;
  AmMutex         receive_mut;

  /** should we receive packets? if not -> drop */
  bool receiving;

  /** if relay_stream is initialized, received RTP is relayed there */
  bool            relay_enabled;
  /** if true, packets are note parsed or checked */
  bool            relay_raw;
  /** pointer to relay stream.
      NOTE: This may only be accessed in initialization
      or by the AmRtpReceiver thread while relaying!  */
  AmRtpStream*    relay_stream;
  /** control transparency for RTP seqno in RTP relay mode */
  bool            relay_transparent_seqno;
  /** control transparency for RTP ssrc in RTP relay mode */
  bool            relay_transparent_ssrc;
  /** filter RTP DTMF (2833 / 4733) in relaying */
  bool            relay_filter_dtmf;

  /** remote IP for RTP MUX - empty if disabled */
  string          rtp_mux_remote_ip;
  /** remote port for RTP MUX - 0 if disabled*/
  unsigned int    rtp_mux_remote_port;
 
  /** Session owning this stream */
  AmSession*         session;

  msg_logger *logger;

  /** Payload provider */
  AmPayloadProvider* payload_provider;

  /** insert packet in DTMF queue if correct payload */
  void recvDtmfPacket(AmRtpPacket* p);

  /** Insert an RTP packet to the buffer queue */
  void bufferPacket(AmRtpPacket* p);
  /* Get next packet from the buffer queue */
  int nextPacket(AmRtpPacket*& p);
  
  /** Try to reuse oldest buffered packet for newly coming packet */
  AmRtpPacket *reuseBufferedPacket();

  /** handle symmetric RTP/RTCP - if in passive mode, update raddr from rp */
  void handleSymmetricRtp(struct sockaddr_storage* recv_addr, bool rtcp);

  void relay(AmRtpPacket* p);

  /** Sets generic parameters on SDP media */
  void getSdp(SdpMedia& m);

  /** Clear RTP timeout at time recv_time */
  void clearRTPTimeout(struct timeval* recv_time);

  PayloadMask relay_payloads;
  bool offer_answer_used;

  /** set to true if any data received */
  bool active;

  /** 
   * Select a compatible default payload 
   * @return -1 if none available.
   */
  int getDefaultPT();

public:

  /**
   * Set whether RTP stream will receive RTP packets internally (received packets will be dropped or not).
   */
  void setReceiving(bool r);

  /**
   * Stops RTP stream receiving RTP packets internally (received packets will be dropped).
   */
  void pause();

  /**
   * Resume a paused RTP stream internally (received packets will be processed).
   */
  void resume();

  /** Mute */
  bool mute;

  /** should we receive RFC-2833-style DTMF even when receiving is disabled? */
  bool force_receive_dtmf;

  /** Allocates resources for future use of RTP. */
  AmRtpStream(AmSession* _s, int _if);

  /** Stops the stream and frees all resources. */
  virtual ~AmRtpStream();

  int send( unsigned int ts,
	    unsigned char* buffer,
	    unsigned int   size );
  
  int send_raw( char* packet, unsigned int length );

  int compile_and_send( const int payload, bool marker, 
		        unsigned int ts, unsigned char* buffer, 
		        unsigned int size );

  int receive( unsigned char* buffer, unsigned int size,
	       unsigned int& ts, int& payload );

  virtual void recvPacket(int fd, unsigned char* pkt = NULL, size_t len = 0);

  void recvRtcpPacket(AmRtpPacket* p = NULL);

  /** ping the remote side, to open NATs and enable symmetric RTP */
  int ping();

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
	    
  /** 
   * Initializes with a new random local port if 'p' is 0,
   * else binds the given port, and sets own attributes properly. 
   */
  void setLocalPort(unsigned short p = 0);

  /** 
   * Gets RTP port number. If no RTP port in assigned, assigns a new one.
   * @return local RTP port. 
   */
  int getLocalPort();

  /** 
   * Gets RTCP port number. If no RTP/RTCP port in assigned, assigns a new one.
   * @return local RTCP port. 
   */
  int getLocalRtcpPort();

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
  void setRAddr(const string& addr, unsigned short port,
		unsigned short rtcp_port = 0);

  /** Symmetric RTP & RTCP: passive mode ? */
  void setPassiveMode(bool p);
  bool getPassiveMode() { return passive || passive_rtcp; }

  unsigned int get_ssrc() { return l_ssrc; }

  int getLocalTelephoneEventPT();
  int getLocalTelephoneEventRate();

  void setPayloadProvider(AmPayloadProvider* pl_prov);

  int getSdpMediaIndex() { return sdp_media_index; }
  void forceSdpMediaIndex(int idx) { sdp_media_index = idx; offer_answer_used = false; }
  int getPayloadType() { return payload; }
  int getLastPayload() { return last_payload; }
  string getPayloadName(int payload_type);

  /**
   * send a DTMF as RTP payload (RFC4733)
   * @param event event ID (e.g. key press), see rfc
   * @param duration_ms duration in milliseconds
   */
  void sendDtmf(int event, unsigned int duration_ms);

  /**
   * Generate an SDP offer based on the stream capabilities.
   * @param index index of the SDP media within the SDP.
   * @param offer the local offer to be filled/completed.
   */
  virtual void getSdpOffer(unsigned int index, SdpMedia& offer);

  /**
   * Generate an answer for the given SDP media based on the stream capabilities.
   * @param index index of the SDP media within the SDP.
   * @param offer the remote offer.
   * @param answer the local answer to be filled/completed.
   */
  virtual void getSdpAnswer(unsigned int index, const SdpMedia& offer, SdpMedia& answer);

  /**
   * Enables RTP stream.
   * @param local the SDP message generated by the local UA.
   * @param remote the SDP message generated by the remote UA.
   * @warning It is necessary to call getSdpOffer/getSdpAnswer prior to init(...)
   * @warning so that the internal SDP media line index is set properly.
   */
  virtual int init(const AmSdp& local, const AmSdp& remote, bool force_passive_mode = false);

  /** set the RTP stream on hold */
  void setOnHold(bool on_hold);
  
  /** get whether RTP stream is on hold  */
  bool getOnHold();

  /** request the remote end to set the RTP stream on hold */
  void setRemoteHold(bool remote_hold);

  /** get whether the remote end is requested to set the RTP stream is on hold  */
  bool getRemoteHold();

  /** setter for monitor_rtp_timeout */
  void setMonitorRTPTimeout(bool m) { monitor_rtp_timeout = m; }
  /** getter for monitor_rtp_timeout */
  bool getMonitorRTPTimeout() { return monitor_rtp_timeout; }

  /*
   * clear RTP timeout to current time
   */
  void clearRTPTimeout();

  /** set relay stream for  RTP relaying */
  void setRelayStream(AmRtpStream* stream);

  /** set relay payloads for  RTP relaying */
  void setRelayPayloads(const PayloadMask &_relay_payloads);

  /** ensable RTP relaying through relay stream */
  void enableRtpRelay();

  /** disable RTP relaying through relay stream */
  void disableRtpRelay();

  /** enable raw UDP relaying through relay stream */
  void enableRawRelay();

  /** disable raw UDP relaying through relay stream */
  void disableRawRelay();

  /** enable or disable transparent RTP seqno for relay */
  void setRtpRelayTransparentSeqno(bool transparent);

  /** enable or disable transparent SSRC seqno for relay */
  void setRtpRelayTransparentSSRC(bool transparent);

  /** enable or disable filtering of RTP DTMF for relay */
  void setRtpRelayFilterRtpDtmf(bool filter);

  /** set RTP Mux remote IP / port ("" and 0 to disable) */
  void setRtpMuxRemote(const string& remote_ip, unsigned int remote_port);

  /** remove from RTP receiver */
  void stopReceiving();

  /** (re-)insert into RTP receiver */
  void resumeReceiving();

  /** Quick hack to assign existing stream to another session. The stream should
   * not be reinitialised implicitly (it might be used for media traffic
   * already). */
  void changeSession(AmSession *_s) { session = _s; }

  /** set destination for logging all received/sent RTP and RTCP packets */
  void setLogger(msg_logger *_logger);

  void debug();
};

#endif

// Local Variables:
// mode:C++
// End:

