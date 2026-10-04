/*
 * Audio helpers for the SIP to DIS gateway
 *
 * Everything here works on 16-bit linear PCM in host byte order and has no
 * SEMS dependencies, so that it can be unit tested on its own.
 */

#ifndef _DIS_AUDIO_H
#define _DIS_AUDIO_H

#include <stdint.h>
#include <stddef.h>

#include <deque>
#include <map>
#include <mutex>
#include <vector>

namespace disaudio {

uint8_t linearToUlaw(int16_t pcm);
int16_t ulawToLinear(uint8_t ulaw);

/** Is this Signal PDU encoding scheme (class + type) one we can decode? */
bool canDecode(uint16_t encoding_scheme);

/** Decode the data of an encoded-audio Signal PDU into PCM16. 'samples' is
    the PDU's sample count; the data length bounds it. */
bool decodeSignalData(uint16_t encoding_scheme, const std::vector<uint8_t>& data,
                      unsigned int samples, std::vector<int16_t>& out);

/** Encode PCM16 into Signal PDU data with the given encoding type. */
bool encodeSignalData(uint16_t encoding_type, const int16_t* pcm, size_t n,
                      std::vector<uint8_t>& out);

/** Streaming linear-interpolation resampler, enough for radio voice. */
class LinearResampler {
  unsigned int in_rate;
  unsigned int out_rate;
  double step;
  double pos;
  int16_t prev;
  bool has_prev;

public:
  LinearResampler(unsigned int in_rate, unsigned int out_rate);

  unsigned int inputRate() const { return in_rate; }
  void process(const int16_t* in, size_t n, std::vector<int16_t>& out);
};

/**
 * Mixes the audio of several concurrent transmitters for one listener.
 *
 * Every source gets its own small playout buffer: DIS Signal PDUs carry no
 * sequence numbers and often arrive in bursts, so playback of a source only
 * starts once 'prebuffer' samples are queued, and starts over (re-buffers)
 * after the source ran dry. A source that queues more than 'max' samples is
 * trimmed back to the prebuffer level to bound the latency.
 *
 * push() is called from the network thread, read() from the media processor
 * and expire() from the network thread's housekeeping.
 */
class RxMixer {
  struct Source {
    std::deque<int16_t> q;
    bool playing;
    uint64_t last_push_ms;
    Source() : playing(false), last_push_ms(0) {}
  };

  size_t prebuffer;
  size_t max_queued;
  std::map<uint64_t, Source> sources;
  std::mutex mut;

public:
  RxMixer(size_t prebuffer_samples, size_t max_samples);

  void push(uint64_t source, const int16_t* pcm, size_t n, uint64_t now_ms);

  /** Mix n samples of all playing sources into out (silence if none). */
  void read(int16_t* out, size_t n);

  /** Sources that have not been fed for idle_ms are played out and then
      dropped. Returns whether any source is left, and one of them. */
  bool expire(uint64_t now_ms, uint64_t idle_ms, uint64_t* active_source);
};

/** Voice operated switch: opens on a frame louder than the threshold and
    closes after 'hang' samples of quieter frames. */
class VoxGate {
  double threshold;
  size_t hang;
  size_t hang_left;
  bool open;

public:
  VoxGate(double threshold_dbfs, size_t hang_samples);

  bool isOpen() const { return open; }
  bool process(const int16_t* pcm, size_t n);
};

} // namespace disaudio

#endif
