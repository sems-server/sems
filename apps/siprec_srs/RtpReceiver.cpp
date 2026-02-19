/*
 * RTP receiver for SIPREC SRS.
 */

#include "RtpReceiver.h"
#include "log.h"
#include "sip/ip_util.h"

#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define RTP_HEADER_SIZE 12
#define RTP_MAX_PACKET  1500
#define WAV_SAMPLE_RATE 8000
#define WAV_BITS        16
#define WAV_CHANNELS    1

// --- G.711 A-law decode table (ITU-T G.711) ---
static const int16_t alaw_table[256] = {
   -5504,  -5248,  -6016,  -5760,  -4480,  -4224,  -4992,  -4736,
   -7552,  -7296,  -8064,  -7808,  -6528,  -6272,  -7040,  -6784,
   -2752,  -2624,  -3008,  -2880,  -2240,  -2112,  -2496,  -2368,
   -3776,  -3648,  -4032,  -3904,  -3264,  -3136,  -3520,  -3392,
  -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
  -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
  -11008, -10496, -12032, -11520,  -8960,  -8448,  -9984,  -9472,
  -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
    -344,   -328,   -376,   -360,   -280,   -264,   -312,   -296,
    -472,   -456,   -504,   -488,   -408,   -392,   -440,   -424,
     -88,    -72,   -120,   -104,    -24,     -8,    -56,    -40,
    -216,   -200,   -248,   -232,   -152,   -136,   -184,   -168,
   -1376,  -1312,  -1504,  -1440,  -1120,  -1056,  -1248,  -1184,
   -1888,  -1824,  -2016,  -1952,  -1632,  -1568,  -1760,  -1696,
    -688,   -656,   -752,   -720,   -560,   -528,   -624,   -592,
    -944,   -912,  -1008,   -976,   -816,   -784,   -880,   -848,
    5504,   5248,   6016,   5760,   4480,   4224,   4992,   4736,
    7552,   7296,   8064,   7808,   6528,   6272,   7040,   6784,
    2752,   2624,   3008,   2880,   2240,   2112,   2496,   2368,
    3776,   3648,   4032,   3904,   3264,   3136,   3520,   3392,
   22016,  20992,  24064,  23040,  17920,  16896,  19968,  18944,
   30208,  29184,  32256,  31232,  26112,  25088,  28160,  27136,
   11008,  10496,  12032,  11520,   8960,   8448,   9984,   9472,
   15104,  14592,  16128,  15616,  13056,  12544,  14080,  13568,
     344,    328,    376,    360,    280,    264,    312,    296,
     472,    456,    504,    488,    408,    392,    440,    424,
      88,     72,    120,    104,     24,      8,     56,     40,
     216,    200,    248,    232,    152,    136,    184,    168,
    1376,   1312,   1504,   1440,   1120,   1056,   1248,   1184,
    1888,   1824,   2016,   1952,   1632,   1568,   1760,   1696,
     688,    656,    752,    720,    560,    528,    624,    592,
     944,    912,   1008,    976,    816,    784,    880,    848
};

// --- G.711 u-law decode table (ITU-T G.711) ---
static const int16_t ulaw_table[256] = {
  -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
  -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
  -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
  -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
   -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
   -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
   -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
   -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
   -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
   -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
    -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
    -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
    -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
    -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
    -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
     -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
   32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
   23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
   15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
   11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
    7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
    5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
    3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
    2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
    1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
    1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
     876,    844,    812,    780,    748,    716,    684,    652,
     620,    588,    556,    524,    492,    460,    428,    396,
     372,    356,    340,    324,    308,    292,    276,    260,
     244,    228,    212,    196,    180,    164,    148,    132,
     120,    112,    104,     96,     88,     80,     72,     64,
      56,     48,     40,     32,     24,     16,      8,      0
};

// --- PortAllocator ---

PortAllocator g_port_allocator;

PortAllocator::PortAllocator()
  : next_port_(40000), min_port_(40000), max_port_(40999)
{
}

void PortAllocator::configure(unsigned short min_port, unsigned short max_port) {
  min_port_ = min_port;
  max_port_ = max_port;
  next_port_ = min_port;
}

unsigned short PortAllocator::allocate() {
  mutex_.lock();
  unsigned short port = next_port_;
  next_port_ += 2;  // RTP uses even ports
  if (next_port_ > max_port_)
    next_port_ = min_port_;
  mutex_.unlock();
  return port;
}

// --- RtpReceiver ---

RtpReceiver::RtpReceiver()
  : sd_(-1), local_port_(0), wav_fp_(NULL),
    data_bytes_(0), running_(false), codec_(8)
{
}

RtpReceiver::~RtpReceiver() {
  shutdown();
}

unsigned short RtpReceiver::init(const string& local_ip, unsigned short port,
                                 const string& wav_path, int codec_pt) {
  local_ip_ = local_ip;
  local_port_ = port;
  wav_path_ = wav_path;
  codec_ = codec_pt;

  // Create and bind UDP socket (dual-stack: supports both IPv4 and IPv6)
  struct sockaddr_storage local_addr;
  memset(&local_addr, 0, sizeof(local_addr));
  int af = AF_INET;

  if (local_ip.empty() || local_ip == "0.0.0.0") {
    SAv4(&local_addr)->sin_family = AF_INET;
    SAv4(&local_addr)->sin_port = htons(port);
    SAv4(&local_addr)->sin_addr.s_addr = INADDR_ANY;
  } else if (local_ip == "::") {
    af = AF_INET6;
    SAv6(&local_addr)->sin6_family = AF_INET6;
    SAv6(&local_addr)->sin6_port = htons(port);
    SAv6(&local_addr)->sin6_addr = in6addr_any;
  } else {
    if (am_inet_pton(local_ip.c_str(), &local_addr) != 1) {
      ERROR("SRS RtpReceiver: invalid local IP '%s'\n", local_ip.c_str());
      return 0;
    }
    af = local_addr.ss_family;
    am_set_port(&local_addr, port);
  }

  sd_ = socket(af, SOCK_DGRAM, 0);
  if (sd_ < 0) {
    ERROR("SRS RtpReceiver: socket() failed: %s\n", strerror(errno));
    return 0;
  }

  if (bind(sd_, (struct sockaddr*)&local_addr, SA_len(&local_addr)) < 0) {
    ERROR("SRS RtpReceiver: bind() port %u failed: %s\n",
          port, strerror(errno));
    close(sd_);
    sd_ = -1;
    return 0;
  }

  // Open WAV file
  wav_fp_ = fopen(wav_path.c_str(), "wb");
  if (!wav_fp_) {
    ERROR("SRS RtpReceiver: cannot open '%s': %s\n",
          wav_path.c_str(), strerror(errno));
    close(sd_);
    sd_ = -1;
    return 0;
  }

  writeWavHeader();
  data_bytes_ = 0;

  DBG("SRS RtpReceiver: bound port %u, writing to '%s', codec pt=%d\n",
      port, wav_path.c_str(), codec_pt);

  return port;
}

void RtpReceiver::writeWavHeader() {
  // Write a placeholder WAV header (44 bytes) — updated on close
  uint8_t hdr[44];
  memset(hdr, 0, sizeof(hdr));

  // RIFF header
  memcpy(hdr, "RIFF", 4);
  // file size - 8 (placeholder, updated later)
  uint32_t file_size = 36;  // will be updated
  memcpy(hdr + 4, &file_size, 4);
  memcpy(hdr + 8, "WAVE", 4);

  // fmt sub-chunk
  memcpy(hdr + 12, "fmt ", 4);
  uint32_t fmt_size = 16;
  memcpy(hdr + 16, &fmt_size, 4);
  uint16_t audio_format = 1;  // PCM
  memcpy(hdr + 20, &audio_format, 2);
  uint16_t channels = WAV_CHANNELS;
  memcpy(hdr + 22, &channels, 2);
  uint32_t sample_rate = WAV_SAMPLE_RATE;
  memcpy(hdr + 24, &sample_rate, 4);
  uint32_t byte_rate = WAV_SAMPLE_RATE * WAV_CHANNELS * (WAV_BITS / 8);
  memcpy(hdr + 28, &byte_rate, 4);
  uint16_t block_align = WAV_CHANNELS * (WAV_BITS / 8);
  memcpy(hdr + 32, &block_align, 2);
  uint16_t bits = WAV_BITS;
  memcpy(hdr + 34, &bits, 2);

  // data sub-chunk
  memcpy(hdr + 36, "data", 4);
  uint32_t data_size = 0;  // placeholder
  memcpy(hdr + 40, &data_size, 4);

  fwrite(hdr, 1, 44, wav_fp_);
}

void RtpReceiver::updateWavHeader() {
  if (!wav_fp_) return;

  // Update RIFF chunk size
  fseek(wav_fp_, 4, SEEK_SET);
  uint32_t file_size = 36 + data_bytes_;
  fwrite(&file_size, 4, 1, wav_fp_);

  // Update data sub-chunk size
  fseek(wav_fp_, 40, SEEK_SET);
  fwrite(&data_bytes_, 4, 1, wav_fp_);

  fflush(wav_fp_);
}

int16_t RtpReceiver::decodePCMA(uint8_t sample) {
  return alaw_table[sample];
}

int16_t RtpReceiver::decodePCMU(uint8_t sample) {
  return ulaw_table[sample];
}

void RtpReceiver::run() {
  running_.store(true);
  INFO("SRS RtpReceiver: started on port %u\n", local_port_);

  struct pollfd pfd;
  pfd.fd = sd_;
  pfd.events = POLLIN;

  uint8_t buf[RTP_MAX_PACKET];

  while (running_.load()) {
    int ret = poll(&pfd, 1, 500);  // 500ms timeout for clean shutdown
    if (ret < 0) {
      if (errno == EINTR) continue;
      ERROR("SRS RtpReceiver: poll() failed: %s\n", strerror(errno));
      break;
    }
    if (ret == 0) continue;  // timeout

    ssize_t len = recv(sd_, buf, sizeof(buf), 0);
    if (len < RTP_HEADER_SIZE) continue;

    // Skip RTP header (12 bytes minimum, more with CSRC/extensions)
    uint8_t cc = buf[0] & 0x0F;        // CSRC count
    bool has_ext = (buf[0] >> 4) & 1;  // extension bit
    int hdr_len = RTP_HEADER_SIZE + cc * 4;

    if (has_ext && len > hdr_len + 4) {
      // Extension header: 2 bytes profile + 2 bytes length (in 32-bit words)
      uint16_t ext_len = (buf[hdr_len + 2] << 8) | buf[hdr_len + 3];
      hdr_len += 4 + ext_len * 4;
    }

    if (hdr_len >= len) continue;  // no payload

    // Read payload type from RTP header (byte 1, bits 0-6)
    uint8_t pt = buf[1] & 0x7F;

    // Skip non-audio payload types (CN=13, telephone-event=96-127)
    if (pt == 13 || pt >= 96) continue;

    int payload_len = len - hdr_len;
    uint8_t* payload = buf + hdr_len;

    // Decode G.711 payload to 16-bit PCM based on actual RTP payload type
    int16_t pcm_buf[RTP_MAX_PACKET];
    for (int i = 0; i < payload_len; i++) {
      if (pt == 0)
        pcm_buf[i] = decodePCMU(payload[i]);
      else
        pcm_buf[i] = decodePCMA(payload[i]);
    }

    if (wav_fp_) {
      size_t written = fwrite(pcm_buf, sizeof(int16_t), payload_len, wav_fp_);
      data_bytes_ += written * sizeof(int16_t);
    }
  }

  INFO("SRS RtpReceiver: stopped on port %u, %u bytes written to '%s'\n",
       local_port_, data_bytes_, wav_path_.c_str());
}

void RtpReceiver::on_stop() {
  running_.store(false);
}

void RtpReceiver::shutdown() {
  if (!running_.load() && sd_ < 0 && !wav_fp_)
    return;

  running_.store(false);

  // Close socket to unblock recv()
  if (sd_ >= 0) {
    ::shutdown(sd_, SHUT_RDWR);
    close(sd_);
    sd_ = -1;
  }

  // Wait for thread to finish
  stop();

  // Finalize WAV file
  if (wav_fp_) {
    updateWavHeader();
    fclose(wav_fp_);
    wav_fp_ = NULL;
  }
}
