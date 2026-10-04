#include "fct.h"

#include "AmConfigReader.h"

#include "../../apps/sip_dis_gw/Dis7Pdu.h"
#include "../../apps/sip_dis_gw/DisAudio.h"
#include "../../apps/sip_dis_gw/DisNetwork.h"
#include "../../apps/sip_dis_gw/SipDisGwConfig.h"

#include "test_sip_dis_gw.h"

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

using namespace disgw_test;

namespace {

/** a free UDP port */
unsigned short freePort() {
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  bind(sd, (struct sockaddr *)&a, sizeof(a));
  socklen_t len = sizeof(a);
  getsockname(sd, (struct sockaddr *)&a, &len);
  close(sd);
  return ntohs(a.sin_port);
}

/** a socket joined to a multicast group on the loopback interface, or -1 */
int multicastMember(const char *group, unsigned short port) {
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef SO_REUSEPORT
  setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(group);
  mreq.imr_interface.s_addr = inet_addr("127.0.0.1");
  struct in_addr lo;
  lo.s_addr = inet_addr("127.0.0.1");
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  if (bind(sd, (struct sockaddr *)&a, sizeof(a)) < 0 ||
      setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0 ||
      setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &lo, sizeof(lo)) < 0 ||
      setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    close(sd);
    return -1;
  }
  return sd;
}

/** next PDU of the given type on a member socket */
bool receiveOn(int sd, uint8_t type, std::vector<uint8_t> &pdu) {
  uint64_t deadline = DisNetwork::nowMs() + 2000;
  while (DisNetwork::nowMs() < deadline) {
    pdu.resize(dis7::MAX_PDU_SIZE);
    ssize_t len = recv(sd, &pdu[0], pdu.size(), 0);
    if (len >= (ssize_t)dis7::HEADER_SIZE && pdu[2] == type) {
      pdu.resize(len);
      return true;
    }
  }
  return false;
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
    fct_chk_eq_int(c.dis.modulation.major_modulation, 3);
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
      {
        int16_t heard_v = waitForAudio(*radio);
        fct_chk_eq_int(heard_v, 4000);
      }

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
      {
        int16_t heard_v = waitForAudio(*radio);
        fct_chk_eq_int(heard_v, 4000);
      }

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
  // ---------------------------------------------------------------- Dis7Pdu

  FCT_TEST_BGN(pdu_defaults_and_radio_id) {
    dis7::PduHeader h;
    fct_chk_eq_int(h.version, 7);
    fct_chk_eq_int(h.exercise, 1);
    fct_chk(h.type == 0 && h.family == 0 && h.timestamp == 0 && h.length == 0 && h.status == 0);

    dis7::TransmitterPdu t;
    fct_chk_eq_int(t.transmit_state, dis7::TX_OFF);
    fct_chk(t.frequency == 0 && t.bandwidth == 0 && t.power == 0);
    fct_chk(t.antenna_location[0] == 0 && t.antenna_location[2] == 0);
    fct_chk(t.relative_antenna_location[1] == 0);
    fct_chk_eq_int(t.radio_type.kind, 7);
    fct_chk(t.modulation.spread_spectrum == 0 && t.modulation.major_modulation == 1 &&
            t.modulation.detail == 2 && t.modulation.radio_system == 1);

    dis7::SignalPdu s;
    fct_chk_eq_int(s.encoding_scheme, dis7::ENC_MULAW8);
    fct_chk_eq_int(s.sample_rate, 8000);
    fct_chk(s.data.empty() && s.samples == 0 && s.tdl_type == 0);

    dis7::ReceiverPdu r;
    fct_chk_eq_int(r.receiver_state, dis7::RX_OFF);

    dis7::EntityStatePdu e;
    fct_chk(e.force_id == 0 && e.appearance == 0 && e.capabilities == 0 && e.location[1] == 0);
    fct_chk(e.marking[0] == 0 && e.marking[10] == 0);

    dis7::RadioId a(dis7::EntityId(1, 2, 3), 4);
    fct_chk(a.key() == 0x0001000200030004ULL);
    fct_chk(a == dis7::RadioId(dis7::EntityId(1, 2, 3), 4));
    fct_chk(!(a == dis7::RadioId(dis7::EntityId(1, 2, 3), 5)));
    // ordering follows site, application, entity, radio
    fct_chk(a < dis7::RadioId(dis7::EntityId(1, 2, 3), 5));
    fct_chk(a < dis7::RadioId(dis7::EntityId(2, 0, 0), 0));
    fct_chk(!(dis7::RadioId(dis7::EntityId(2, 0, 0), 0) < a));
    fct_chk(dis7::RadioId().key() == 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(entity_state_layout_complete) {
    dis7::EntityStatePdu es;
    es.header.exercise = 5;
    es.entity = dis7::EntityId(1, 200, 9);
    es.force_id = 2;
    es.entity_type.kind = 1;
    es.entity_type.domain = 2;
    es.entity_type.country = 703;
    es.entity_type.category = 3;
    es.entity_type.subcategory = 4;
    es.entity_type.specific = 5;
    es.entity_type.extra = 6;
    es.location[0] = 1.5;
    es.location[1] = -2.5;
    es.location[2] = 3.25;
    es.appearance = 0xa1b2c3d4;
    es.capabilities = 0x01020304;
    memcpy(es.marking, "ABCDEFGHIJK", 11);

    std::vector<uint8_t> b;
    dis7::encode(es, b);
    fct_chk_eq_int(b.size(), 144);
    fct_chk_eq_int(be16(b, 8), 144);
    fct_chk_eq_int(b[1], 5);
    fct_chk_eq_int(be16(b, 12), 1);
    fct_chk_eq_int(be16(b, 16), 9);
    fct_chk_eq_int(b[18], 2); // force
    fct_chk_eq_int(b[19], 0); // variable parameter records
    // entity type, then the same as alternative entity type
    const uint8_t type[8] = {1, 2, 0x02, 0xbf, 3, 4, 5, 6};
    fct_chk(!memcmp(&b[20], type, 8));
    fct_chk(!memcmp(&b[28], type, 8));
    fct_chk(bef32(b, 36) == 0 && bef32(b, 44) == 0); // velocity
    fct_chk(bef64(b, 48) == 1.5);
    fct_chk(bef64(b, 56) == -2.5);
    fct_chk(bef64(b, 64) == 3.25);
    fct_chk(bef32(b, 72) == 0); // orientation
    fct_chk(be32(b, 84) == 0xa1b2c3d4);
    fct_chk_eq_int(b[88], 1);  // dead reckoning: static
    fct_chk_eq_int(b[128], 1); // ASCII marking
    fct_chk(!memcmp(&b[129], "ABCDEFGHIJK", 11));
    fct_chk(be32(b, 140) == 0x01020304);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(decode_header_and_short_pdus) {
    std::vector<uint8_t> b;
    dis7::encode(sampleTransmitter(), b);
    dis7::PduHeader h;

    fct_chk(!dis7::decodeHeader(NULL, 100, h));
    fct_chk(!dis7::decodeHeader(&b[0], 11, h));

    fct_chk(dis7::decodeHeader(&b[0], b.size(), h));
    fct_chk_eq_int(h.version, 7);
    fct_chk_eq_int(h.exercise, 3);
    fct_chk_eq_int(h.type, dis7::PDU_TRANSMITTER);
    fct_chk_eq_int(h.family, dis7::FAMILY_RADIO_COMMUNICATIONS);
    fct_chk(h.timestamp == 0x12345679);
    fct_chk_eq_int(h.length, 104);
    fct_chk_eq_int(h.status, 0);

    // versions 4 to 7 share the layout, others are refused
    b[0] = 4;
    fct_chk(dis7::decodeHeader(&b[0], b.size(), h));
    b[0] = 3;
    fct_chk(!dis7::decodeHeader(&b[0], b.size(), h));
    b[0] = 7;

    // a length shorter than a header or longer than the datagram
    b[8] = 0;
    b[9] = 11;
    fct_chk(!dis7::decodeHeader(&b[0], b.size(), h));
    b[9] = 105;
    fct_chk(!dis7::decodeHeader(&b[0], b.size(), h));

    // a Receiver PDU cut short
    dis7::ReceiverPdu rx;
    dis7::encode(rx, b);
    dis7::ReceiverPdu r;
    fct_chk(dis7::decode(&b[0], b.size(), r));
    b[9] = 35;
    fct_chk(!dis7::decode(&b[0], 35, r));

    // a Signal PDU without data, and one whose data is not whole bytes
    dis7::SignalPdu s;
    dis7::encode(s, b);
    fct_chk_eq_int(b.size(), 32);
    dis7::SignalPdu d;
    fct_chk(dis7::decode(&b[0], b.size(), d));
    fct_chk(d.data.empty() && d.data_length == 0);

    s.data.assign(2, 0x5a);
    dis7::encode(s, b);
    b[28] = 0;
    b[29] = 12; // 12 bits: two bytes, the second one partly used
    fct_chk(dis7::decode(&b[0], b.size(), d));
    fct_chk_eq_int(d.data.size(), 2);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(timestamp_bounds) {
    struct timespec ts;
    ts.tv_sec = 3 * 3600;
    ts.tv_nsec = 0;
    fct_chk(dis7::timestamp(ts, false) == 0);

    // the very end of an hour does not overflow into the absolute bit
    ts.tv_sec = 3 * 3600 + 3599;
    ts.tv_nsec = 999999999;
    uint32_t end = dis7::timestamp(ts, false);
    fct_chk_eq_int(end & 1, 0);
    fct_chk((end >> 1) <= 0x7fffffff && (end >> 1) > 0x7ff00000);

    // grows within the hour
    ts.tv_sec = 3600 + 10;
    ts.tv_nsec = 0;
    uint32_t a = dis7::timestamp(ts, true);
    ts.tv_nsec = 500000000;
    uint32_t b2 = dis7::timestamp(ts, true);
    fct_chk(b2 > a);
    // one second is 2^31 / 3600 units
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    fct_chk(((dis7::timestamp(ts, false) >> 1) - 596523U) <= 1U);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(ecef_axes) {
    double e[3];
    dis7::geodeticToEcef(0, 90, 0, e);
    fct_chk(fabs(e[0]) < 0.001 && fabs(e[1] - 6378137.0) < 0.001 && fabs(e[2]) < 0.001);
    dis7::geodeticToEcef(-90, 0, 0, e);
    fct_chk(fabs(e[2] + 6356752.314) < 0.01);
    // altitude goes along the normal: at the equator straight out
    dis7::geodeticToEcef(0, 0, 1000, e);
    fct_chk(fabs(e[0] - 6379137.0) < 0.001);
    dis7::geodeticToEcef(0, 180, 0, e);
    fct_chk(fabs(e[0] + 6378137.0) < 0.001);
  }
  FCT_TEST_END();

  // --------------------------------------------------------------- DisAudio

  FCT_TEST_BGN(audio_encoding_edge_cases) {
    const uint16_t ok[] = {1, 4, 5, 100};
    for (size_t i = 0; i < 4; i++) {
      fct_chk(disaudio::canDecode(ok[i]));
    }
    const uint16_t bad[] = {0, 2, 3, 6, 8, 9, 10, 255, 0x8001, 0xc004};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
      fct_chk(!disaudio::canDecode(bad[i]));
    }

    std::vector<uint8_t> data;
    std::vector<int16_t> out(5, 1);
    // no data: success, nothing decoded
    fct_chk(disaudio::decodeSignalData(dis7::ENC_MULAW8, data, 0, out));
    fct_chk(out.empty());
    // sample count 0 means "all the data"
    data.assign(7, 0xff);
    fct_chk(disaudio::decodeSignalData(dis7::ENC_MULAW8, data, 0, out));
    fct_chk_eq_int(out.size(), 7);
    // an odd byte at the end of 16-bit PCM is ignored
    fct_chk(disaudio::decodeSignalData(dis7::ENC_PCM16_BE, data, 0, out));
    fct_chk_eq_int(out.size(), 3);

    int16_t none = 0;
    fct_chk(disaudio::encodeSignalData(dis7::ENC_MULAW8, &none, 0, data));
    fct_chk(data.empty());

    // mu-law: symmetric, and close to the input
    bool sym = true, close = true;
    for (int v = 1; v < 32767; v += 37) {
      if (disaudio::linearToUlaw((int16_t)v) != (disaudio::linearToUlaw((int16_t)-v) ^ 0x80)) {
        sym = false;
      }
      int back = disaudio::ulawToLinear(disaudio::linearToUlaw((int16_t)v));
      if (abs(back - v) > v / 16 + 16 && v < 32124) {
        close = false;
      }
    }
    fct_chk(sym);
    fct_chk(close);
    fct_chk_eq_int(disaudio::ulawToLinear(disaudio::linearToUlaw(32767)), 32124); // clipped
  }
  FCT_TEST_END();

  FCT_TEST_BGN(resampler_edge_cases) {
    std::vector<int16_t> out(3, 1);
    disaudio::LinearResampler same(8000, 8000);
    fct_chk_eq_int(same.inputRate(), 8000);
    int16_t in[4] = {1, 2, 3, 4};
    same.process(in, 4, out);
    fct_chk(out.size() == 4 && out[3] == 4);
    same.process(in, 0, out);
    fct_chk(out.empty());

    // upsampling one sample at a time keeps the ramp continuous
    disaudio::LinearResampler up(8000, 16000);
    fct_chk_eq_int(up.inputRate(), 8000);
    std::vector<int16_t> all;
    for (int i = 0; i < 50; i++) {
      int16_t v = (int16_t)(i * 100);
      up.process(&v, 1, out);
      all.insert(all.end(), out.begin(), out.end());
    }
    fct_chk(all.size() >= 96 && all.size() <= 100);
    bool ramp = true;
    for (size_t i = 0; i < all.size(); i++) {
      if (all[i] != (int16_t)(i * 50)) {
        ramp = false;
      }
    }
    fct_chk(ramp);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(rx_mixer_edge_cases) {
    std::vector<int16_t> out(10, 7);
    disaudio::RxMixer empty(0, 100);
    empty.read(&out[0], out.size());
    fct_chk(out[0] == 0 && out[9] == 0);
    uint64_t active = 42;
    fct_chk(!empty.expire(1000, 10, &active));
    fct_chk(active == 42);

    // no prebuffer: plays at once; sums clip at the negative end too
    std::vector<int16_t> neg(10, -30000);
    empty.push(1, &neg[0], neg.size(), 0);
    empty.push(2, &neg[0], neg.size(), 0);
    empty.read(&out[0], out.size());
    fct_chk_eq_int(out[0], -32768);
    fct_chk_eq_int(out[9], -32768);

    // expire() reports a remaining source, NULL is allowed
    empty.push(3, &neg[0], 5, 100);
    fct_chk(empty.expire(100, 10, NULL));
    fct_chk(empty.expire(100, 10, &active));
    fct_chk(active == 3);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(vox_gate_edge_cases) {
    std::vector<int16_t> loud(160, 3000), quiet(160, 0);

    disaudio::VoxGate vox(-40, 0);
    fct_chk(!vox.isOpen());
    fct_chk(!vox.process(&loud[0], 0)); // nothing to look at
    fct_chk(vox.process(&loud[0], loud.size()));
    fct_chk(vox.isOpen());
    fct_chk(vox.process(&loud[0], 0));              // unchanged
    fct_chk(!vox.process(&quiet[0], quiet.size())); // no hang time
    fct_chk(!vox.isOpen());

    // a threshold of 0 dBFS needs full scale
    disaudio::VoxGate deaf(0, 1000);
    fct_chk(!deaf.process(&loud[0], loud.size()));
    std::vector<int16_t> full(160, 32767);
    fct_chk(!deaf.process(&full[0], full.size())); // 32767 < 32768
  }
  FCT_TEST_END();

  // --------------------------------------------------------- SipDisGwConfig

  FCT_TEST_BGN(config_defaults) {
    AmConfigReader r;
    SipDisGwConfig c;
    fct_chk(c.load(r));
    fct_chk(c.allow_dialed_frequency);
    fct_chk(c.ptt_mode == SipDisGwConfig::PTT_VOX);
    fct_chk(c.vox_threshold_dbfs == -40);
    fct_chk_eq_int(c.vox_hang_ms, 600);
    fct_chk_eq_int(c.vox_preroll_ms, 40);
    fct_chk_eq_int(c.ptt_max_ms, 60000);
    fct_chk(c.radios.empty());
    fct_chk(c.dis.mode == DisGwConfig::BROADCAST);
    fct_chk(c.dis.address == "255.255.255.255");

    // the default address follows the mode
    const char *mc = "dis_mode=multicast\n";
    AmConfigReader rm;
    rm.loadString(mc, strlen(mc));
    SipDisGwConfig cm;
    fct_chk(cm.load(rm));
    fct_chk(cm.dis.address == "239.1.2.3");
    const char *uc = "dis_mode=unicast\n";
    AmConfigReader ru;
    ru.loadString(uc, strlen(uc));
    SipDisGwConfig cu;
    fct_chk(cu.load(ru));
    fct_chk(cu.dis.address == "127.0.0.1");
  }
  FCT_TEST_END();

  FCT_TEST_BGN(config_every_setting) {
    const char *conf = "dis_mode=broadcast\n"
                       "dis_address= 192.168.1.255 \n"
                       "dis_port=3001\n"
                       "dis_interface=192.168.1.10\n"
                       "dis_multicast_ttl=3\n"
                       "dis_multicast_loop=off\n"
                       "dis_exercise_id=9\n"
                       "dis_site_id=11\n"
                       "dis_application_id=12\n"
                       "dis_entity_id=13\n"
                       "dis_absolute_timestamps=1\n"
                       "radio_entity_type=7:1:840:2:3:4\n"
                       "modulation=0:3:1:1\n"
                       "input_source=2\n"
                       "transmit_bandwidth=25kHz\n"
                       "transmit_power=37.5\n"
                       "frequency_tolerance=1kHz\n"
                       "antenna_latitude=0\n"
                       "antenna_longitude=90\n"
                       "antenna_altitude=0\n"
                       "encoding=pcm16le\n"
                       "signal_frame_ms=40\n"
                       "jitter_prebuffer_ms=100\n"
                       "jitter_max_ms=900\n"
                       "local_loop=no\n"
                       "transmitter_heartbeat_ms=1500\n"
                       "receiver_heartbeat_ms=2500\n"
                       "remote_transmitter_timeout_ms=7000\n"
                       "tx_idle_ms=800\n"
                       "publish_receiver=false\n"
                       "publish_entity_state=true\n"
                       "entity_heartbeat_ms=3000\n"
                       "entity_force_id=3\n"
                       "entity_type=1.1.840.4.5.6.7\n"
                       "entity_marking=TOWER\n"
                       "radio_tower=118100000\n"
                       "allow_dialed_frequency=off\n"
                       "ptt_mode=listen\n"
                       "vox_threshold_dbfs=-30.5\n"
                       "vox_hang_ms=900\n"
                       "vox_preroll_ms=80\n"
                       "ptt_max_ms=0\n";
    AmConfigReader r;
    r.loadString(conf, strlen(conf));
    SipDisGwConfig c;
    fct_req(c.load(r));
    const DisGwConfig &d = c.dis;
    fct_chk(d.mode == DisGwConfig::BROADCAST);
    fct_chk(d.address == "192.168.1.255"); // trimmed
    fct_chk_eq_int(d.port, 3001);
    fct_chk(d.interface_addr == "192.168.1.10");
    fct_chk_eq_int(d.multicast_ttl, 3);
    fct_chk(!d.multicast_loop);
    fct_chk_eq_int(d.exercise_id, 9);
    fct_chk(d.entity.site == 11 && d.entity.application == 12 && d.entity.entity == 13);
    fct_chk(d.absolute_timestamps);
    fct_chk(d.radio_type.kind == 7 && d.radio_type.domain == 1 && d.radio_type.country == 840 &&
            d.radio_type.category == 2 && d.radio_type.nomenclature_version == 3 &&
            d.radio_type.nomenclature == 4);
    fct_chk(d.modulation.major_modulation == 3 && d.modulation.detail == 1 && d.modulation.radio_system == 1);
    fct_chk_eq_int(d.input_source, 2);
    fct_chk(d.bandwidth == 25000.0f);
    fct_chk(d.power == 37.5f);
    fct_chk(d.frequency_tolerance == 1000);
    fct_chk(fabs(d.antenna_location[1] - 6378137.0) < 0.001);
    fct_chk_eq_int(d.encoding, dis7::ENC_PCM16_LE);
    fct_chk_eq_int(d.frame_ms, 40);
    fct_chk_eq_int(d.jitter_prebuffer_ms, 100);
    fct_chk_eq_int(d.jitter_max_ms, 900);
    fct_chk(!d.local_loop);
    fct_chk_eq_int(d.transmitter_heartbeat_ms, 1500);
    fct_chk_eq_int(d.receiver_heartbeat_ms, 2500);
    fct_chk_eq_int(d.remote_timeout_ms, 7000);
    fct_chk_eq_int(d.tx_idle_ms, 800);
    fct_chk(!d.publish_receiver);
    fct_chk(d.publish_entity_state);
    fct_chk_eq_int(d.entity_heartbeat_ms, 3000);
    fct_chk_eq_int(d.force_id, 3);
    fct_chk(d.entity_type.kind == 1 && d.entity_type.country == 840 && d.entity_type.category == 4 &&
            d.entity_type.subcategory == 5 && d.entity_type.specific == 6 && d.entity_type.extra == 7);
    fct_chk(d.entity_marking == "TOWER");
    fct_chk(c.radios.size() == 1 && c.radios["tower"] == 118100000ULL);
    fct_chk(!c.allow_dialed_frequency);
    fct_chk(c.ptt_mode == SipDisGwConfig::PTT_LISTEN);
    fct_chk(c.vox_threshold_dbfs == -30.5);
    fct_chk_eq_int(c.vox_hang_ms, 900);
    fct_chk_eq_int(c.vox_preroll_ms, 80);
    fct_chk_eq_int(c.ptt_max_ms, 0);

    // remaining values of the enumerated settings
    const char *more = "ptt_mode=always\nencoding=mulaw\nfrequency_tolerance=0\n";
    AmConfigReader r2;
    r2.loadString(more, strlen(more));
    SipDisGwConfig c2;
    fct_chk(c2.load(r2));
    fct_chk(c2.ptt_mode == SipDisGwConfig::PTT_ALWAYS);
    fct_chk_eq_int(c2.dis.encoding, dis7::ENC_MULAW8);
    fct_chk(c2.dis.frequency_tolerance == 0);
    const char *more2 = "ptt_mode=vox\nencoding=pcm16be\n";
    AmConfigReader r3;
    r3.loadString(more2, strlen(more2));
    SipDisGwConfig c3;
    fct_chk(c3.load(r3));
    fct_chk(c3.ptt_mode == SipDisGwConfig::PTT_VOX);
    fct_chk_eq_int(c3.dis.encoding, dis7::ENC_PCM16_BE);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(config_rejects_bad_values) {
    const char *bad[] = {
        "dis_multicast_ttl=256\n",
        "dis_exercise_id=0\n",
        "dis_site_id=65535\n",
        "dis_port=0\n",
        "dis_port=-1\n",
        "dis_port=3000x\n",
        "encoding=gsm\n",
        "signal_frame_ms=5\n",
        "jitter_max_ms=10\n",
        "vox_threshold_dbfs=5\n",
        "vox_threshold_dbfs=-100\n",
        "transmit_power=loud\n",
        "modulation=1.2.3\n",
        "entity_type=1.1.840.4.5.6\n",
        "antenna_longitude=181\n",
        "antenna_altitude=high\n",
        "frequency_tolerance=-1\n",
        "dis_absolute_timestamps=maybe\n",
        "transmit_bandwidth=wide\n",
        "radio_entity_type=7.1.70000.1.0.0\n",
        "input_source=256\n",
        "tx_idle_ms=50\n",
        // the prebuffer must fit under the latency cap (default 400 ms)
        "jitter_prebuffer_ms=500\n",
        "jitter_prebuffer_ms=1000\njitter_max_ms=20\n",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
      AmConfigReader br;
      br.loadString(bad[i], strlen(bad[i]));
      SipDisGwConfig bc;
      fct_xchk(!bc.load(br), "accepted: %s", bad[i]);
    }

    // a prebuffer as long as the cap is fine
    const char *equal = "jitter_prebuffer_ms=400\njitter_max_ms=400\n";
    AmConfigReader er;
    er.loadString(equal, strlen(equal));
    SipDisGwConfig ec;
    fct_chk(ec.load(er));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(config_frequencies) {
    uint64_t f = 0;
    fct_chk(SipDisGwConfig::parseFrequency("1", f) && f == 1);
    fct_chk(SipDisGwConfig::parseFrequency("118100000 Hz", f) && f == 118100000ULL);
    fct_chk(SipDisGwConfig::parseFrequency("118100khz", f) && f == 118100000ULL);
    fct_chk(SipDisGwConfig::parseFrequency("118.1MHZ", f) && f == 118100000ULL);
    fct_chk(SipDisGwConfig::parseFrequency("1.2GHz", f) && f == 1200000000ULL);
    fct_chk(SipDisGwConfig::parseFrequency(".5MHz", f) && f == 500000ULL);
    fct_chk(SipDisGwConfig::parseFrequency("118.00833MHz", f) && f == 118008330ULL);
    fct_chk(!SipDisGwConfig::parseFrequency("", f));
    fct_chk(!SipDisGwConfig::parseFrequency("0", f));
    fct_chk(!SipDisGwConfig::parseFrequency("2000GHz", f));
    fct_chk(!SipDisGwConfig::parseFrequency("MHz", f));
    fct_chk(!SipDisGwConfig::parseFrequency("12 34", f));

    // a named radio wins over a dialled frequency of the same digits
    const char *conf = "radio_121500=130MHz\n";
    AmConfigReader r;
    r.loadString(conf, strlen(conf));
    SipDisGwConfig c;
    fct_req(c.load(r));
    fct_chk(c.lookupFrequency("121500", f) && f == 130000000ULL);
    fct_chk(c.lookupFrequency("121600", f) && f == 121600000ULL);
    fct_chk(c.lookupFrequency("121.6", f) && f == 121600000ULL);
    fct_chk(!c.lookupFrequency("12a", f));
    fct_chk(!c.lookupFrequency(".", f));
  }
  FCT_TEST_END();

  // ------------------------------------------------------------- DisNetwork

  FCT_TEST_BGN(network_defaults_and_local_radio) {
    DisGwConfig d;
    fct_chk(d.mode == DisGwConfig::BROADCAST && d.address == "255.255.255.255" && d.port == 3000);
    fct_chk(d.multicast_ttl == 16 && d.multicast_loop);
    fct_chk(d.exercise_id == 1 && d.entity.site == 1 && d.entity.application == 200 && d.entity.entity == 1);
    fct_chk(!d.absolute_timestamps);
    fct_chk(d.bandwidth == 25000 && d.power == 40 && d.frequency_tolerance == 500);
    fct_chk(d.encoding == dis7::ENC_MULAW8 && d.frame_ms == 20);
    fct_chk(d.jitter_prebuffer_ms == 60 && d.jitter_max_ms == 400 && d.local_loop);
    fct_chk(d.transmitter_heartbeat_ms == 2000 && d.receiver_heartbeat_ms == 5000);
    fct_chk(d.remote_timeout_ms == 4800 && d.tx_idle_ms == 1000);
    fct_chk(d.publish_receiver && !d.publish_entity_state);
    fct_chk(d.entity_marking == "SEMS-GW");
    fct_chk(d.antenna_location[0] == 0);

    LocalRadio radio(dis7::RadioId(dis7::EntityId(1, 2, 3), 7), 121500000ULL, 0, 100);
    fct_chk(radio.radioId() == dis7::RadioId(dis7::EntityId(1, 2, 3), 7));
    fct_chk(radio.frequency() == 121500000ULL);
    std::vector<int16_t> out(20, 9);
    radio.readAudio(&out[0], out.size());
    fct_chk(out[0] == 0 && out[19] == 0);

    uint64_t t0 = DisNetwork::nowMs();
    usleep(20000);
    uint64_t t1 = DisNetwork::nowMs();
    fct_chk(t1 >= t0 + 15 && t1 < t0 + 1000);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network_init_failures) {
    DisGwConfig cfg;
    cfg.mode = DisGwConfig::UNICAST;
    cfg.port = 0; // any free port

    cfg.address = "not-an-address";
    DisNetwork a(cfg);
    fct_chk(!a.init());

    cfg.address = "127.0.0.1";
    cfg.interface_addr = "nowhere";
    DisNetwork b(cfg);
    fct_chk(!b.init());

    cfg.interface_addr = "";
    cfg.mode = DisGwConfig::MULTICAST;
    DisNetwork c(cfg);
    fct_chk(!c.init()); // 127.0.0.1 is no multicast group

    // dispose() of a network whose thread never ran returns at once
    a.dispose();
    fct_chk(a.is_stopped());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network_radio_numbers) {
    SimSocket sim;
    if (!sim.ok) {
      fprintf(stderr, "test_sip_dis_gw/network_radio_numbers: cannot bind 127.0.0.2, skipped\n");
    } else {
      DisGwConfig cfg = loopbackConfig(sim);
      cfg.publish_receiver = false;
      DisNetwork net(cfg);
      fct_req(net.init());

      std::shared_ptr<LocalRadio> r1 = net.attach(1000000);
      std::shared_ptr<LocalRadio> r2 = net.attach(1000000);
      std::shared_ptr<LocalRadio> r3 = net.attach(1000000);
      fct_chk(r1->radioId().radio == 1 && r2->radioId().radio == 2 && r3->radioId().radio == 3);
      fct_chk(r1->radioId().entity.site == 4 && r1->radioId().entity.entity == 6);

      // the lowest free number is reused
      net.detach(r2);
      std::shared_ptr<LocalRadio> r4 = net.attach(1000000);
      fct_chk_eq_int(r4->radioId().radio, 2);
      std::shared_ptr<LocalRadio> r5 = net.attach(1000000);
      fct_chk_eq_int(r5->radioId().radio, 4);

      // detaching twice, or nothing, is harmless; only one "off" goes out
      sim.drain();
      net.detach(r1);
      net.detach(r1);
      net.detach(std::shared_ptr<LocalRadio>());
      std::vector<std::vector<uint8_t>> tx = sim.collect(dis7::PDU_TRANSMITTER, 300);
      fct_chk_eq_int(tx.size(), 1);
      if (tx.size() == 1) {
        fct_chk_eq_int(tx[0][28], dis7::TX_OFF);
        fct_chk_eq_int(be16(tx[0], 18), 1);
      }

      // a detached radio does not transmit anymore
      net.setTransmitting(*r1, true);
      std::vector<int16_t> pcm(160, 100);
      net.sendAudio(*r1, &pcm[0], pcm.size());
      fct_chk(sim.collect(dis7::PDU_TRANSMITTER, 200).empty());

      // keying twice sends one PDU; empty audio sends nothing
      net.setTransmitting(*r3, true);
      net.setTransmitting(*r3, true);
      net.sendAudio(*r3, &pcm[0], 0);
      size_t keyed_pdus = sim.collect(dis7::PDU_TRANSMITTER, 200).size();
      fct_chk_eq_int(keyed_pdus, 1);
      fct_chk(sim.collect(dis7::PDU_SIGNAL, 100).empty());

      net.detach(r3);
      net.detach(r4);
      net.detach(r5);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network_heartbeats_and_entity_state) {
    SimSocket sim;
    if (!sim.ok) {
      fprintf(stderr,
              "test_sip_dis_gw/network_heartbeats_and_entity_state: cannot bind 127.0.0.2, skipped\n");
    } else {
      DisGwConfig cfg = loopbackConfig(sim);
      cfg.transmitter_heartbeat_ms = 200;
      cfg.receiver_heartbeat_ms = 300;
      cfg.publish_entity_state = true;
      cfg.entity_heartbeat_ms = 250;
      cfg.force_id = 2;
      cfg.entity_type.kind = 1;
      cfg.entity_type.country = 703;
      cfg.entity_marking = "LZIB-TWR-RADIO"; // longer than the 11 characters
      cfg.antenna_location[0] = 4000000;
      cfg.antenna_location[1] = 1200000;
      cfg.antenna_location[2] = 4700000;
      cfg.absolute_timestamps = true;
      DisNetwork net(cfg);
      fct_req(net.init());
      net.start();

      std::shared_ptr<LocalRadio> radio = net.attach(118100000ULL);
      sim.drain();
      std::vector<std::vector<uint8_t>> tx = sim.collect(dis7::PDU_TRANSMITTER, 1100);
      fct_xchk(tx.size() >= 4 && tx.size() <= 7, "%d transmitter heartbeats", (int)tx.size());
      bool fields = !tx.empty();
      for (size_t i = 0; i < tx.size(); i++) {
        dis7::TransmitterPdu t;
        if (!dis7::decode(&tx[i][0], tx[i].size(), t) || t.transmit_state != dis7::TX_ON_NOT_TRANSMITTING ||
            t.frequency != 118100000ULL || t.antenna_location[1] != 1200000 || t.bandwidth != 25000 ||
            t.power != 40 || (t.header.timestamp & 1) != 1) {
          fields = false;
        }
      }
      fct_chk(fields);

      std::vector<std::vector<uint8_t>> rx = sim.collect(dis7::PDU_RECEIVER, 1000);
      fct_xchk(rx.size() >= 2 && rx.size() <= 5, "%d receiver heartbeats", (int)rx.size());
      if (!rx.empty()) {
        fct_chk_eq_int(be16(rx[0], 20), dis7::RX_ON_NOT_RECEIVING);
      }

      std::vector<uint8_t> es;
      fct_req(sim.receive(dis7::PDU_ENTITY_STATE, es));
      fct_chk_eq_int(es.size(), 144);
      fct_chk_eq_int(es[3], dis7::FAMILY_ENTITY_INFORMATION);
      fct_chk(be16(es, 12) == 4 && be16(es, 14) == 5 && be16(es, 16) == 6);
      fct_chk_eq_int(es[18], 2);
      fct_chk(es[20] == 1 && be16(es, 22) == 703);
      fct_chk(bef64(es, 48) == 4000000 && bef64(es, 56) == 1200000 && bef64(es, 64) == 4700000);
      fct_chk(!memcmp(&es[129], "LZIB-TWR-RA", 11));

      net.detach(radio);
      net.dispose();
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network_receiver_state_and_remote_expiry) {
    SimSocket sim;
    if (!sim.ok) {
      fprintf(stderr,
              "test_sip_dis_gw/network_receiver_state_and_remote_expiry: cannot bind 127.0.0.2, skipped\n");
    } else {
      DisGwConfig cfg = loopbackConfig(sim);
      cfg.remote_timeout_ms = 400;
      DisNetwork net(cfg);
      fct_req(net.init());
      net.start();
      std::shared_ptr<LocalRadio> radio = net.attach(118100000ULL);
      std::vector<int16_t> tone(160, 3000);

      // receiving: the Receiver PDU names the transmitter heard
      sim.sendToGateway(remoteTransmitter(1, 118100000ULL, dis7::TX_ON_TRANSMITTING));
      usleep(50000);
      sim.drain();
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, tone));
      }
      {
        int16_t heard_v = waitForAudio(*radio);
        fct_chk_eq_int(heard_v, 3000);
      }
      std::vector<uint8_t> rx;
      bool receiving = false;
      for (int i = 0; i < 10 && !receiving; i++) {
        if (sim.receive(dis7::PDU_RECEIVER, rx, 500) && be16(rx, 20) == dis7::RX_ON_RECEIVING) {
          receiving = true;
        }
      }
      fct_req(receiving);
      dis7::ReceiverPdu r;
      fct_chk(dis7::decode(&rx[0], rx.size(), r));
      fct_chk(r.radio == radio->radioId());
      fct_chk(r.transmitter == dis7::RadioId(dis7::EntityId(9, 9, 9), 1));

      // ... and goes back to "not receiving" when the signal stops
      bool idle = false;
      for (int i = 0; i < 10 && !idle; i++) {
        radio->readAudio(&tone[0], tone.size());
        if (sim.receive(dis7::PDU_RECEIVER, rx, 300) && be16(rx, 20) == dis7::RX_ON_NOT_RECEIVING) {
          idle = true;
        }
      }
      fct_chk(idle);

      // a remote transmitter not heard from for the timeout is forgotten
      usleep(600000);
      std::vector<int16_t> heard(160, 0);
      radio->readAudio(&heard[0], heard.size());
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(160, 2000)));
      }
      usleep(150000);
      radio->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      // switching off forgets it at once
      sim.sendToGateway(remoteTransmitter(1, 118100000ULL, dis7::TX_ON_TRANSMITTING));
      usleep(30000);
      sim.sendToGateway(remoteTransmitter(1, 118100000ULL, dis7::TX_OFF));
      usleep(30000);
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(160, 2000)));
      }
      usleep(150000);
      radio->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      net.detach(radio);
      net.dispose();
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network_signal_formats_and_matching) {
    SimSocket sim;
    if (!sim.ok) {
      fprintf(stderr,
              "test_sip_dis_gw/network_signal_formats_and_matching: cannot bind 127.0.0.2, skipped\n");
    } else {
      DisGwConfig cfg = loopbackConfig(sim);
      cfg.local_loop = false;
      cfg.encoding = dis7::ENC_PCM16_LE;
      cfg.frequency_tolerance = 500;
      DisNetwork net(cfg);
      fct_req(net.init());
      net.start();
      std::shared_ptr<LocalRadio> radio = net.attach(118100000ULL);
      std::shared_ptr<LocalRadio> other = net.attach(118100000ULL);
      std::vector<int16_t> heard(160, 0);

      // 501 Hz off: outside the tolerance
      sim.sendToGateway(remoteTransmitter(1, 118100501ULL, dis7::TX_ON_TRANSMITTING));
      usleep(30000);
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(160, 1000)));
      }
      usleep(150000);
      radio->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      // 16 kHz audio is resampled to 8 kHz
      sim.sendToGateway(remoteTransmitter(2, 118100000ULL, dis7::TX_ON_TRANSMITTING));
      usleep(30000);
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(2, std::vector<int16_t>(320, 1500), 16000));
      }
      {
        int16_t heard_v = waitForAudio(*radio);
        fct_chk_eq_int(heard_v, 1500);
      }
      usleep(100000);
      drainAudio(*radio);
      drainAudio(*other);

      // CVSD and a sample rate of 0 are ignored
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(2, std::vector<int16_t>(160, 1500), 8000, 2));
        sim.sendToGateway(remoteSignal(2, std::vector<int16_t>(160, 1500), 0));
      }
      usleep(150000);
      radio->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);

      // mu-law
      dis7::SignalPdu ul;
      ul.header.exercise = 1;
      ul.radio = dis7::RadioId(dis7::EntityId(9, 9, 9), 2);
      ul.samples = 160;
      ul.data.assign(160, disaudio::linearToUlaw(1000));
      std::vector<uint8_t> b;
      dis7::encode(ul, b);
      sim.sendToGateway(b);
      sim.sendToGateway(b);
      {
        int16_t heard_v = waitForAudio(*radio);
        fct_chk_eq_int(heard_v, disaudio::ulawToLinear(disaudio::linearToUlaw(1000)));
      }

      // no local loop: the other caller on the frequency hears nothing of
      // this one, and what goes out is 16-bit PCM little endian
      usleep(100000);
      drainAudio(*radio);
      drainAudio(*other);
      sim.drain();
      net.setTransmitting(*radio, true);
      std::vector<int16_t> pcm(160, 0x1234);
      net.sendAudio(*radio, &pcm[0], pcm.size());
      other->readAudio(&heard[0], heard.size());
      fct_chk_eq_int(heard[0], 0);
      std::vector<uint8_t> sig;
      fct_req(sim.receive(dis7::PDU_SIGNAL, sig));
      dis7::SignalPdu s;
      fct_chk(dis7::decode(&sig[0], sig.size(), s));
      fct_chk_eq_int(s.encoding_scheme, dis7::ENC_PCM16_LE);
      fct_chk(s.data.size() == 320 && s.data[0] == 0x34 && s.data[1] == 0x12);

      net.detach(radio);
      net.detach(other);
      net.dispose();
    }
  }
  FCT_TEST_END();
  FCT_TEST_BGN(network_multicast) {
    const char *group = "239.255.42.17";
    unsigned short port = freePort();
    int member = multicastMember(group, port);
    if (member < 0) {
      fprintf(stderr, "test_sip_dis_gw/network_multicast: no multicast on loopback, skipped\n");
    } else {
      DisGwConfig cfg;
      cfg.mode = DisGwConfig::MULTICAST;
      cfg.address = group;
      cfg.port = port;
      cfg.interface_addr = "127.0.0.1";
      cfg.multicast_ttl = 0;
      cfg.multicast_loop = true;
      cfg.jitter_prebuffer_ms = 20;
      DisNetwork net(cfg);
      fct_req(net.init());
      net.start();

      // the gateway's PDUs reach the group
      std::shared_ptr<LocalRadio> radio = net.attach(118100000ULL);
      std::vector<uint8_t> pdu;
      fct_chk(receiveOn(member, dis7::PDU_TRANSMITTER, pdu));

      // and the gateway hears the group (its own PDUs, looped back, are
      // ignored)
      struct sockaddr_in to;
      memset(&to, 0, sizeof(to));
      to.sin_family = AF_INET;
      to.sin_port = htons(port);
      to.sin_addr.s_addr = inet_addr(group);
      std::vector<uint8_t> tx = remoteTransmitter(1, 118100000ULL, dis7::TX_ON_TRANSMITTING);
      sendto(member, &tx[0], tx.size(), 0, (struct sockaddr *)&to, sizeof(to));
      usleep(30000);
      std::vector<uint8_t> sig = remoteSignal(1, std::vector<int16_t>(160, 2500));
      for (int i = 0; i < 3; i++) {
        sendto(member, &sig[0], sig.size(), 0, (struct sockaddr *)&to, sizeof(to));
      }
      int16_t heard = waitForAudio(*radio);
      fct_chk_eq_int(heard, 2500);

      net.detach(radio);
      net.dispose();
      close(member);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(network_ignores_junk_and_unheard_signals) {
    SimSocket sim;
    if (!sim.ok) {
      fprintf(stderr,
              "test_sip_dis_gw/network_ignores_junk_and_unheard_signals: cannot bind 127.0.0.2, skipped\n");
    } else {
      DisGwConfig cfg = loopbackConfig(sim);
      cfg.publish_receiver = false;
      DisNetwork net(cfg);
      fct_req(net.init());
      net.start();
      std::shared_ptr<LocalRadio> radio = net.attach(118100000ULL);

      // garbage, a short datagram, and a signal on a frequency no call uses
      sim.sendToGateway(std::vector<uint8_t>(40, 0xff));
      sim.sendToGateway(std::vector<uint8_t>(5, 7));
      sim.sendToGateway(remoteTransmitter(2, 300000000ULL, dis7::TX_ON_TRANSMITTING));
      usleep(30000);
      sim.sendToGateway(remoteSignal(2, std::vector<int16_t>(160, 900)));

      // still working afterwards
      sim.sendToGateway(remoteTransmitter(1, 118100000ULL, dis7::TX_ON_TRANSMITTING));
      usleep(30000);
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(160, 700)));
      }
      int16_t heard = waitForAudio(*radio);
      fct_chk_eq_int(heard, 700);

      // a full datagram claiming 1 Hz (8 minutes of audio, millions of
      // samples once resampled) is refused at once ...
      drainAudio(*radio);
      uint64_t before = DisNetwork::nowMs();
      for (int i = 0; i < 20; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(4000, 600), 1));
      }
      usleep(200000);
      std::vector<int16_t> silence(160, 1);
      radio->readAudio(&silence[0], silence.size());
      fct_chk_eq_int(silence[0], 0);
      // ... and the gateway stays responsive
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(160, 500)));
      }
      int16_t after_flood = waitForAudio(*radio);
      fct_chk_eq_int(after_flood, 500);
      fct_chk(DisNetwork::nowMs() - before < 2000);

      // a short PDU at a low but plausible rate is still played
      drainAudio(*radio);
      for (int i = 0; i < 3; i++) {
        sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(80, 400), 4000));
      }
      int16_t low_rate = waitForAudio(*radio);
      fct_chk_eq_int(low_rate, 400);
      drainAudio(*radio);

      // an encoding the gateway cannot produce sends nothing
      sim.drain();
      DisGwConfig odd = cfg;
      odd.encoding = 2; // CVSD
      DisNetwork net2(odd);
      std::shared_ptr<LocalRadio> r2 = net2.attach(118100000ULL);
      net2.setTransmitting(*r2, true);
      std::vector<int16_t> pcm(160, 100);
      net2.sendAudio(*r2, &pcm[0], pcm.size());
      fct_chk(sim.collect(dis7::PDU_SIGNAL, 200).empty());

      net2.detach(r2);
      net.detach(radio);
      net.dispose();
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
