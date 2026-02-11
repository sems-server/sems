/*
 * RTP stream forwarder for SIPREC recording.
 *
 * Forwards (forks) RTP packets to the Session Recording Server
 * via raw UDP sendto(). Designed to be called from the RTP receiver
 * thread (non-blocking, no allocation).
 *
 * Binds to a known local port for symmetric RTP (RFC 7866 Section 8.1.8)
 * and opens an RTCP socket on port+1 (RFC 7866 Section 8.1.1).
 */

#ifndef _RTP_FORWARDER_H
#define _RTP_FORWARDER_H

#include "AmThread.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <atomic>
#include <string>

using std::string;

class AmRtpPacket;

/** Thread-safe port allocator for SRC RTP/RTCP port pairs.
 *  Allocates even ports; RTCP port is always RTP port + 1. */
class SiprecPortAllocator {
  static unsigned short next_port_;
  static unsigned short min_port_;
  static unsigned short max_port_;
  static AmMutex mutex_;

public:
  /** Configure the port range (must be called before allocate).
   *  min_port should be even; range should be large enough for concurrent calls. */
  static void configure(unsigned short min_port, unsigned short max_port);

  /** Allocate an even RTP port. Returns 0 on failure.
   *  RTCP port is always the returned port + 1. */
  static unsigned short allocate();
};

class RtpForwarder {
  int sd_;                        // RTP UDP socket descriptor
  int rtcp_sd_;                   // RTCP UDP socket descriptor (port+1)
  struct sockaddr_storage dest_;  // SRS destination address (RTP)
  unsigned short local_port_;     // local RTP port (for symmetric RTP)
  std::atomic<bool> active_;

public:
  RtpForwarder();
  ~RtpForwarder();

  /** Configure local bind and remote destination, open RTP + RTCP sockets.
   *  @param local_ip    local IP to bind (from SDP c= line)
   *  @param local_port  local RTP port (even, from SiprecPortAllocator)
   *  @param dest_ip     SRS IP address
   *  @param dest_port   SRS RTP port (from SDP answer)
   *  @return 0 on success, -1 on error */
  int init(const string& local_ip, unsigned short local_port,
           const string& dest_ip, unsigned short dest_port);

  /** Start forwarding packets */
  void start();

  /** Stop forwarding packets */
  void stop();

  /** Forward a single RTP packet to the SRS.
   *  Called from onAfterRTPRelay (RTP receiver thread).
   *  Non-blocking. */
  void sendPacket(AmRtpPacket* p);

  bool isActive() const { return active_.load(); }
  unsigned short getLocalPort() const { return local_port_; }
};

#endif
