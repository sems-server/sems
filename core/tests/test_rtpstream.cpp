#include "fct.h"

#include "log.h"

#include "AmRtpPacket.h"
#include "AmRtpStream.h"
#include "AmSdp.h"
#include "AmSession.h"
#include "AmConfig.h"

#include <memory>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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

// A stream whose RTCP socket is a plain UDP socket the test writes to, so
// recvRtcpPacket() can be driven without a negotiated session. It also lets
// the test age the last-RTP-received timestamp that the dead_rtp_time check
// in nextPacket() looks at.
class RtcpStream : public AmRtpStream {
public:
  explicit RtcpStream(AmSession *s) : AmRtpStream(s, 0) {}

  ~RtcpStream() {
    if (l_rtcp_sd > 0) {
      close(l_rtcp_sd);
      l_rtcp_sd = 0;
    }
  }

  // binds the RTCP socket to the loopback interface and returns the address
  // to send reports to
  bool openRtcpSocket(sockaddr_in &to) {
    l_rtcp_sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (l_rtcp_sd < 0) return false;

    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0; // any free port
    if (bind(l_rtcp_sd, (sockaddr *)&sa, sizeof(sa)) < 0) return false;

    socklen_t len = sizeof(to);
    return getsockname(l_rtcp_sd, (sockaddr *)&to, &len) == 0;
  }

  void ageLastRecvTime(unsigned int secs) {
    gettimeofday(&last_recv_time, NULL);
    last_recv_time.tv_sec -= secs;
  }

  using AmRtpStream::nextPacket;
};

// sends a minimal RTCP receiver report to addr and lets the stream pick it up
bool feed_rtcp(RtcpStream &stream, const sockaddr_in &addr) {
  const unsigned char rr[] = {
      0x80, 0xc9, 0x00, 0x01,  // V=2, PT=201 (RR), length
      0x12, 0x34, 0x56, 0x78   // SSRC of packet sender
  };
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) return false;
  bool sent = sendto(s, rr, sizeof(rr), 0, (const sockaddr *)&addr, sizeof(addr)) == (ssize_t)sizeof(rr);
  close(s);
  if (!sent) return false;
  stream.recvRtcpPacket();
  return true;
}

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

  FCT_TEST_BGN(rtcp_does_not_hold_off_the_rtp_timeout) {
    // A peer that keeps sending RTCP while its RTP has stopped must still run
    // into dead_rtp_time: RTCP has its own schedule and says nothing about the
    // media stream being alive.
    fct_req(AmConfig::DeadRtpTime > 0);

    AmSession s;
    RtcpStream stream(&s);
    sockaddr_in rtcp_addr;
    fct_req(stream.openRtcpSocket(rtcp_addr));

    stream.ageLastRecvTime(AmConfig::DeadRtpTime + 5);
    fct_req(feed_rtcp(stream, rtcp_addr));

    AmRtpPacket *p = NULL;
    fct_chk_eq_int(stream.nextPacket(p), RTP_TIMEOUT);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(received_rtp_does_hold_off_the_rtp_timeout) {
    // Counterpart: the RTP path is what keeps a stream alive.
    fct_req(AmConfig::DeadRtpTime > 0);

    AmSession s;
    RtcpStream stream(&s);
    stream.ageLastRecvTime(AmConfig::DeadRtpTime + 5);
    stream.clearRTPTimeout();

    AmRtpPacket *p = NULL;
    fct_chk_eq_int(stream.nextPacket(p), RTP_EMPTY);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
