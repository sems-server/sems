/** @file RtpMuxStream.h */
#ifndef _RtpMuxStream_h_
#define _RtpMuxStream_h_

#include "AmRtpStream.h"
#include "singleton.h"
#include "rtp/rtp.h"
#define MAX_RTP_HDR_LEN 64
#define MAX_MUX_QUEUE_SIZE 4096 // way too much over MTU anyway

#define MAX_RTP_PACKET_LEN 512 // way too long - restricted by max mux frame length 

#define MUX_PERIODIC_SETUP_FRAME_MS  500
#define MUX_SETUP_FRAME_REPEAT       3

#define RESYNC_MAX_DELAY 10*8000   // TS resync assumed if out of that window 

#define DEFAULT_TS_INCREMENT 160
#include <map>
#include <string>

#include "crc4.h"

#define RTP_MUX_HDR_TYPE_SETUP      0
#define RTP_MUX_HDR_TYPE_COMPRESSED 1

#define RTP_MUX_HDR_TS_MULTIPLIER_40  0
#define RTP_MUX_HDR_TS_MULTIPLIER_160 1

#define RTP_MUX_HDR_TS_MULTIPLIER_LOW  40
#define RTP_MUX_HDR_TS_MULTIPLIER_HIGH 160

/**
 * \brief RTP Mux data header type
 */
typedef struct {
#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
    u_int16 t:1;         /* type  */
    u_int16 sid:7;       /* stream id */
#else
    u_int16 sid:7;       /* stream id */
    u_int16 t:1;         /* typem */
#endif
    u_int16 len:8;        /* length */
} rtp_mux_hdr_t;

typedef struct {
#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
    u_int16 t:1;         /* type   = 0*/
    u_int16 sid:7;       /* stream id */
    u_int16 len:8;       /* length */
    u_int16 dstport;     /* dstport */
    u_int16 u:1;         /* ts_inc multiplier  */
    u_int16 ts_inc:7;    /* ts_increment */
#else
    u_int16 sid:7;       /* stream id */
    u_int16 t:1;         /* typem */
    u_int16 len:8;        /* length */
    u_int16 dstport;     /* dstport */
    u_int16 ts_inc:7;    /* ts_increment */
    u_int16 u:1;         /* ts_inc multiplier  */
#endif
} rtp_mux_hdr_setup_t;


typedef struct {
#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
    u_int16 t:1;         /* type   = 1*/
    u_int16 sid:7;       /* stream id */
    u_int16 len:8;       /* length */
    u_int16 m:1;         /* marker  */
    u_int16 sn_lsb:3;    /* SN lsb */
    u_int16 ts_crc4:4;   /* TS CRC-4 */
#else
    u_int16 sid:7;       /* stream id */
    u_int16 t:1;         /* type = 1 */
    u_int16 len:8;       /* length */
    u_int16 ts_crc4:4;   /* TS CRC-4 */
    u_int16 sn_lsb:3;    /* SN lsb */
    u_int16 m:1;         /* marker  */
#endif
} rtp_mux_hdr_compressed_t;

static crc_t calc_crc4(u_int32 ts);
static u_int16 get_rtp_hdr_len(const rtp_hdr_t* hdr);
static bool rtp_hdr_changed(const rtp_hdr_t* hdr1, const rtp_hdr_t* hdr2);

static void decompress(const rtp_mux_hdr_compressed_t* rtp_mux_hdr_compressed, unsigned int ts_increment,
		       const rtp_hdr_t* old_rtp_hdr, unsigned char* rtp_restored_hdr);


using std::string;

struct MuxStreamState {
  u_int16 dstport;
  u_int16 ts_increment;
  unsigned char rtp_hdr[MAX_RTP_HDR_LEN];
  u_int16 rtp_hdr_len;

  size_t setup_frame_ctr;
  unsigned  int last_mux_packet_id;

  u_int32_t last_setup_frame_ts;

  MuxStreamState()
  : setup_frame_ctr(0), last_mux_packet_id(0), dstport(0), ts_increment(DEFAULT_TS_INCREMENT), rtp_hdr_len(0) { }

};

/** incoming */
class AmRtpMuxStream
: public AmRtpStream

{
  MuxStreamState recv_streamstates[256];
  
 public:
  AmRtpMuxStream();
  ~AmRtpMuxStream();

  void recvPacket(int fd, unsigned char* pkt, size_t len);
};

/** outgoing queue for one MUX channel */
struct MuxStreamQueue {
  string remote_ip;
  unsigned short remote_port;
  int l_sd;
  struct sockaddr_storage r_saddr;
  struct sockaddr_storage l_saddr;

  //      port          stream_id
  std::map<unsigned short, unsigned char> stream_ids;
  //      stream_id     state
  std::map<unsigned char, MuxStreamState> streamstates;

  unsigned char buf[MAX_MUX_QUEUE_SIZE];
  unsigned char* end_ptr;

  u_int32_t oldest_frame;
  bool oldest_frame_i;

  // counts up with every packet sent; for send code to determine whether queue still unsent
  unsigned  int mux_packet_id;

  int sendQueue(bool force = false);

  bool is_setup;
  int init(const string& _remote_ip, unsigned short _remote_port);
  void close();

public:
  MuxStreamQueue();
  int send(unsigned char* buffer, unsigned int b_size, const string& _remote_ip, unsigned short _remote_port, unsigned short rtp_dst_port);
  void close(const string& _remote_ip, unsigned short _remote_port, unsigned short rtp_dst_port);
};

class _AmRtpMuxSender {
  /** buffer of outgoing packets for each RTP MUX destination */
  std::map<string, MuxStreamQueue> send_queues;
  /** protects the above - fixme: possibly use lock striping */
  AmMutex send_queues_mut;

 public:
  _AmRtpMuxSender() { }
  int send(unsigned char* buffer, unsigned int b_size, const string& remote_ip, unsigned short remote_port, unsigned short rtp_dst_port);
  void close(const string& remote_ip, unsigned short remote_port, unsigned short rtp_dst_port);
};

typedef singleton<_AmRtpMuxSender> AmRtpMuxSender;

#endif
