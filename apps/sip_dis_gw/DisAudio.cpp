/*
 * Audio helpers for the SIP to DIS gateway
 */

#include "DisAudio.h"
#include "Dis7Pdu.h"

#include <cmath>

namespace disaudio {

namespace {

const int ULAW_BIAS = 0x84;
const int ULAW_CLIP = 32635;

inline int16_t clip16(int32_t v)
{
  if(v > 32767) return 32767;
  if(v < -32768) return -32768;
  return (int16_t)v;
}

} // namespace

uint8_t linearToUlaw(int16_t pcm)
{
  int s = pcm;
  int sign = 0;

  if(s < 0) {
    sign = 0x80;
    s = -s;
  }
  if(s > ULAW_CLIP)
    s = ULAW_CLIP;
  s += ULAW_BIAS;

  int exponent = 7;
  for(int mask = 0x4000; !(s & mask) && exponent > 0; mask >>= 1)
    exponent--;

  int mantissa = (s >> (exponent + 3)) & 0x0f;
  return (uint8_t)~(sign | (exponent << 4) | mantissa);
}

int16_t ulawToLinear(uint8_t ulaw)
{
  ulaw = ~ulaw;
  int exponent = (ulaw >> 4) & 0x07;
  int mantissa = ulaw & 0x0f;
  int s = (((mantissa << 3) + ULAW_BIAS) << exponent) - ULAW_BIAS;
  return (int16_t)((ulaw & 0x80) ? -s : s);
}

bool canDecode(uint16_t encoding_scheme)
{
  if(encoding_scheme & dis7::ENCODING_CLASS_MASK)
    return false; // not encoded audio

  switch(encoding_scheme & dis7::ENCODING_TYPE_MASK) {
  case dis7::ENC_MULAW8:
  case dis7::ENC_PCM16_BE:
  case dis7::ENC_PCM16_LE:
  case dis7::ENC_PCM8_UNSIGNED:
    return true;
  default:
    return false;
  }
}

bool decodeSignalData(uint16_t encoding_scheme, const std::vector<uint8_t>& data,
                      unsigned int samples, std::vector<int16_t>& out)
{
  out.clear();
  if(!canDecode(encoding_scheme))
    return false;

  uint16_t type = encoding_scheme & dis7::ENCODING_TYPE_MASK;
  size_t bytes_per_sample =
    (type == dis7::ENC_PCM16_BE || type == dis7::ENC_PCM16_LE) ? 2 : 1;

  size_t n = data.size() / bytes_per_sample;
  if(samples && samples < n)
    n = samples;

  out.resize(n);
  for(size_t i = 0; i < n; i++) {
    switch(type) {
    case dis7::ENC_MULAW8:
      out[i] = ulawToLinear(data[i]);
      break;
    case dis7::ENC_PCM16_BE:
      out[i] = (int16_t)((data[2*i] << 8) | data[2*i + 1]);
      break;
    case dis7::ENC_PCM16_LE:
      out[i] = (int16_t)((data[2*i + 1] << 8) | data[2*i]);
      break;
    case dis7::ENC_PCM8_UNSIGNED:
      out[i] = (int16_t)((data[i] - 128) * 256);
      break;
    }
  }

  return true;
}

bool encodeSignalData(uint16_t encoding_type, const int16_t* pcm, size_t n,
                      std::vector<uint8_t>& out)
{
  out.clear();

  switch(encoding_type) {
  case dis7::ENC_MULAW8:
    out.resize(n);
    for(size_t i = 0; i < n; i++)
      out[i] = linearToUlaw(pcm[i]);
    return true;

  case dis7::ENC_PCM16_BE:
    out.resize(2 * n);
    for(size_t i = 0; i < n; i++) {
      out[2*i]     = (uint8_t)((uint16_t)pcm[i] >> 8);
      out[2*i + 1] = (uint8_t)(pcm[i] & 0xff);
    }
    return true;

  case dis7::ENC_PCM16_LE:
    out.resize(2 * n);
    for(size_t i = 0; i < n; i++) {
      out[2*i]     = (uint8_t)(pcm[i] & 0xff);
      out[2*i + 1] = (uint8_t)((uint16_t)pcm[i] >> 8);
    }
    return true;

  default:
    return false;
  }
}

LinearResampler::LinearResampler(unsigned int in_rate, unsigned int out_rate)
  : in_rate(in_rate), out_rate(out_rate),
    step((double)in_rate / (double)out_rate),
    pos(0), prev(0), has_prev(false)
{
}

void LinearResampler::process(const int16_t* in, size_t n, std::vector<int16_t>& out)
{
  out.clear();
  if(!n)
    return;

  if(in_rate == out_rate) {
    out.assign(in, in + n);
    return;
  }

  if(!has_prev) {
    // start interpolating at the first input sample
    prev = in[0];
    has_prev = true;
  }

  // input position -1 is the last sample of the previous call
  while(pos < (double)(n - 1)) {
    double fl = floor(pos);
    long i0 = (long)fl;
    double frac = pos - fl;

    int16_t s0 = i0 < 0 ? prev : in[i0];
    int16_t s1 = in[i0 + 1];
    out.push_back((int16_t)lrint(s0 + (s1 - s0) * frac));
    pos += step;
  }

  pos -= (double)n;
  prev = in[n - 1];
}

RxMixer::RxMixer(size_t prebuffer_samples, size_t max_samples)
  : prebuffer(prebuffer_samples),
    max_queued(max_samples > prebuffer_samples ? max_samples : prebuffer_samples)
{
}

void RxMixer::push(uint64_t source, const int16_t* pcm, size_t n, uint64_t now_ms)
{
  std::lock_guard<std::mutex> lock(mut);

  Source& src = sources[source];
  src.q.insert(src.q.end(), pcm, pcm + n);
  src.last_push_ms = now_ms;

  if(src.q.size() > max_queued)
    src.q.erase(src.q.begin(), src.q.begin() + (src.q.size() - prebuffer));
}

void RxMixer::read(int16_t* out, size_t n)
{
  std::vector<int32_t> acc(n, 0);

  {
    std::lock_guard<std::mutex> lock(mut);

    for(std::map<uint64_t, Source>::iterator it = sources.begin();
        it != sources.end(); ++it) {

      Source& src = it->second;
      if(!src.playing) {
        if(src.q.size() < prebuffer || src.q.empty())
          continue;
        src.playing = true;
      }

      size_t take = src.q.size() < n ? src.q.size() : n;
      for(size_t i = 0; i < take; i++)
        acc[i] += src.q[i];
      src.q.erase(src.q.begin(), src.q.begin() + take);

      if(src.q.empty())
        src.playing = false; // ran dry: buffer up again
    }
  }

  for(size_t i = 0; i < n; i++)
    out[i] = clip16(acc[i]);
}

bool RxMixer::expire(uint64_t now_ms, uint64_t idle_ms, uint64_t* active_source)
{
  std::lock_guard<std::mutex> lock(mut);

  std::map<uint64_t, Source>::iterator it = sources.begin();
  while(it != sources.end()) {
    Source& src = it->second;
    if(now_ms - src.last_push_ms > idle_ms) {
      if(src.q.empty()) {
        sources.erase(it++);
        continue;
      }
      src.playing = true; // play out what is left, even below the prebuffer
    }
    ++it;
  }

  if(sources.empty())
    return false;

  if(active_source)
    *active_source = sources.begin()->first;
  return true;
}

VoxGate::VoxGate(double threshold_dbfs, size_t hang_samples)
  : threshold(32768.0 * pow(10.0, threshold_dbfs / 20.0)),
    hang(hang_samples), hang_left(0), open(false)
{
}

bool VoxGate::process(const int16_t* pcm, size_t n)
{
  if(!n)
    return open;

  double sum = 0;
  for(size_t i = 0; i < n; i++)
    sum += (double)pcm[i] * (double)pcm[i];
  double rms = sqrt(sum / (double)n);

  if(rms >= threshold) {
    open = true;
    hang_left = hang;
  } else if(open) {
    if(hang_left > n) {
      hang_left -= n;
    } else {
      hang_left = 0;
      open = false;
    }
  }

  return open;
}

} // namespace disaudio
