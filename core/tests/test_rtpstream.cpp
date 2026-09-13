#include "fct.h"

#include "log.h"

#include "AmRtpPacket.h"
#include "AmRtpStream.h"
#include "AmSdp.h"
#include "AmSession.h"

#include <memory>
#include <vector>

// AmRtpStream::recvDtmfPacket() decodes an RFC 4733 telephone-event payload as
// a 4 byte dtmf_payload_t. A packet with a shorter payload has to be ignored:
// the bytes it lacks are whatever an earlier packet left in the AmRtpPacket
// buffer, and they used to be decoded as part of the event.
//
// The packets go to a stream that has negotiated telephone-event, the session
// runs what the stream posts through its DTMF detector, and onDtmf() records
// the keys that come out. The detector reports a key once the next one arrives.

namespace {

const unsigned char TE_PT = 101;

class TestStream : public AmRtpStream {
public:
  explicit TestStream(AmSession *s) : AmRtpStream(s, 0) {
    local_telephone_event_pt.reset(new SdpPayload(TE_PT, "telephone-event", 8000, 0));
  }

  using AmRtpStream::recvDtmfPacket;
};

class DtmfSession : public AmSession {
public:
  std::vector<int> keys;

  void onDtmf(int event, int /* duration */) override { keys.push_back(event); }

  // runs the DTMF detector over the events posted so far, then delivers what
  // it reported to onDtmf()
  void deliverDtmf() {
    processDtmfEvents();
    processEvents();
  }
};

// fills p with a telephone-event packet for key at RTP timestamp ts that
// carries the first payload_size bytes of the 4 byte event; the bytes after
// them are left as they were in p's buffer
void make_packet(AmRtpPacket &p, unsigned char key, unsigned int ts, unsigned int payload_size) {
  unsigned char raw[] = {
      0x80, TE_PT, 0x00, 0x01, // V=2, PT, sequence number
      (unsigned char)(ts >> 24), (unsigned char)(ts >> 16), (unsigned char)(ts >> 8), (unsigned char)ts,
      0x12, 0x34, 0x56, 0x78, // SSRC
      key, 0x0a, 0x03, 0x20   // event, E=0 R=0 volume=10, duration=800
  };
  p.compile_raw(raw, 12 + payload_size);
  p.parse();
}

} // namespace

FCTMF_SUITE_BGN(test_rtpstream) {

  FCT_TEST_BGN(telephone_event_reaches_session) {
    DtmfSession s;
    std::unique_ptr<TestStream> stream(new TestStream(&s));
    std::unique_ptr<AmRtpPacket> p(new AmRtpPacket());

    make_packet(*p, 4, 1000, 4);
    stream->recvDtmfPacket(p.get());
    make_packet(*p, 5, 2000, 4);
    stream->recvDtmfPacket(p.get());

    s.deliverDtmf();
    fct_req(s.keys.size() == 1);
    fct_chk_eq_int(s.keys[0], 4);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(short_telephone_event_is_ignored) {
    for (unsigned int size = 0; size < 4; size++) {
      DtmfSession s;
      std::unique_ptr<TestStream> stream(new TestStream(&s));
      std::unique_ptr<AmRtpPacket> p(new AmRtpPacket());

      // a full key 4 event goes through the buffer first, so the bytes the
      // short packet lacks still spell that event
      make_packet(*p, 4, 1000, 4);
      make_packet(*p, 4, 1000, size);
      stream->recvDtmfPacket(p.get());
      make_packet(*p, 5, 2000, 4);
      stream->recvDtmfPacket(p.get());
      make_packet(*p, 6, 3000, 4);
      stream->recvDtmfPacket(p.get());

      // only key 5 is reported: key 6 is still pending, and key 4 was never
      // received in full
      s.deliverDtmf();
      fct_xchk(s.keys.size() == 1 && s.keys[0] == 5, "%u byte payload: %d keys reported, the first %d", size,
               (int)s.keys.size(), s.keys.empty() ? -1 : s.keys[0]);
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
