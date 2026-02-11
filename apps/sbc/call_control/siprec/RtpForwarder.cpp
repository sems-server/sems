/*
 * RTP stream forwarder for SIPREC recording.
 *
 * Binds to a known local port for symmetric RTP and opens an RTCP
 * socket on port+1 per RFC 7866 Section 8.1.1 / 8.1.8.
 */

#include "RtpForwarder.h"
#include "AmRtpPacket.h"
#include "log.h"
#include "sip/ip_util.h"

#include <unistd.h>
#include <cstring>
#include <errno.h>

// --- SiprecPortAllocator ---

unsigned short SiprecPortAllocator::next_port_ = 0;
unsigned short SiprecPortAllocator::min_port_ = 50000;
unsigned short SiprecPortAllocator::max_port_ = 50999;
AmMutex SiprecPortAllocator::mutex_;

void SiprecPortAllocator::configure(unsigned short min_port,
                                     unsigned short max_port) {
  AmLock lock(mutex_);
  // Ensure min_port is even (RTP convention)
  min_port_ = (min_port % 2 == 0) ? min_port : min_port + 1;
  max_port_ = max_port;
  next_port_ = min_port_;
  DBG("SIPREC PortAllocator: configured range %u-%u\n", min_port_, max_port_);
}

unsigned short SiprecPortAllocator::allocate() {
  AmLock lock(mutex_);
  if (next_port_ == 0) next_port_ = min_port_;

  unsigned short port = next_port_;
  // Ensure even port (RTP); RTCP will be port+1
  if (port % 2 != 0) port++;

  next_port_ = port + 2; // skip past RTCP port
  if (next_port_ > max_port_) {
    next_port_ = min_port_;
  }

  return port;
}

// --- RtpForwarder ---

RtpForwarder::RtpForwarder()
  : sd_(-1), rtcp_sd_(-1), local_port_(0), active_(false)
{
  memset(&dest_, 0, sizeof(dest_));
}

RtpForwarder::~RtpForwarder() {
  stop();
  if (sd_ >= 0) {
    close(sd_);
    sd_ = -1;
  }
  if (rtcp_sd_ >= 0) {
    close(rtcp_sd_);
    rtcp_sd_ = -1;
  }
}

static int create_and_bind_udp(const string& local_ip, unsigned short port, int af) {
  int sd = socket(af, SOCK_DGRAM, 0);
  if (sd < 0) {
    ERROR("SIPREC RtpForwarder: socket() failed: %s\n", strerror(errno));
    return -1;
  }

  // Allow address reuse for quick restart
  int reuse = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_storage local_addr;
  memset(&local_addr, 0, sizeof(local_addr));

  if (local_ip.empty() || local_ip == "0.0.0.0" || local_ip == "::") {
    // Bind to INADDR_ANY
    if (af == AF_INET) {
      struct sockaddr_in* a4 = (struct sockaddr_in*)&local_addr;
      a4->sin_family = AF_INET;
      a4->sin_port = htons(port);
      a4->sin_addr.s_addr = INADDR_ANY;
    } else {
      struct sockaddr_in6* a6 = (struct sockaddr_in6*)&local_addr;
      a6->sin6_family = AF_INET6;
      a6->sin6_port = htons(port);
      a6->sin6_addr = in6addr_any;
    }
  } else {
    // Parse local IP using SEMS utility
    if (am_inet_pton(local_ip.c_str(), &local_addr) != 1) {
      ERROR("SIPREC RtpForwarder: invalid local IP '%s'\n", local_ip.c_str());
      close(sd);
      return -1;
    }
    am_set_port(&local_addr, port);
  }

  socklen_t addr_len = (af == AF_INET)
    ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

  if (::bind(sd, (struct sockaddr*)&local_addr, addr_len) < 0) {
    ERROR("SIPREC RtpForwarder: bind(%s:%u) failed: %s\n",
          local_ip.c_str(), port, strerror(errno));
    close(sd);
    return -1;
  }

  return sd;
}

int RtpForwarder::init(const string& local_ip, unsigned short local_port,
                        const string& dest_ip, unsigned short dest_port) {
  if (dest_port == 0) {
    ERROR("SIPREC RtpForwarder: SRS port is 0, cannot forward\n");
    return -1;
  }

  // Parse destination address
  if (am_inet_pton(dest_ip.c_str(), &dest_) != 1) {
    ERROR("SIPREC RtpForwarder: invalid SRS IP '%s'\n", dest_ip.c_str());
    return -1;
  }
  am_set_port(&dest_, dest_port);

  int af = dest_.ss_family;
  local_port_ = local_port;

  // Create and bind RTP socket to local port (symmetric RTP - RFC 7866 8.1.8)
  sd_ = create_and_bind_udp(local_ip, local_port, af);
  if (sd_ < 0) return -1;

  // Create and bind RTCP socket on port+1 (RFC 7866 8.1.1 - RTCP REQUIRED)
  rtcp_sd_ = create_and_bind_udp(local_ip, local_port + 1, af);
  if (rtcp_sd_ < 0) {
    WARN("SIPREC RtpForwarder: RTCP socket bind(%s:%u) failed, "
         "continuing without RTCP\n", local_ip.c_str(), local_port + 1);
    // Non-fatal: RTCP is REQUIRED but we can still forward RTP
  }

  DBG("SIPREC RtpForwarder: initialized, local %s:%u/%u -> target %s:%u\n",
      local_ip.c_str(), local_port, local_port + 1, dest_ip.c_str(), dest_port);

  return 0;
}

void RtpForwarder::start() {
  active_.store(true);
  DBG("SIPREC RtpForwarder: started (local port %u)\n", local_port_);
}

void RtpForwarder::stop() {
  active_.store(false);
  DBG("SIPREC RtpForwarder: stopped\n");
}

void RtpForwarder::sendPacket(AmRtpPacket* p) {
  if (!active_.load() || sd_ < 0 || !p)
    return;

  socklen_t addr_len = (dest_.ss_family == AF_INET)
    ? sizeof(struct sockaddr_in)
    : sizeof(struct sockaddr_in6);

  ssize_t ret = ::sendto(sd_, p->getBuffer(), p->getBufferSize(), 0,
                         (struct sockaddr*)&dest_, addr_len);
  if (ret < 0) {
    // Don't flood logs - only log occasionally
    static int err_count = 0;
    if (++err_count % 1000 == 1) {
      ERROR("SIPREC RtpForwarder: sendto() failed: %s (error #%d)\n",
            strerror(errno), err_count);
    }
  }
}
