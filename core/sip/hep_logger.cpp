#include "hep_logger.h"
#include "ip_util.h"

#include "log.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// HEPv3 chunk types, see https://github.com/sipcapture/HEP
#define CHUNK_IP_FAMILY  0x0001
#define CHUNK_IP_PROTO   0x0002
#define CHUNK_SRC_IP4    0x0003
#define CHUNK_DST_IP4    0x0004
#define CHUNK_SRC_IP6    0x0005
#define CHUNK_DST_IP6    0x0006
#define CHUNK_SRC_PORT   0x0007
#define CHUNK_DST_PORT   0x0008
#define CHUNK_TS_SEC     0x0009
#define CHUNK_TS_USEC    0x000a
#define CHUNK_PROTO_TYPE 0x000b
#define CHUNK_CAPTURE_ID 0x000c
#define CHUNK_PAYLOAD    0x000f
#define CHUNK_CORR_ID    0x0011

#define HEP_PROTO_SIP    0x01

namespace {

  // header part of a HEPv3 packet; the payload goes in a second iovec
  struct hep_hdr_writer {
    unsigned char buf[128];
    size_t len;

    hep_hdr_writer() : len(6) { memcpy(buf, "HEP3", 4); }

    void chunk(uint16_t type, const void* data, uint16_t data_len) {
      uint16_t h[3] = { 0, htons(type), htons(6 + data_len) };
      memcpy(buf + len, h, 6);
      if(data_len) memcpy(buf + len + 6, data, data_len);
      len += 6 + data_len;
    }

    void u8(uint16_t type, uint8_t v) { chunk(type, &v, 1); }
    void u16(uint16_t type, uint16_t v) { v = htons(v); chunk(type, &v, 2); }
    void u32(uint16_t type, uint32_t v) { v = htonl(v); chunk(type, &v, 4); }

    void addr(uint16_t type4, uint16_t type6, uint16_t port_type,
	      const sockaddr_storage* sa) {
      if(sa->ss_family == AF_INET6) {
	chunk(type6, &SAv6(sa)->sin6_addr, sizeof(in6_addr));
	u16(port_type, ntohs(SAv6(sa)->sin6_port));
      } else {
	chunk(type4, &SAv4(sa)->sin_addr, sizeof(in_addr));
	u16(port_type, ntohs(SAv4(sa)->sin_port));
      }
    }
  };

  // one socket per address family for the whole process, not one per call
  int shared_socket(int family)
  {
    if(family == AF_INET6) {
      static int sd6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      return sd6;
    }
    static int sd4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sd4;
  }

}

hep_logger::hep_logger()
  : capt_id(0), sd(-1)
{
  memset(&capt_addr, 0, sizeof(capt_addr));
}

int hep_logger::init(const string& dest, const string& correlation_id)
{
  string addr = dest;
  corr_id = correlation_id;

  size_t semi = addr.find(';');
  if(semi != string::npos) {
    string params = addr.substr(semi + 1);
    addr.resize(semi);
    if(params.compare(0, 3, "id=") == 0)
      capt_id = strtoul(params.c_str() + 3, NULL, 10);
  }

  size_t colon = addr.rfind(':');
  if(colon == string::npos || colon == 0) {
    ERROR("hep_logger: missing port in '%s'\n", dest.c_str());
    return -1;
  }
  int port = atoi(addr.c_str() + colon + 1);
  addr.resize(colon);

  if(port <= 0 || port > 65535 || !am_inet_pton(addr.c_str(), &capt_addr)) {
    ERROR("hep_logger: invalid capture address '%s'\n", dest.c_str());
    return -1;
  }
  am_set_port(&capt_addr, port);

  sd = shared_socket(capt_addr.ss_family);
  if(sd == -1) {
    ERROR("hep_logger: no UDP socket for the capture server\n");
    return -1;
  }

  return 0;
}

int hep_logger::log(const char* buf, int len,
		    sockaddr_storage* src_ip,
		    sockaddr_storage* dst_ip,
		    cstring method, int reply_code)
{
  // RTP comes without method and reply code
  if(!method.len && !reply_code) return 0;

  struct timeval now;
  gettimeofday(&now, NULL);

  hep_hdr_writer h;
  h.u8(CHUNK_IP_FAMILY, src_ip->ss_family == AF_INET6 ? 10 : 2);
  h.u8(CHUNK_IP_PROTO, IPPROTO_UDP);
  h.addr(CHUNK_SRC_IP4, CHUNK_SRC_IP6, CHUNK_SRC_PORT, src_ip);
  h.addr(CHUNK_DST_IP4, CHUNK_DST_IP6, CHUNK_DST_PORT, dst_ip);
  h.u32(CHUNK_TS_SEC, now.tv_sec);
  h.u32(CHUNK_TS_USEC, now.tv_usec);
  h.u8(CHUNK_PROTO_TYPE, HEP_PROTO_SIP);
  h.u32(CHUNK_CAPTURE_ID, capt_id);
  h.chunk(CHUNK_PAYLOAD, NULL, 0);

  // the payload chunk length includes the message
  uint16_t payload_len = htons(6 + len);
  memcpy(h.buf + h.len - 2, &payload_len, 2);

  struct iovec iov[4];
  iov[0].iov_base = h.buf;
  iov[0].iov_len = h.len;
  iov[1].iov_base = (void*)buf;
  iov[1].iov_len = len;
  size_t iovcnt = 2;

  // chunk order does not matter in HEP, so the correlation ID goes last
  uint16_t corr_hdr[3];
  if(!corr_id.empty()) {
    corr_hdr[0] = 0;
    corr_hdr[1] = htons(CHUNK_CORR_ID);
    corr_hdr[2] = htons(6 + corr_id.size());
    iov[2].iov_base = corr_hdr;
    iov[2].iov_len = 6;
    iov[3].iov_base = (void*)corr_id.data();
    iov[3].iov_len = corr_id.size();
    iovcnt = 4;
  }

  size_t total = 0;
  for(size_t i = 0; i < iovcnt; i++) total += iov[i].iov_len;
  uint16_t total_len = htons(total);
  memcpy(h.buf + 4, &total_len, 2);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_name = &capt_addr;
  msg.msg_namelen = SA_len(&capt_addr);
  msg.msg_iov = iov;
  msg.msg_iovlen = iovcnt;

  // never stall SIP processing on a full send buffer, drop the copy instead
  if(sendmsg(sd, &msg, MSG_DONTWAIT) < 0) {
    DBG("hep_logger: sendmsg(): %s\n", strerror(errno));
    return -1;
  }

  return 0;
}
