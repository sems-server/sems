/*
 * RTP receiver for SIPREC SRS.
 *
 * Binds a UDP socket, receives RTP packets, decodes G.711
 * (PCMA/PCMU) payload, and writes 16-bit PCM to a WAV file.
 * Runs a receiver thread per instance.
 */

#ifndef _SIPREC_SRS_RTP_RECEIVER_H
#define _SIPREC_SRS_RTP_RECEIVER_H

#include "AmThread.h"

#include <string>
#include <atomic>
#include <stdio.h>

using std::string;

class RtpReceiver : public AmThread {
  int sd_;                        // UDP socket
  unsigned short local_port_;     // bound port
  string local_ip_;
  FILE* wav_fp_;                  // output WAV file
  string wav_path_;
  uint32_t data_bytes_;           // PCM bytes written
  std::atomic<bool> running_;
  int codec_;                     // 0 = PCMU, 8 = PCMA

  void writeWavHeader();
  void updateWavHeader();
  static int16_t decodePCMA(uint8_t sample);
  static int16_t decodePCMU(uint8_t sample);

protected:
  void run();
  void on_stop();

public:
  RtpReceiver();
  ~RtpReceiver();

  /** Bind UDP socket and open WAV file.
   *  @return bound port on success, 0 on error */
  unsigned short init(const string& local_ip, unsigned short port,
                      const string& wav_path, int codec_pt);

  unsigned short getPort() const { return local_port_; }

  /** Stop receiving and close file */
  void shutdown();
};

// Global port allocator
class PortAllocator {
  unsigned short next_port_;
  unsigned short min_port_;
  unsigned short max_port_;
  AmMutex mutex_;

public:
  PortAllocator();
  void configure(unsigned short min_port, unsigned short max_port);
  unsigned short allocate();
};

extern PortAllocator g_port_allocator;

#endif
