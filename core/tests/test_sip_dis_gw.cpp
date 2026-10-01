#include "fct.h"

#include "AmConfigReader.h"

#include "../../apps/sip_dis_gw/Dis7Pdu.h"
#include "../../apps/sip_dis_gw/DisAudio.h"
#include "../../apps/sip_dis_gw/DisNetwork.h"
#include "../../apps/sip_dis_gw/SipDisGwConfig.h"

#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <vector>

// The SIP to DIS gateway (apps/sip_dis_gw): DIS 7 PDU layouts, the audio
// helpers, the configuration and the network side on the loopback interface.

namespace {

uint16_t be16(const std::vector<uint8_t> &b, size_t off) { return (uint16_t)((b[off] << 8) | b[off + 1]); }

uint32_t be32(const std::vector<uint8_t> &b, size_t off) {
  return ((uint32_t)be16(b, off) << 16) | be16(b, off + 2);
}

uint64_t be64(const std::vector<uint8_t> &b, size_t off) {
  return ((uint64_t)be32(b, off) << 32) | be32(b, off + 4);
}

float bef32(const std::vector<uint8_t> &b, size_t off) {
  uint32_t u = be32(b, off);
  float f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

dis7::TransmitterPdu sampleTransmitter() {
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
    tv.tv_usec = 200000;
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

  /** next PDU of the given type, skipping others (waits up to 3 s) */
  bool receive(uint8_t type, std::vector<uint8_t> &pdu) {
    uint64_t deadline = DisNetwork::nowMs() + 3000;
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
  bool receiveTransmitter(dis7::TransmitterPdu &out) {
    std::vector<uint8_t> pdu;
    return receive(dis7::PDU_TRANSMITTER, pdu) && dis7::decode(&pdu[0], pdu.size(), out);
  }
};

/** wait until the radio plays audio, return its first sample */
int16_t waitForAudio(LocalRadio &radio) {
  std::vector<int16_t> heard(160, 0);
  for (int i = 0; i < 100; i++) {
    radio.readAudio(&heard[0], heard.size());
    if (heard[0]) {
      return heard[0];
    }
    usleep(10000);
  }
  return 0;
}

} // namespace

FCTMF_SUITE_BGN(test_sip_dis_gw) {

  FCT_TEST_BGN(transmitter_pdu_layout) {
    std::vector<uint8_t> b;
    dis7::encode(sampleTransmitter(), b);

    fct_chk_eq_int(b.size(), dis7::TRANSMITTER_PDU_SIZE);
    fct_chk_eq_int(b[0], 7); // protocol version
    fct_chk_eq_int(b[1], 3); // exercise
    fct_chk_eq_int(b[2], dis7::PDU_TRANSMITTER);
    fct_chk_eq_int(b[3], dis7::FAMILY_RADIO_COMMUNICATIONS);
    fct_chk(be32(b, 4) == 0x12345679);
    fct_chk_eq_int(be16(b, 8), 104);
    fct_chk_eq_int(be16(b, 12), 1);      // site
    fct_chk_eq_int(be16(b, 14), 200);    // application
    fct_chk_eq_int(be16(b, 16), 7);      // entity
    fct_chk_eq_int(be16(b, 18), 4);      // radio number
    fct_chk_eq_int(b[20], 7);            // radio entity kind
    fct_chk_eq_int(b[21], 2);            // domain
    fct_chk_eq_int(be16(b, 22), 225);    // country
    fct_chk_eq_int(be16(b, 26), 0x1234); // nomenclature
    fct_chk_eq_int(b[28], dis7::TX_ON_TRANSMITTING);
    fct_chk_eq_int(b[29], 1);             // input source
    fct_chk(be64(b, 72) == 118100000ULL); // frequency
    fct_chk(bef32(b, 80) == 25000.0f);    // bandwidth
    fct_chk(bef32(b, 84) == 40.0f);       // power
    fct_chk_eq_int(be16(b, 88), 0);       // spread spectrum
    fct_chk_eq_int(be16(b, 90), 1);       // major: amplitude
    fct_chk_eq_int(be16(b, 92), 2);       // detail: AM
    fct_chk_eq_int(be16(b, 94), 1);       // system: generic

    dis7::TransmitterPdu d;
    fct_chk(dis7::decode(&b[0], b.size(), d));
    fct_chk(d.radio == sampleTransmitter().radio);
    fct_chk(d.frequency == 118100000ULL);
    fct_chk_eq_int(d.transmit_state, dis7::TX_ON_TRANSMITTING);
    fct_chk(d.antenna_location[0] == 4000000.5);
    fct_chk(d.antenna_location[1] == -1000.25);
    fct_chk(d.bandwidth == 25000.0f);
    fct_chk_eq_int(d.radio_type.nomenclature, 0x1234);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(signal_pdu_layout) {
    dis7::SignalPdu pdu;
    pdu.radio = dis7::RadioId(dis7::EntityId(1, 2, 3), 9);
    pdu.encoding_scheme = dis7::ENC_MULAW8;
    pdu.sample_rate = 8000;
    pdu.samples = 160;
    pdu.data.assign(160, 0x55);

    std::vector<uint8_t> b;
    dis7::encode(pdu, b);
    fct_chk_eq_int(b.size(), 32 + 160);
    fct_chk_eq_int(b[2], dis7::PDU_SIGNAL);
    fct_chk_eq_int(be16(b, 8), 192);
    fct_chk_eq_int(be16(b, 18), 9);                // radio number
    fct_chk_eq_int(be16(b, 20), dis7::ENC_MULAW8); // encoding scheme
    fct_chk_eq_int(be16(b, 22), 0);                // TDL type
    fct_chk_eq_int(be32(b, 24), 8000);             // sample rate
    fct_chk_eq_int(be16(b, 28), 1280);             // data length in bits
    fct_chk_eq_int(be16(b, 30), 160);              // samples
    fct_chk_eq_int(b[32], 0x55);

    // odd data sizes are padded to 32 bits, the bit length stays exact
    pdu.data.assign(3, 0xaa);
    pdu.samples = 3;
    dis7::encode(pdu, b);
    fct_chk_eq_int(b.size(), 36);
    fct_chk_eq_int(be16(b, 8), 36);
    fct_chk_eq_int(be16(b, 28), 24);

    dis7::SignalPdu d;
    fct_chk(dis7::decode(&b[0], b.size(), d));
    fct_chk_eq_int(d.data.size(), 3);
    fct_chk_eq_int(d.samples, 3);
    fct_chk(d.radio == pdu.radio);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(receiver_and_entity_state_layout) {
    dis7::ReceiverPdu rx;
    rx.radio = dis7::RadioId(dis7::EntityId(1, 2, 3), 4);
    rx.receiver_state = dis7::RX_ON_RECEIVING;
    rx.received_power = -60.0f;
    rx.transmitter = dis7::RadioId(dis7::EntityId(5, 6, 7), 8);

    std::vector<uint8_t> b;
    dis7::encode(rx, b);
    fct_chk_eq_int(b.size(), dis7::RECEIVER_PDU_SIZE);
    fct_chk_eq_int(b[2], dis7::PDU_RECEIVER);
    fct_chk_eq_int(be16(b, 20), dis7::RX_ON_RECEIVING);
    fct_chk_eq_int(be16(b, 28), 5); // transmitter site
    fct_chk_eq_int(be16(b, 34), 8); // transmitter radio

    dis7::ReceiverPdu d;
    fct_chk(dis7::decode(&b[0], b.size(), d));
    fct_chk(d.transmitter == rx.transmitter);
    fct_chk(d.received_power == -60.0f);

    dis7::EntityStatePdu es;
    es.entity = dis7::EntityId(1, 200, 1);
    es.entity_type.kind = 1;
    es.entity_type.country = 840;
    memcpy(es.marking, "SEMS-GW", 7);
    dis7::encode(es, b);
    fct_chk_eq_int(b.size(), dis7::ENTITY_STATE_PDU_SIZE);
    fct_chk_eq_int(b[2], dis7::PDU_ENTITY_STATE);
    fct_chk_eq_int(b[3], dis7::FAMILY_ENTITY_INFORMATION);
    fct_chk_eq_int(b[20], 1);         // entity kind
    fct_chk_eq_int(be16(b, 22), 840); // country
    fct_chk_eq_int(b[128], 1);        // ASCII marking
    fct_chk(!memcmp(&b[129], "SEMS-GW", 7));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(decode_rejects_malformed_pdus) {
    std::vector<uint8_t> b;
    dis7::encode(sampleTransmitter(), b);
    dis7::TransmitterPdu t;
    dis7::PduHeader h;

    // truncated datagram
    fct_chk(!dis7::decode(&b[0], b.size() - 1, t));
    // header shorter than the fixed part
    fct_chk(!dis7::decode(&b[0], 11, t));
    // unknown protocol version
    b[0] = 8;
    fct_chk(!dis7::decodeHeader(&b[0], b.size(), h));
    // older versions share the radio PDU layout
    b[0] = 6;
    fct_chk(dis7::decode(&b[0], b.size(), t));
    // length field too small for a Transmitter PDU
    b[8] = 0;
    b[9] = 40;
    fct_chk(!dis7::decode(&b[0], b.size(), t));

    // signal whose data length exceeds the PDU
    dis7::SignalPdu s;
    s.data.assign(16, 0);
    dis7::encode(s, b);
    b[28] = 0x10; // 4096 bits
    b[29] = 0x00;
    dis7::SignalPdu d;
    fct_chk(!dis7::decode(&b[0], b.size(), d));

    // wrong PDU type
    dis7::encode(sampleTransmitter(), b);
    fct_chk(!dis7::decode(&b[0], b.size(), d));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(timestamp_and_ecef) {
    struct timespec ts;
    ts.tv_sec = 5 * 3600 + 1800; // half past
    ts.tv_nsec = 0;
    fct_chk(dis7::timestamp(ts, false) == 0x80000000U);
    fct_chk(dis7::timestamp(ts, true) == 0x80000001U);
    ts.tv_sec = 7 * 3600;
    fct_chk(dis7::timestamp(ts, true) == 1U);

    double e[3];
    dis7::geodeticToEcef(0, 0, 0, e);
    fct_chk(fabs(e[0] - 6378137.0) < 0.001);
    fct_chk(fabs(e[1]) < 0.001 && fabs(e[2]) < 0.001);
    dis7::geodeticToEcef(90, 0, 100, e);
    fct_chk(fabs(e[2] - 6356852.314) < 0.01);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(ulaw_and_signal_data) {
    fct_chk_eq_int(disaudio::linearToUlaw(0), 0xff);
    fct_chk_eq_int(disaudio::ulawToLinear(0xff), 0);
    fct_chk_eq_int(disaudio::ulawToLinear(0x80), 32124);
    fct_chk_eq_int(disaudio::ulawToLinear(0x00), -32124);

    // every code word except negative zero survives decode + encode
    bool stable = true;
    for (int u = 0; u < 256; u++) {
      if (u == 0x7f) {
        continue;
      }
      if (disaudio::linearToUlaw(disaudio::ulawToLinear((uint8_t)u)) != u) {
        stable = false;
      }
    }
    fct_chk(stable);
    fct_chk_eq_int(disaudio::linearToUlaw(-32768), 0x00);

    int16_t pcm[3] = {1000, -2, 0x1234};
    std::vector<uint8_t> data;
    std::vector<int16_t> out;

    fct_chk(disaudio::encodeSignalData(dis7::ENC_PCM16_BE, pcm, 3, data));
    fct_chk_eq_int(data.size(), 6);
    fct_chk_eq_int(data[4], 0x12);
    fct_chk(disaudio::decodeSignalData(dis7::ENC_PCM16_BE, data, 3, out));
    fct_chk(out.size() == 3 && out[0] == 1000 && out[1] == -2 && out[2] == 0x1234);

    fct_chk(disaudio::encodeSignalData(dis7::ENC_PCM16_LE, pcm, 3, data));
    fct_chk_eq_int(data[4], 0x34);
    fct_chk(disaudio::decodeSignalData(dis7::ENC_PCM16_LE, data, 0, out));
    fct_chk(out.size() == 3 && out[2] == 0x1234);

    // the sample count bounds the data, the data bounds the sample count
    fct_chk(disaudio::decodeSignalData(dis7::ENC_PCM16_LE, data, 2, out));
    fct_chk_eq_int(out.size(), 2);
    fct_chk(disaudio::decodeSignalData(dis7::ENC_PCM16_LE, data, 50, out));
    fct_chk_eq_int(out.size(), 3);

    data.assign(2, 0);
    data[0] = 128;
    data[1] = 255;
    fct_chk(disaudio::decodeSignalData(dis7::ENC_PCM8_UNSIGNED, data, 2, out));
    fct_chk(out[0] == 0 && out[1] == 127 * 256);

    // other encoding classes and types are refused
    fct_chk(!disaudio::canDecode(0x4000 | dis7::ENC_MULAW8));
    fct_chk(!disaudio::canDecode(2)); // CVSD
    fct_chk(!disaudio::decodeSignalData(2, data, 2, out));
    fct_chk(!disaudio::encodeSignalData(2, pcm, 3, data));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(resampler) {
    std::vector<int16_t> in(320, 1000);
    std::vector<int16_t> out;
    size_t total = 0;
    bool flat = true;

    disaudio::LinearResampler down(16000, 8000);
    for (int i = 0; i < 50; i++) {
      down.process(&in[0], in.size(), out);
      total += out.size();
      for (size_t j = 0; j < out.size(); j++) {
        if (out[j] != 1000) {
          flat = false;
        }
      }
    }
    fct_chk(total >= 50 * 160 - 1 && total <= 50 * 160 + 1);
    fct_chk(flat);

    // 11025 Hz to 8 kHz keeps the long-run rate
    disaudio::LinearResampler odd(11025, 8000);
    total = 0;
    for (int i = 0; i < 100; i++) {
      odd.process(&in[0], 220, out);
      total += out.size();
    }
    double expected = 100 * 220 * 8000.0 / 11025.0;
    fct_chk(fabs((double)total - expected) <= 2);

    // a ramp stays a ramp across chunk boundaries
    disaudio::LinearResampler up(8000, 16000);
    std::vector<int16_t> ramp(10), all;
    for (int c = 0; c < 3; c++) {
      for (int i = 0; i < 10; i++) {
        ramp[i] = (int16_t)((c * 10 + i) * 100);
      }
      up.process(&ramp[0], ramp.size(), out);
      all.insert(all.end(), out.begin(), out.end());
    }
    bool linear = true;
    for (size_t i = 0; i < all.size(); i++) {
      if (all[i] != (int16_t)(i * 50)) {
        linear = false;
      }
    }
    fct_chk(linear);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(rx_mixer) {
    disaudio::RxMixer mixer(160, 800);
    std::vector<int16_t> a(100, 1000), b(400, 30000), out(80);

    // below the prebuffer: silence
    mixer.push(1, &a[0], a.size(), 0);
    mixer.read(&out[0], out.size());
    fct_chk(out[0] == 0 && out[79] == 0);

    // prebuffer reached: plays, and sums with clipping
    mixer.push(1, &a[0], a.size(), 0);
    mixer.push(2, &b[0], b.size(), 0);
    mixer.read(&out[0], out.size());
    fct_chk_eq_int(out[0], 31000);
    mixer.push(3, &b[0], b.size(), 0);
    mixer.read(&out[0], out.size());
    fct_chk_eq_int(out[0], 32767);

    // a source that ran dry buffers up again
    disaudio::RxMixer m2(160, 800);
    m2.push(1, &b[0], 200, 0);
    out.resize(200);
    m2.read(&out[0], out.size());
    fct_chk_eq_int(out[199], 30000);
    m2.push(1, &a[0], 100, 10);
    m2.read(&out[0], 100);
    fct_chk_eq_int(out[0], 0);

    // an idle source plays out what is left below the prebuffer, then goes
    uint64_t active = 0;
    fct_chk(m2.expire(100, 500, &active));
    fct_chk(active == 1);
    fct_chk(m2.expire(700, 500, &active));
    m2.read(&out[0], 100);
    fct_chk_eq_int(out[0], 1000);
    fct_chk(!m2.expire(800, 500, &active));

    // the queue is trimmed back to the prebuffer when it grows too long
    disaudio::RxMixer m3(160, 800);
    std::vector<int16_t> big(1000, 5);
    m3.push(1, &big[0], big.size(), 0);
    out.resize(170);
    m3.read(&out[0], out.size());
    fct_chk_eq_int(out[159], 5);
    fct_chk_eq_int(out[160], 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(vox_gate) {
    disaudio::VoxGate vox(-40, 400);
    std::vector<int16_t> quiet(160, 50), loud(160, 3000);

    fct_chk(!vox.process(&quiet[0], quiet.size()));
    fct_chk(vox.process(&loud[0], loud.size()));
    fct_chk(vox.process(&quiet[0], quiet.size()));  // 240 left
    fct_chk(vox.process(&quiet[0], quiet.size()));  // 80 left
    fct_chk(!vox.process(&quiet[0], quiet.size())); // closed
  }
  FCT_TEST_END();

  FCT_TEST_BGN(config) {
    AmConfigReader r;
    const char *conf = "dis_mode=multicast\n"
                       "dis_address=239.1.2.3\n"
                       "dis_port=3001\n"
                       "dis_site_id=5\n"
                       "radio_entity_type=7.2.826.1.0.17\n"
                       "modulation=0.3.1.1\n"
                       "transmit_bandwidth=8.33kHz\n"
                       "encoding=pcm16be\n"
                       "radio_tower=118.1MHz\n"
                       "radio_guard=243000000\n"
                       "ptt_mode=dtmf\n";
    r.loadString(conf, strlen(conf));

    SipDisGwConfig c;
    fct_chk(c.load(r));
    fct_chk(c.dis.mode == DisGwConfig::MULTICAST);
    fct_chk_eq_int(c.dis.port, 3001);
    fct_chk_eq_int(c.dis.entity.site, 5);
    fct_chk_eq_int(c.dis.radio_type.domain, 2);
    fct_chk_eq_int(c.dis.radio_type.country, 826);
    fct_chk_eq_int(c.dis.radio_type.nomenclature, 17);
    fct_chk_eq_int(c.dis.modulation.major, 3);
    fct_chk(c.dis.bandwidth == 8330.0f);
    fct_chk_eq_int(c.dis.encoding, dis7::ENC_PCM16_BE);
    fct_chk(c.ptt_mode == SipDisGwConfig::PTT_DTMF);
    fct_chk_eq_int(c.radios.size(), 2);

    uint64_t f = 0;
    fct_chk(c.lookupFrequency("tower", f) && f == 118100000ULL);
    fct_chk(c.lookupFrequency("guard", f) && f == 243000000ULL);
    fct_chk(c.lookupFrequency("121500", f) && f == 121500000ULL);
    fct_chk(c.lookupFrequency("121.5", f) && f == 121500000ULL);
    fct_chk(!c.lookupFrequency("tower2", f));
    fct_chk(!c.lookupFrequency("1.2.3", f));
    fct_chk(!c.lookupFrequency("", f));

    c.allow_dialed_frequency = false;
    fct_chk(!c.lookupFrequency("121500", f));

    fct_chk(SipDisGwConfig::parseFrequency(" 118.0083 MHz", f) && f == 118008300ULL);
    fct_chk(!SipDisGwConfig::parseFrequency("118 furlongs", f));
    fct_chk(!SipDisGwConfig::parseFrequency("-5", f));

    const char *bad[] = {
        "dis_mode=anycast\n",
        "dis_port=70000\n",
        "radio_entity_type=7.1.840\n",
        "radio_entity_type=7.300.840.1.0.0\n",
        "radio_x=fast\n",
        "ptt_mode=telepathy\n",
        "antenna_latitude=91\n",
        "allow_dialed_frequency=no\n",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
      AmConfigReader br;
      br.loadString(bad[i], strlen(bad[i]));
      SipDisGwConfig bc;
      fct_chk(!bc.load(br));
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network) {
    SimSocket sim;
    if (!sim.ok) {
      // BSD and macOS only configure 127.0.0.1 on the loopback interface
      fprintf(stderr, "test_sip_dis_gw/network: cannot bind 127.0.0.2, skipped\n");
    } else {
      DisGwConfig cfg;
      cfg.mode = DisGwConfig::UNICAST;
      cfg.address = "127.0.0.2";
      cfg.port = sim.port;
      cfg.entity = dis7::EntityId(4, 5, 6);
      cfg.jitter_prebuffer_ms = 20;
      cfg.publish_receiver = false;

      DisNetwork net(cfg);
      fct_req(net.init());
      net.start();

      // a new radio announces itself switched on
      std::shared_ptr<LocalRadio> radio = net.attach(118100000ULL);
      fct_req(radio.get() != NULL);
      fct_chk_eq_int(radio->radioId().radio, 1);

      dis7::TransmitterPdu t;
      fct_chk(sim.receiveTransmitter(t));
      fct_chk_eq_int(t.header.version, 7);
      fct_chk_eq_int(t.transmit_state, dis7::TX_ON_NOT_TRANSMITTING);
      fct_chk(t.radio == radio->radioId());
      fct_chk_eq_int(t.radio.entity.site, 4);
      fct_chk(t.frequency == 118100000ULL);

      std::shared_ptr<LocalRadio> other = net.attach(121500000ULL);
      fct_chk_eq_int(other->radioId().radio, 2);
      fct_chk(sim.receiveTransmitter(t));

      // keyed: Transmitter PDU first, then the audio
      std::vector<int16_t> tone(160, 1234);
      net.setTransmitting(*radio, true);
      net.sendAudio(*radio, &tone[0], tone.size());
      fct_chk(sim.receiveTransmitter(t));
      fct_chk_eq_int(t.transmit_state, dis7::TX_ON_TRANSMITTING);

      std::vector<uint8_t> b;
      dis7::SignalPdu s;
      fct_chk(sim.receive(dis7::PDU_SIGNAL, b) && dis7::decode(&b[0], b.size(), s));
      fct_chk(s.radio == radio->radioId());
      fct_chk_eq_int(s.encoding_scheme, dis7::ENC_MULAW8);
      fct_chk_eq_int(s.sample_rate, 8000);
      fct_chk_eq_int(s.samples, 160);
      std::vector<int16_t> pcm;
      fct_chk(disaudio::decodeSignalData(s.encoding_scheme, s.data, s.samples, pcm));
      fct_chk(pcm.size() == 160 && abs(pcm[80] - 1234) < 40);

      net.setTransmitting(*radio, false);
      fct_chk(sim.receiveTransmitter(t));
      fct_chk_eq_int(t.transmit_state, dis7::TX_ON_NOT_TRANSMITTING);

      // not keyed: no audio leaves
      net.sendAudio(*radio, &tone[0], tone.size());

      // a simulator transmitter on the first radio's frequency is heard there
      dis7::TransmitterPdu tx = sampleTransmitter();
      tx.header.exercise = cfg.exercise_id;
      tx.radio = dis7::RadioId(dis7::EntityId(9, 9, 9), 1);
      tx.frequency = 118100200ULL; // within the default tolerance
      dis7::encode(tx, b);
      sim.sendToGateway(b);
      usleep(50000);

      dis7::SignalPdu sig;
      sig.header.exercise = cfg.exercise_id;
      sig.radio = tx.radio;
      sig.encoding_scheme = dis7::ENC_PCM16_LE;
      sig.sample_rate = 8000;
      sig.samples = 160;
      std::vector<int16_t> sim_tone(160, 4000);
      disaudio::encodeSignalData(dis7::ENC_PCM16_LE, &sim_tone[0], 160, sig.data);
      dis7::encode(sig, b);
      sim.sendToGateway(b);
      sim.sendToGateway(b);
      fct_chk_eq_int(waitForAudio(*radio), 4000);

      // ... but not on the other frequency
      std::vector<int16_t> heard(160, 0);
      other->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      // signals of a transmitter without Transmitter PDU, of another exercise,
      // or carrying the gateway's own site and application, are dropped
      std::vector<uint8_t> unknown, foreign, own;
      sig.radio = dis7::RadioId(dis7::EntityId(9, 9, 9), 2);
      dis7::encode(sig, unknown);
      sig.radio = tx.radio;
      sig.header.exercise = cfg.exercise_id + 1;
      dis7::encode(sig, foreign);
      tx.header.exercise = sig.header.exercise = cfg.exercise_id;
      tx.radio = sig.radio = dis7::RadioId(dis7::EntityId(4, 5, 99), 1);
      dis7::encode(tx, own);
      sim.sendToGateway(own);
      dis7::encode(sig, own);
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(unknown);
        sim.sendToGateway(foreign);
        sim.sendToGateway(own);
      }
      usleep(200000);
      radio->readAudio(&heard[0], heard.size());
      radio->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      // two PDUs bundled in one datagram
      tx.radio = sig.radio = dis7::RadioId(dis7::EntityId(9, 9, 9), 3);
      dis7::encode(tx, b);
      std::vector<uint8_t> second;
      dis7::encode(sig, second);
      b.insert(b.end(), second.begin(), second.end());
      b.insert(b.end(), second.begin(), second.end());
      sim.sendToGateway(b);
      fct_chk_eq_int(waitForAudio(*radio), 4000);

      // local loop: another caller on the frequency hears this one, the
      // sender does not hear itself
      std::shared_ptr<LocalRadio> third = net.attach(118100000ULL);
      usleep(600000); // let the simulator's audio drain
      radio->readAudio(&heard[0], heard.size());
      third->readAudio(&heard[0], heard.size());
      net.setTransmitting(*radio, true);
      net.sendAudio(*radio, &tone[0], tone.size());
      third->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 1234);
      radio->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      // the network thread unkeys a radio whose audio stopped
      dis7::TransmitterPdu last;
      bool unkeyed = false;
      for (int i = 0; i < 20 && !unkeyed; i++) {
        if (sim.receiveTransmitter(last) && last.radio == radio->radioId() &&
            last.transmit_state == dis7::TX_ON_NOT_TRANSMITTING) {
          unkeyed = true;
        }
      }
      fct_chk(unkeyed);

      // switching off
      net.detach(radio);
      bool off = false;
      for (int i = 0; i < 20 && !off; i++) {
        if (sim.receiveTransmitter(last) && last.radio == radio->radioId() &&
            last.transmit_state == dis7::TX_OFF) {
          off = true;
        }
      }
      fct_chk(off);

      // a detached radio stays silent and its number is reused
      net.setTransmitting(*radio, true);
      std::shared_ptr<LocalRadio> reused = net.attach(127000000ULL);
      fct_chk_eq_int(reused->radioId().radio, 1);

      net.detach(other);
      net.detach(third);
      net.detach(reused);
      net.dispose();
      fct_chk(net.is_stopped());
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
