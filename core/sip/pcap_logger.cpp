#include "pcap_logger.h"

#include "log.h"

#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <netinet/ip.h>

using namespace std;

// FIXME: currently for little endian only!

// see http://wiki.wireshark.org/Development/LibpcapFileFormat

#define LINKTYPE_ETHERNET   1 // http://www.tcpdump.org/linktypes.html
#define LINKTYPE_IPV4     228
#define LINKTYPE_RAW      101

typedef uint32_t guint32;
typedef int32_t gint32;
typedef uint16_t guint16;

typedef struct pcap_hdr_s {
  guint32 magic_number;   /* magic number */
  guint16 version_major;  /* major version number */
  guint16 version_minor;  /* minor version number */
  gint32  thiszone;       /* GMT to local correction */
  guint32 sigfigs;        /* accuracy of timestamps */
  guint32 snaplen;        /* max length of captured packets, in octets */
  guint32 network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
  guint32 ts_sec;         /* timestamp seconds */
  guint32 ts_usec;        /* timestamp microseconds */
  guint32 incl_len;       /* number of octets of packet saved in file */
  guint32 orig_len;       /* actual length of packet */
} pcaprec_hdr_t;


struct packet_header {
  pcaprec_hdr_t pcap;
  struct ip ip;
  struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t chksum;
  } udp;
};

static uint32_t sum(const void *_data, unsigned _len)
{
  const uint16_t *data = (const uint16_t *)_data;
  unsigned len = _len >> 1;

  uint32_t r = 0;
  for (unsigned i = 0; i < len; i++) r += data[i];
  if (_len & 1) r += (unsigned)((const char*)_data)[_len - 1];

  return r;
}

static uint16_t ipv4_chksum(uint32_t sum)
{
  while (sum >> 16) { sum = (sum >> 16) + (sum & 0xFFFF); }

  uint16_t res = sum;
  res = ~res;
  if (res == 0) res = ~res;

  return res;
}

//////////////////////////////////////////////////////////////////////////////////////////////

int pcap_logger::write_file_header()
{
  // write PCAP header
  pcap_hdr_t hdr;
  hdr.magic_number = 0xa1b2c3d4;
  hdr.version_major = 2;
  hdr.version_minor = 4;
  hdr.thiszone = 0;
  hdr.sigfigs = 0;
  hdr.snaplen = 65535; // FIXME
  hdr.network = LINKTYPE_RAW;

  return write(&hdr, sizeof(hdr));
}

int pcap_logger::log(const char* buf, int len,
            sockaddr_storage* src_ip,
            sockaddr_storage* dst_ip,
            cstring method, int reply_code)
{
  return log(buf, len, (sockaddr*)src_ip, (sockaddr*)dst_ip, sizeof(sockaddr_storage));
}

int pcap_logger::log(const char *data, int data_len, struct sockaddr *src, struct sockaddr *dst, size_t addr_len)
{
  if (((sockaddr_in*)src)->sin_family != AF_INET) {
    ERROR("writing only IPv4 is supported\n");
    return -1;
  }

  // generate fake IP packet to be written
  packet_header hdr;
  struct timeval t;
  gettimeofday(&t, NULL);

  memset(&hdr, 0, sizeof(hdr));
  unsigned size = data_len + sizeof(hdr) - sizeof(hdr.pcap);
  hdr.pcap.ts_sec = t.tv_sec;
  hdr.pcap.ts_usec = t.tv_usec;
  hdr.pcap.incl_len = size;
  hdr.pcap.orig_len = size;

  hdr.ip.ip_hl = 5;
  hdr.ip.ip_v = 4;
  hdr.ip.ip_tos = 0;
  hdr.ip.ip_len = htons(size);
  hdr.ip.ip_id = htonl(54321);
  hdr.ip.ip_off = 0;
  hdr.ip.ip_ttl = 255;
  hdr.ip.ip_p = 0x11; // UDP
  hdr.ip.ip_sum = 0;
  hdr.ip.ip_src.s_addr = ((sockaddr_in*)src)->sin_addr.s_addr;
  hdr.ip.ip_dst.s_addr = ((sockaddr_in*)dst)->sin_addr.s_addr;

  hdr.ip.ip_sum = ipv4_chksum(sum(&hdr.ip, sizeof(hdr.ip)));

  // UDP header
  unsigned udp_size = data_len + sizeof(hdr.udp);
  hdr.udp.src_port = ((sockaddr_in*)src)->sin_port;
  hdr.udp.dst_port = ((sockaddr_in*)dst)->sin_port;
  hdr.udp.length = htons(udp_size);
  hdr.udp.chksum = 0;

  hdr.udp.chksum = ipv4_chksum(sum(&hdr.ip.ip_src, 8) + sum(&hdr.udp, 8) + htons(udp_size) + 0x1100 + sum(data, data_len));

  AmLock _l(fd_mut);

  if (write(&hdr, sizeof(hdr)) != sizeof(hdr)) {
    // TODO: close the file (is broken anyway)
    return -1;
  }
  if (write(data, data_len) != data_len) {
    // TODO: close the file (is broken anyway)
    return -1;
  }
  return 0;
}
