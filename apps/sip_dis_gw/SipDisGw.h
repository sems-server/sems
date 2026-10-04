/*
 * SIP to DIS radio gateway
 *
 * A SIP call to sip:<radio>@<gateway> becomes a DIS 7 radio on the radio's
 * frequency: the caller hears every DIS transmitter on that frequency, and
 * the caller's voice is sent as Transmitter and Signal PDUs while the caller
 * is keyed (push-to-talk by voice activation, DTMF or permanently).
 */

#ifndef _SIP_DIS_GW_H
#define _SIP_DIS_GW_H

#include "AmSession.h"
#include "AmAudio.h"

#include "DisNetwork.h"
#include "SipDisGwConfig.h"

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <string>

/**
 * Audio of one call: reading gives the radio's received audio, writing
 * feeds the caller's voice through the push-to-talk logic to the DIS network.
 * Both run on the session's media processor thread.
 */
class DisRadioAudio : public AmAudio {
  const SipDisGwConfig& cfg;
  std::shared_ptr<DisNetwork> net;
  std::shared_ptr<LocalRadio> radio;

  disaudio::VoxGate     vox;
  std::deque<int16_t>   preroll;
  std::vector<int16_t>  tx_frame;
  size_t                frame_samples;
  bool                  keyed;
  uint64_t              keyed_since_ms;

  // set from the session's event thread (DTMF)
  std::atomic<bool> ptt_request;

  bool wantKeyed(const int16_t* pcm, size_t n, uint64_t now);
  void sendFrames(const int16_t* pcm, size_t n);

protected:
  int read(unsigned int user_ts, unsigned int size);
  int write(unsigned int user_ts, unsigned int size);

public:
  DisRadioAudio(const SipDisGwConfig& cfg, const std::shared_ptr<DisNetwork>& net,
                const std::shared_ptr<LocalRadio>& radio);

  void setPtt(bool on) { ptt_request = on; }
};

class SipDisGwSession : public AmSession {
  const SipDisGwConfig& cfg;
  std::shared_ptr<DisNetwork> net;
  std::string radio_name;
  uint64_t frequency;

  std::shared_ptr<LocalRadio> radio;
  std::unique_ptr<DisRadioAudio> audio;

  void releaseRadio();

public:
  SipDisGwSession(const SipDisGwConfig& cfg, const std::shared_ptr<DisNetwork>& net,
                  const std::string& radio_name, uint64_t frequency);
  ~SipDisGwSession();

  void onSessionStart();
  void onDtmf(int event, int duration_msec);
  void onBye(const AmSipRequest& req);
  void onSessionTimeout();
  void onRtpTimeout();
};

class SipDisGwFactory : public AmSessionFactory {
  SipDisGwConfig cfg;
  std::shared_ptr<DisNetwork> net;

public:
  SipDisGwFactory(const std::string& name);
  ~SipDisGwFactory();

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const std::string& app_name,
                      const std::map<std::string, std::string>& app_params);
};

#endif
