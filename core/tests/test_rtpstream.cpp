#include "fct.h"

#include "log.h"

#include "AmRtpPacket.h"
#include "AmRtpStream.h"
#include "AmSdp.h"
#include "AmSession.h"

#include <atomic>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <set>
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

// PacketMem hands slots out to AmRtpStream::recvPacket() in the RTP receiver
// thread and takes them back from both that thread and the media processor, so
// the exerciser below claims and releases slots from several threads at once.
// claims[] counts how many holders a slot has at any time: anything but 0 -> 1
// -> 0 means the same AmRtpPacket was handed out twice, which used to happen
// because the slot was claimed with a read followed by a separate write.
struct PoolExerciser {
  PacketMem mem;
  std::atomic<unsigned int> claims[MAX_PACKETS];
  std::atomic<unsigned int> violations;
  std::atomic<unsigned int> allocations;

  PoolExerciser() : violations(0), allocations(0) {
    for (unsigned int i = 0; i < MAX_PACKETS; i++)
      claims[i].store(0);
  }

  void run(unsigned int rounds) {
    for (unsigned int i = 0; i < rounds; i++) {
      AmRtpPacket *p = mem.newPacket();
      if (!p)
        continue; // pool momentarily exhausted by the other threads

      allocations.fetch_add(1);

      unsigned int idx = p - &mem.packets[0];
      if (claims[idx].fetch_add(1) != 0)
        violations.fetch_add(1);

      // hold the slot for a moment so an overlapping claim has time to show up
      sched_yield();

      if (claims[idx].fetch_sub(1) != 1)
        violations.fetch_add(1);

      mem.freePacket(p);
    }
  }
};

void *exercise_pool(void *arg) {
  static_cast<PoolExerciser *>(arg)->run(20000);
  return NULL;
}

} // namespace

FCTMF_SUITE_BGN(test_rtpstream) {

  FCT_TEST_BGN(packet_pool_hands_out_each_slot_once) {
    PacketMem mem;
    std::set<AmRtpPacket *> handed_out;

    for (unsigned int i = 0; i < MAX_PACKETS; i++) {
      AmRtpPacket *p = mem.newPacket();
      fct_req(p != NULL);
      fct_chk(handed_out.insert(p).second);
    }

    fct_chk_eq_int(mem.usedCount(), MAX_PACKETS);
    fct_chk(mem.newPacket() == NULL);

    // releasing one slot makes exactly one slot available again
    AmRtpPacket *first = *handed_out.begin();
    mem.freePacket(first);
    fct_chk_eq_int(mem.usedCount(), MAX_PACKETS - 1);
    fct_chk(mem.newPacket() == first);
    fct_chk(mem.newPacket() == NULL);

    // and the counter comes back to zero, so the pool stays usable
    for (std::set<AmRtpPacket *>::iterator it = handed_out.begin(); it != handed_out.end(); ++it)
      mem.freePacket(*it);
    fct_chk_eq_int(mem.usedCount(), 0);
    fct_chk(mem.newPacket() != NULL);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(packet_pool_is_safe_against_concurrent_alloc_free) {
    PoolExerciser ex;
    const unsigned int threads = 4;
    pthread_t tid[threads];
    unsigned int started = 0;

    for (unsigned int i = 0; i < threads; i++) {
      if (pthread_create(&tid[i], NULL, exercise_pool, &ex) == 0)
        started++;
      else
        break;
    }
    fct_req(started == threads);

    for (unsigned int i = 0; i < started; i++)
      pthread_join(tid[i], NULL);

    fct_xchk(ex.violations.load() == 0, "%u slots were handed out more than once (%u allocations)",
             ex.violations.load(), ex.allocations.load());

    // every claim was released again, so the counter must be back to zero and
    // the whole pool available: a counter that drifted up used to make
    // newPacket() fail for the rest of the stream's life
    fct_chk_eq_int(ex.mem.usedCount(), 0);
    for (unsigned int i = 0; i < MAX_PACKETS; i++)
      fct_req(ex.mem.newPacket() != NULL);
  }
  FCT_TEST_END();

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
