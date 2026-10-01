/*
 * DIS network side of the SIP to DIS gateway
 *
 * One DisNetwork instance owns the DIS socket and a thread that receives
 * PDUs and sends the periodic (heartbeat) PDUs. Every SIP call is one
 * LocalRadio: a DIS radio of the gateway entity with its own radio number,
 * tuned to one frequency.
 */

#ifndef _DIS_NETWORK_H
#define _DIS_NETWORK_H

#include "AmThread.h"

#include "Dis7Pdu.h"
#include "DisAudio.h"

#include <netinet/in.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/** Audio sample rate of the gateway's radios (narrowband radio voice) */
#define DIS_GW_SAMPLE_RATE 8000

struct DisGwConfig {
  enum Mode { BROADCAST, MULTICAST, UNICAST };

  // network
  Mode           mode;
  std::string    address;          // broadcast, multicast group or peer
  unsigned short port;
  std::string    interface_addr;   // local IPv4 address, optional
  unsigned int   multicast_ttl;
  bool           multicast_loop;

  // identity
  uint8_t        exercise_id;
  dis7::EntityId entity;
  bool           absolute_timestamps;

  // radio parameters
  dis7::RadioType      radio_type;
  dis7::ModulationType modulation;
  uint8_t              input_source;
  float                bandwidth;    // Hz
  float                power;        // dBm
  uint64_t             frequency_tolerance; // Hz
  double               antenna_location[3]; // ECEF metres

  // audio
  uint16_t     encoding;           // DIS encoding type sent
  unsigned int frame_ms;           // audio per Signal PDU
  unsigned int jitter_prebuffer_ms;
  unsigned int jitter_max_ms;
  bool         local_loop;         // callers on one frequency hear each other

  // timing
  unsigned int transmitter_heartbeat_ms;
  unsigned int receiver_heartbeat_ms;
  unsigned int remote_timeout_ms;  // forget silent remote transmitters
  unsigned int tx_idle_ms;         // unkey when the caller's audio stops

  // optional PDUs
  bool             publish_receiver;
  bool             publish_entity_state;
  unsigned int     entity_heartbeat_ms;
  uint8_t          force_id;
  dis7::EntityType entity_type;
  std::string      entity_marking;

  DisGwConfig();
};

class DisNetwork;

/** One SIP call's radio. */
class LocalRadio {
  friend class DisNetwork;

  const dis7::RadioId id;
  const uint64_t      freq;
  disaudio::RxMixer   rx;

  // everything below is guarded by state_mut, which is also held while the
  // radio's PDUs are sent so that they leave in state order
  std::mutex state_mut;
  bool       attached;
  uint8_t    transmit_state;
  uint64_t   last_transmitter_ms;
  uint64_t   last_audio_ms;
  uint16_t   receiver_state;
  uint64_t   receiving_from;
  uint64_t   last_receiver_ms;

public:
  LocalRadio(const dis7::RadioId& id, uint64_t frequency,
             size_t prebuffer_samples, size_t max_samples);

  const dis7::RadioId& radioId() const { return id; }
  uint64_t frequency() const { return freq; }

  /** Mixed audio of everything this radio receives (8 kHz PCM16). */
  void readAudio(int16_t* out, size_t n) { rx.read(out, n); }
};

class DisNetwork : public AmThread {
  const DisGwConfig cfg;

  int                sd;
  struct sockaddr_in dest;
  std::atomic<bool>  running;

  std::mutex radios_mut;
  std::map<uint16_t, std::shared_ptr<LocalRadio> > radios;

  // remote transmitters, only touched by the network thread
  struct RemoteTransmitter {
    uint64_t frequency;
    uint8_t  state;
    uint64_t last_seen_ms;
    bool     unsupported_logged;
    std::unique_ptr<disaudio::LinearResampler> resampler;
    RemoteTransmitter()
      : frequency(0), state(dis7::TX_OFF), last_seen_ms(0),
        unsupported_logged(false) {}
  };
  std::map<uint64_t, RemoteTransmitter> remotes;
  uint64_t last_entity_state_ms;

  std::vector<std::shared_ptr<LocalRadio> > listeners(uint64_t frequency,
                                                      const LocalRadio* except);
  bool matches(uint64_t a, uint64_t b) const;
  dis7::PduHeader header() const;
  void send(const std::vector<uint8_t>& pdu);

  // callers hold radio.state_mut
  void sendTransmitter(LocalRadio& radio, uint64_t now);
  void sendReceiver(LocalRadio& radio, uint64_t now);

  void sendEntityState(uint64_t now);
  void handleDatagram(const uint8_t* buf, size_t len, uint64_t now);
  void handleTransmitter(const uint8_t* buf, size_t len, uint64_t now);
  void handleSignal(const uint8_t* buf, size_t len, uint64_t now);
  void housekeeping(uint64_t now);

protected:
  void run();
  void on_stop();

public:
  explicit DisNetwork(const DisGwConfig& cfg);
  ~DisNetwork();

  /** Open the DIS socket. */
  bool init();

  /** Stop the network thread and wait for it to finish. */
  void dispose();

  /** Create a radio on the given frequency, switched on and not
      transmitting. Returns null if all radio numbers are in use. */
  std::shared_ptr<LocalRadio> attach(uint64_t frequency);

  /** Switch the radio off and forget it. */
  void detach(const std::shared_ptr<LocalRadio>& radio);

  /** Key (true) or unkey the radio's transmitter. */
  void setTransmitting(LocalRadio& radio, bool transmitting);

  /** Send one frame of the radio's audio (8 kHz PCM16) as a Signal PDU and
      hand it to the other local radios on the same frequency. */
  void sendAudio(LocalRadio& radio, const int16_t* pcm, size_t n);

  static uint64_t nowMs();
};

#endif
