#include "fct.h"

#include "AmAudio.h"
#include "AmConfig.h"
#include "AmPlugIn.h"
#include "amci/codecs.h"
#include "AmSipMsg.h"

#include "../../apps/sip_dis_gw/SipDisGw.h"

#include "test_sip_dis_gw.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

// The SIP side of the SIP to DIS gateway (apps/sip_dis_gw/SipDisGw.cpp): the
// per-call audio bridge with its push-to-talk modes, the session and the
// factory, each against a fake simulator socket on the loopback interface.

using namespace disgw_test;

namespace {

/** a gateway network side for one test */
struct Gw {
  SimSocket sim;
  SipDisGwConfig cfg;
  std::shared_ptr<DisNetwork> net;

  Gw() {
    // the audio works in 16-bit PCM, a codec SEMS registers itself on start
    if (!AmPlugIn::instance()->codec(CODEC_PCM16)) {
      AmPlugIn::instance()->init();
    }
    cfg.dis = loopbackConfig(sim);
    cfg.dis.publish_receiver = false;
  }

  bool start() {
    if (!sim.ok) {
      return false;
    }
    net.reset(new DisNetwork(cfg.dis));
    if (!net->init()) {
      return false;
    }
    net->start();
    return true;
  }

  ~Gw() {
    if (net) {
      net->dispose();
    }
  }
};

/** what the caller says: n samples of one value, 8 kHz, as the session's
    media processing would hand it to the audio */
void say(AmAudio &audio, int16_t value, size_t n) {
  static unsigned long long ts = 0;
  std::vector<int16_t> pcm(n, value);
  audio.put(ts, (unsigned char *)&pcm[0], 8000, n * 2);
  ts += n;
}

/** PDUs as they arrive: type and bytes */
struct Arrival {
  uint8_t type;
  std::vector<uint8_t> pdu;
};

std::vector<Arrival> arrivals(SimSocket &sim, unsigned int wait_ms) {
  std::vector<Arrival> res;
  uint64_t deadline = DisNetwork::nowMs() + wait_ms;
  while (DisNetwork::nowMs() < deadline) {
    std::vector<uint8_t> pdu(dis7::MAX_PDU_SIZE);
    ssize_t len = recv(sim.sd, &pdu[0], pdu.size(), 0);
    if (len >= (ssize_t)dis7::HEADER_SIZE) {
      pdu.resize(len);
      Arrival a;
      a.type = pdu[2];
      a.pdu = pdu;
      res.push_back(a);
    }
  }
  return res;
}

/** transmit states and Signal PDU sample counts in arrival order:
    transmitter state n as "Tn", a Signal PDU of n samples as "Sn" */
std::string sequence(const std::vector<Arrival> &in) {
  std::string s;
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i].type == dis7::PDU_TRANSMITTER) {
      dis7::TransmitterPdu t;
      if (dis7::decode(&in[i].pdu[0], in[i].pdu.size(), t)) {
        s += "T" + std::to_string(t.transmit_state) + " ";
      }
    } else if (in[i].type == dis7::PDU_SIGNAL) {
      dis7::SignalPdu sig;
      if (dis7::decode(&in[i].pdu[0], in[i].pdu.size(), sig)) {
        s += "S" + std::to_string(sig.samples) + " ";
      }
    }
  }
  return s;
}

/** first decoded sample of a Signal PDU */
int16_t firstSample(const std::vector<uint8_t> &pdu) {
  dis7::SignalPdu s;
  std::vector<int16_t> pcm;
  if (!dis7::decode(&pdu[0], pdu.size(), s) ||
      !disaudio::decodeSignalData(s.encoding_scheme, s.data, s.samples, pcm) || pcm.empty()) {
    return 0;
  }
  return pcm[0];
}

/** a session whose audio the test can reach */
class TestSession : public SipDisGwSession {
public:
  TestSession(const SipDisGwConfig &cfg, const std::shared_ptr<DisNetwork> &net, const std::string &name,
              uint64_t frequency)
      : SipDisGwSession(cfg, net, name, frequency) {
    // keeps AmSession::onSessionStart() from handing the session to the
    // media processor: the test drives the audio itself
    setStopped();
  }

  DisRadioAudio *audio() { return dynamic_cast<DisRadioAudio *>(input); }
  bool sameInOut() { return input == output; }
};

void skipped(const char *test) {
  fprintf(stderr, "test_sip_dis_gw_app/%s: cannot bind 127.0.0.2, skipped\n", test);
}

} // namespace

FCTMF_SUITE_BGN(test_sip_dis_gw_app) {

  // ---------------------------------------------------------- DisRadioAudio

  FCT_TEST_BGN(audio_read_gives_received_audio_or_silence) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_LISTEN;
    if (!gw.start()) {
      skipped("audio_read_gives_received_audio_or_silence");
    } else {
      std::shared_ptr<LocalRadio> radio = gw.net->attach(118100000ULL);
      DisRadioAudio audio(gw.cfg, gw.net, radio);
      fct_chk_eq_int(audio.getSampleRate(), 8000);

      // nothing received: a full frame of silence, so that RTP keeps flowing
      unsigned char buf[AUDIO_BUFFER_SIZE];
      memset(buf, 0x55, sizeof(buf));
      int got_bytes = audio.get(0, buf, 8000, 160);
      fct_chk_eq_int(got_bytes, 320);
      fct_chk(((int16_t *)buf)[0] == 0 && ((int16_t *)buf)[159] == 0);

      gw.sim.sendToGateway(remoteTransmitter(1, 118100000ULL, dis7::TX_ON_TRANSMITTING));
      usleep(30000);
      for (int i = 0; i < 3; i++) {
        gw.sim.sendToGateway(remoteSignal(1, std::vector<int16_t>(160, 4000)));
      }
      bool heard = false;
      for (int i = 0; i < 100 && !heard; i++) {
        usleep(10000);
        audio.get(0, buf, 8000, 160);
        heard = ((int16_t *)buf)[0] == 4000;
      }
      fct_chk(heard);
      gw.net->detach(radio);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(audio_ptt_always_sends_whole_frames) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_ALWAYS;
    if (!gw.start()) {
      skipped("audio_ptt_always_sends_whole_frames");
    } else {
      std::shared_ptr<LocalRadio> radio = gw.net->attach(118100000ULL);
      DisRadioAudio audio(gw.cfg, gw.net, radio);
      gw.sim.drain();

      // 3 x 10 ms: keyed at once, one 20 ms frame out, 10 ms kept back
      for (int i = 0; i < 3; i++) {
        say(audio, 1000, 80);
      }
      std::vector<Arrival> got = arrivals(gw.sim, 300);
      {
        std::string seq_v = sequence(got);
        fct_chk_eq_str(seq_v.c_str(), "T2 S160 ");
      }
      if (got.size() == 2) {
        fct_chk(abs(firstSample(got[1].pdu) - 1000) < 40);
      }
      gw.net->detach(radio);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(audio_ptt_listen_never_transmits) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_LISTEN;
    if (!gw.start()) {
      skipped("audio_ptt_listen_never_transmits");
    } else {
      std::shared_ptr<LocalRadio> radio = gw.net->attach(118100000ULL);
      DisRadioAudio audio(gw.cfg, gw.net, radio);
      audio.setPtt(true); // no effect outside dtmf mode
      gw.sim.drain();
      for (int i = 0; i < 6; i++) {
        say(audio, 20000, 160);
      }
      {
        std::string seq_v = sequence(arrivals(gw.sim, 300));
        fct_chk_eq_str(seq_v.c_str(), "");
      }
      gw.net->detach(radio);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(audio_ptt_dtmf_keys_flushes_and_times_out) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_DTMF;
    gw.cfg.ptt_max_ms = 300;
    if (!gw.start()) {
      skipped("audio_ptt_dtmf_keys_flushes_and_times_out");
    } else {
      std::shared_ptr<LocalRadio> radio = gw.net->attach(118100000ULL);
      DisRadioAudio audio(gw.cfg, gw.net, radio);
      gw.sim.drain();

      // not keyed: nothing goes out
      say(audio, 1000, 160);
      {
        std::string seq_v = sequence(arrivals(gw.sim, 200));
        fct_chk_eq_str(seq_v.c_str(), "");
      }

      // keyed: transmitter first, then the audio; on release the partial
      // last frame goes out before the transmitter is switched back
      audio.setPtt(true);
      say(audio, 1000, 200);
      audio.setPtt(false);
      say(audio, 1000, 80);
      {
        std::string seq_v = sequence(arrivals(gw.sim, 300));
        fct_chk_eq_str(seq_v.c_str(), "T2 S160 S40 T1 ");
      }

      // a stuck key is released after ptt_max_ms
      audio.setPtt(true);
      say(audio, 1000, 160);
      usleep(400000);
      say(audio, 1000, 160);
      say(audio, 1000, 160);
      std::string seq = sequence(arrivals(gw.sim, 300));
      fct_xchk(seq.find("T2 ") == 0 && seq.find("T1 ") != std::string::npos, "sequence %s", seq.c_str());
      gw.net->detach(radio);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(audio_ptt_vox_keys_with_preroll_and_hangs) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_VOX;
    gw.cfg.vox_threshold_dbfs = -40;
    gw.cfg.vox_hang_ms = 100;
    gw.cfg.vox_preroll_ms = 40;
    if (!gw.start()) {
      skipped("audio_ptt_vox_keys_with_preroll_and_hangs");
    } else {
      std::shared_ptr<LocalRadio> radio = gw.net->attach(118100000ULL);
      DisRadioAudio audio(gw.cfg, gw.net, radio);
      gw.sim.drain();

      // quiet: not keyed
      for (int i = 0; i < 4; i++) {
        say(audio, 20, 160);
      }
      {
        std::string seq_v = sequence(arrivals(gw.sim, 200));
        fct_chk_eq_str(seq_v.c_str(), "");
      }

      // speech: keyed, and the 40 ms before it go out first
      say(audio, 3000, 160);
      std::vector<Arrival> got = arrivals(gw.sim, 300);
      {
        std::string seq_v = sequence(got);
        fct_chk_eq_str(seq_v.c_str(), "T2 S160 S160 S160 ");
      }
      if (got.size() == 4) {
        fct_chk(abs(firstSample(got[1].pdu) - 20) <= 4);
        fct_chk(abs(firstSample(got[2].pdu) - 20) <= 4);
        fct_chk(abs(firstSample(got[3].pdu) - 3000) < 100);
      }

      // quiet again: still sent during the 100 ms hang time, then unkeyed
      for (int i = 0; i < 7; i++) {
        say(audio, 20, 160);
      }
      std::string seq = sequence(arrivals(gw.sim, 300));
      fct_xchk(seq.find("S160 ") == 0 && seq.size() >= 3 && seq.substr(seq.size() - 3) == "T1 ",
               "sequence %s", seq.c_str());
      gw.net->detach(radio);
    }
  }
  FCT_TEST_END();

  // -------------------------------------------------------- SipDisGwSession

  FCT_TEST_BGN(session_start_dtmf_and_bye) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_DTMF;
    if (!gw.start()) {
      skipped("session_start_dtmf_and_bye");
    } else {
      TestSession s(gw.cfg, gw.net, "tower", 118100000ULL);
      fct_chk(s.audio() == NULL);
      s.onDtmf(10, 100); // before the start: ignored

      s.onSessionStart();
      fct_req(s.audio() != NULL);
      fct_chk(s.sameInOut());
      dis7::TransmitterPdu t;
      fct_chk(gw.sim.receiveTransmitter(t));
      fct_chk(t.radio.radio == 1 && t.frequency == 118100000ULL &&
              t.transmit_state == dis7::TX_ON_NOT_TRANSMITTING);

      // '*' keys, other digits do nothing, '#' unkeys
      s.onDtmf(10, 100);
      say(*s.audio(), 1000, 160);
      s.onDtmf(5, 100);
      say(*s.audio(), 1000, 160);
      s.onDtmf(11, 100);
      say(*s.audio(), 1000, 160);
      {
        std::string seq_v = sequence(arrivals(gw.sim, 300));
        fct_chk_eq_str(seq_v.c_str(), "T2 S160 S160 T1 ");
      }

      // BYE: radio off, audio detached, and only once
      s.onBye(AmSipRequest());
      fct_chk(s.audio() == NULL);
      fct_chk(gw.sim.waitTransmitter(1, dis7::TX_OFF));
      s.onBye(AmSipRequest());
      fct_chk(sequence(arrivals(gw.sim, 200)).find("T0") == std::string::npos);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(session_dtmf_ignored_outside_dtmf_mode) {
    Gw gw;
    gw.cfg.ptt_mode = SipDisGwConfig::PTT_VOX;
    if (!gw.start()) {
      skipped("session_dtmf_ignored_outside_dtmf_mode");
    } else {
      TestSession s(gw.cfg, gw.net, "tower", 118100000ULL);
      s.onSessionStart();
      fct_req(s.audio() != NULL);
      gw.sim.drain();
      s.onDtmf(10, 100);
      say(*s.audio(), 0, 160);
      {
        std::string seq_v = sequence(arrivals(gw.sim, 200));
        fct_chk_eq_str(seq_v.c_str(), "");
      }
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(session_releases_radio_on_every_ending) {
    Gw gw;
    if (!gw.start()) {
      skipped("session_releases_radio_on_every_ending");
    } else {
      {
        TestSession s(gw.cfg, gw.net, "a", 121500000ULL);
        s.onSessionStart();
        fct_chk(gw.sim.waitTransmitter(1, dis7::TX_ON_NOT_TRANSMITTING));
        s.onRtpTimeout();
        fct_chk(s.audio() == NULL);
        fct_chk(gw.sim.waitTransmitter(1, dis7::TX_OFF));
      }
      {
        TestSession s(gw.cfg, gw.net, "b", 121500000ULL);
        s.onSessionStart();
        fct_chk(gw.sim.waitTransmitter(1, dis7::TX_ON_NOT_TRANSMITTING));
        s.onSessionTimeout();
        fct_chk(gw.sim.waitTransmitter(1, dis7::TX_OFF));
      }
      {
        // ended without any of the callbacks: the destructor cleans up
        TestSession s(gw.cfg, gw.net, "c", 121500000ULL);
        s.onSessionStart();
        fct_chk(gw.sim.waitTransmitter(1, dis7::TX_ON_NOT_TRANSMITTING));
      }
      fct_chk(gw.sim.waitTransmitter(1, dis7::TX_OFF));
      {
        // never started: nothing to release
        TestSession s(gw.cfg, gw.net, "d", 121500000ULL);
      }
      fct_chk(sequence(arrivals(gw.sim, 200)).empty());
    }
  }
  FCT_TEST_END();

  // -------------------------------------------------------- SipDisGwFactory

  FCT_TEST_BGN(factory_load_and_invite) {
    SimSocket sim;
    if (!sim.ok) {
      skipped("factory_load_and_invite");
    } else {
      char dir[] = "/tmp/sems_dis_gw_XXXXXX";
      fct_req(mkdtemp(dir) != NULL);
      std::string conf = std::string(dir) + "/sip_dis_gw.conf";
      std::string saved = AmConfig::ModConfigPath;
      AmConfig::ModConfigPath = std::string(dir) + "/";

      {
        std::ofstream f(conf.c_str());
        f << "dis_mode=unicast\ndis_address=127.0.0.2\ndis_port=" << sim.port
          << "\nradio_tower=118.1MHz\nallow_dialed_frequency=yes\nptt_mode=dtmf\n";
      }
      {
        SipDisGwFactory factory("sip_dis_gw");
        int loaded = factory.onLoad();
        fct_chk_eq_int(loaded, 0);

        std::map<std::string, std::string> params;
        AmSipRequest req;
        req.user = "tower";
        AmSession *s = factory.onInvite(req, "sip_dis_gw", params);
        fct_chk(dynamic_cast<SipDisGwSession *>(s) != NULL);
        delete s;

        req.user = "121500";
        s = factory.onInvite(req, "sip_dis_gw", params);
        fct_chk(s != NULL);
        delete s;

        req.user = "nosuchradio";
        int code = 0;
        std::string reason;
        try {
          factory.onInvite(req, "sip_dis_gw", params);
        } catch (const AmSession::Exception &e) {
          code = e.code;
          reason = e.reason;
        }
        fct_chk_eq_int(code, 404);
        fct_chk_eq_str(reason.c_str(), "Unknown Radio");
      } // the factory stops its network thread here

      // an invalid configuration, or a network that cannot start, fails
      {
        std::ofstream f(conf.c_str());
        f << "dis_mode=anycast\n";
      }
      {
        SipDisGwFactory factory("sip_dis_gw");
        int loaded = factory.onLoad();
        fct_chk_eq_int(loaded, -1);
      }
      {
        std::ofstream f(conf.c_str());
        f << "dis_mode=unicast\ndis_address=not-an-address\ndis_port=" << sim.port << "\n";
      }
      {
        SipDisGwFactory factory("sip_dis_gw");
        int loaded = factory.onLoad();
        fct_chk_eq_int(loaded, -1);
      }

      AmConfig::ModConfigPath = saved;
      unlink(conf.c_str());
      rmdir(dir);
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
