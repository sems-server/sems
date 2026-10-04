#pragma once

// Helpers shared by the SIP to DIS gateway suites (test_sip_dis_gw and
// test_sip_dis_gw_app).

#include "../../apps/sip_dis_gw/Dis7Pdu.h"
#include "../../apps/sip_dis_gw/DisNetwork.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <vector>

namespace disgw_test {

inline uint16_t be16(const std::vector<uint8_t> &b, size_t off) {
  return (uint16_t)((b[off] << 8) | b[off + 1]);
}

inline uint32_t be32(const std::vector<uint8_t> &b, size_t off) {
  return ((uint32_t)be16(b, off) << 16) | be16(b, off + 2);
}

inline uint64_t be64(const std::vector<uint8_t> &b, size_t off) {
  return ((uint64_t)be32(b, off) << 32) | be32(b, off + 4);
}

inline float bef32(const std::vector<uint8_t> &b, size_t off) {
  uint32_t u = be32(b, off);
  float f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

inline double bef64(const std::vector<uint8_t> &b, size_t off) {
  uint64_t u = be64(b, off);
  double d;
  memcpy(&d, &u, sizeof(d));
  return d;
}

inline dis7::TransmitterPdu sampleTransmitter() {
  dis7::TransmitterPdu pdu;
  pdu.header.exercise = 3;
  pdu.header.timestamp = 0x12345679;
  pdu.radio = dis7::RadioId(dis7::EntityId(1, 200, 7), 4);
  pdu.radio_type.domain = 2;
  pdu.radio_type.country = 225;
  pdu.radio_type.nomenclature = 0x1234;
  pdu.transmit_state = dis7::TX_ON_TRANSMITTING;
  pdu.input_source = 1;
  pdu.antenna_location[0] = 4000000.5;
  pdu.antenna_location[1] = -1000.25;
  pdu.antenna_location[2] = 5000000.0;
  pdu.frequency = 118100000ULL;
  pdu.bandwidth = 25000.0f;
  pdu.power = 40.0f;
  return pdu;
}

/** a simulator transmitter (site 9) as the gateway would hear it */
inline std::vector<uint8_t> remoteTransmitter(uint16_t radio, uint64_t frequency, uint8_t state,
                                              uint8_t exercise = 1) {
  dis7::TransmitterPdu tx = sampleTransmitter();
  tx.header.exercise = exercise;
  tx.radio = dis7::RadioId(dis7::EntityId(9, 9, 9), radio);
  tx.frequency = frequency;
  tx.transmit_state = state;
  std::vector<uint8_t> b;
  dis7::encode(tx, b);
  return b;
}

/** a simulator Signal PDU with the given 16-bit PCM (big endian) */
inline std::vector<uint8_t> remoteSignal(uint16_t radio, const std::vector<int16_t> &pcm,
                                         uint32_t rate = 8000, uint16_t encoding = dis7::ENC_PCM16_BE) {
  dis7::SignalPdu sig;
  sig.header.exercise = 1;
  sig.radio = dis7::RadioId(dis7::EntityId(9, 9, 9), radio);
  sig.encoding_scheme = encoding;
  sig.sample_rate = rate;
  sig.samples = (uint16_t)pcm.size();
  for (size_t i = 0; i < pcm.size(); i++) {
    sig.data.push_back((uint8_t)((uint16_t)pcm[i] >> 8));
    sig.data.push_back((uint8_t)(pcm[i] & 0xff));
  }
  std::vector<uint8_t> b;
  dis7::encode(sig, b);
  return b;
}

/**
 * A socket standing in for the simulator. The gateway listens on the DIS
 * port of all addresses and sends to 127.0.0.2; the simulator socket is bound
 * to 127.0.0.2 on the same port, which the kernel prefers over the wildcard
 * for datagrams addressed to 127.0.0.2. Datagrams to 127.0.0.1 reach the
 * gateway only.
 */
struct SimSocket {
  int sd;
  unsigned short port;
  bool ok;

  SimSocket() : port(0), ok(false) {
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = inet_addr("127.0.0.2");
    if (bind(sd, (struct sockaddr *)&a, sizeof(a)) < 0) {
      return;
    }
    socklen_t len = sizeof(a);
    getsockname(sd, (struct sockaddr *)&a, &len);
    port = ntohs(a.sin_port);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ok = true;
  }

  ~SimSocket() { close(sd); }

  void sendToGateway(const std::vector<uint8_t> &pdu) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    sendto(sd, &pdu[0], pdu.size(), 0, (struct sockaddr *)&a, sizeof(a));
  }

  /** next PDU of the given type, skipping others (waits up to wait_ms) */
  bool receive(uint8_t type, std::vector<uint8_t> &pdu, unsigned int wait_ms = 3000) {
    uint64_t deadline = DisNetwork::nowMs() + wait_ms;
    while (DisNetwork::nowMs() < deadline) {
      pdu.resize(dis7::MAX_PDU_SIZE);
      ssize_t len = recv(sd, &pdu[0], pdu.size(), 0);
      if (len <= 0) {
        continue;
      }
      pdu.resize(len);
      if (len >= (ssize_t)dis7::HEADER_SIZE && pdu[2] == type) {
        return true;
      }
    }
    return false;
  }

  /** next Transmitter PDU */
  bool receiveTransmitter(dis7::TransmitterPdu &out, unsigned int wait_ms = 3000) {
    std::vector<uint8_t> pdu;
    return receive(dis7::PDU_TRANSMITTER, pdu, wait_ms) && dis7::decode(&pdu[0], pdu.size(), out);
  }

  /** wait for a Transmitter PDU of the radio in the given state */
  bool waitTransmitter(uint16_t radio, uint8_t state, unsigned int wait_ms = 3000) {
    uint64_t deadline = DisNetwork::nowMs() + wait_ms;
    dis7::TransmitterPdu t;
    while (DisNetwork::nowMs() < deadline) {
      if (receiveTransmitter(t, 200) && t.radio.radio == radio && t.transmit_state == state) {
        return true;
      }
    }
    return false;
  }

  /** collect every PDU of the given type arriving within wait_ms */
  std::vector<std::vector<uint8_t>> collect(uint8_t type, unsigned int wait_ms) {
    std::vector<std::vector<uint8_t>> res;
    uint64_t deadline = DisNetwork::nowMs() + wait_ms;
    while (DisNetwork::nowMs() < deadline) {
      std::vector<uint8_t> pdu(dis7::MAX_PDU_SIZE);
      ssize_t len = recv(sd, &pdu[0], pdu.size(), 0);
      if (len >= (ssize_t)dis7::HEADER_SIZE && pdu[2] == type) {
        pdu.resize(len);
        res.push_back(pdu);
      }
    }
    return res;
  }

  /** throw away everything queued so far */
  void drain() {
    std::vector<uint8_t> pdu(dis7::MAX_PDU_SIZE);
    while (recv(sd, &pdu[0], pdu.size(), MSG_DONTWAIT) > 0) {
    }
  }
};

/** a gateway network side talking to a SimSocket, unicast on loopback */
inline DisGwConfig loopbackConfig(const SimSocket &sim) {
  DisGwConfig cfg;
  cfg.mode = DisGwConfig::UNICAST;
  cfg.address = "127.0.0.2";
  cfg.port = sim.port;
  cfg.entity = dis7::EntityId(4, 5, 6);
  cfg.jitter_prebuffer_ms = 20;
  return cfg;
}

/** wait until the radio plays audio, return its first sample */
inline int16_t waitForAudio(LocalRadio &radio, unsigned int wait_ms = 1000) {
  std::vector<int16_t> heard(160, 0);
  uint64_t deadline = DisNetwork::nowMs() + wait_ms;
  while (DisNetwork::nowMs() < deadline) {
    radio.readAudio(&heard[0], heard.size());
    if (heard[0]) {
      return heard[0];
    }
    usleep(10000);
  }
  return 0;
}

/** read the radio's audio until it has been silent for quiet_ms, so that
    Signal PDUs still on their way are played (and dropped) too */
inline void drainAudio(LocalRadio &radio, unsigned int quiet_ms = 200, unsigned int wait_ms = 3000) {
  std::vector<int16_t> heard(160, 0);
  uint64_t now = DisNetwork::nowMs();
  uint64_t deadline = now + wait_ms;
  uint64_t quiet_since = now;
  while (now < deadline && now - quiet_since < quiet_ms) {
    radio.readAudio(&heard[0], heard.size());
    for (size_t j = 0; j < heard.size(); j++) {
      if (heard[j]) {
        quiet_since = DisNetwork::nowMs();
        break;
      }
    }
    usleep(10000);
    now = DisNetwork::nowMs();
  }
}

} // namespace disgw_test
