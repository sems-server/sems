#include "fct.h"

#include "log.h"

#include "AmRtpPacket.h"
#include "AmRtpStream.h"
#include "AmSdp.h"
#include "AmSession.h"

#include <atomic>
#include <memory>
#include <set>
#include <thread>
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

  using AmRtpStream::nextPacket;
  using AmRtpStream::recvDtmfPacket;

  // what the RTP receiver hands over for a packet with a 4 byte payload of
  // type pt at RTP timestamp ts
  void receive(unsigned char pt, unsigned int ts) {
    unsigned char raw[] = {0x80, pt, 0x00, 0x01, 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78, 0x01, 0x0a, 0x03, 0x20};
    for (unsigned int i = 0; i < 4; i++) {
      raw[4 + i] = (unsigned char)(ts >> (24 - 8 * i));
    }
    recvPacket(-1, raw, sizeof(raw));
  }

  // what AmRtpStream::receive() does with a packet from nextPacket() once it
  // is done with it
  void done(AmRtpPacket *p) { mem.freePacket(p); }

  unsigned int packetsInUse() const { return mem.usedCount(); }
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

// Only the RTP receiver thread takes slots out of the pool, in
// AmRtpStream::recvPacket(). It hands back itself every packet bufferPacket()
// does not keep, while the media processor hands back the ones it took out of
// the receive buffer in AmRtpStream::receive(). PoolExerciser models exactly
// that: one thread allocates and frees every other packet itself, and passes
// the rest through a small ring to a second thread that frees them.
//
// The ring holds fewer packets than the pool, so the pool is never exhausted:
// every allocation has to succeed, and once everything has been freed again
// the in-use counter has to be back at zero. With a plain counter the
// receiver's n_used++ and the media processor's n_used-- lost updates against
// each other, and the counter drifted away from the real number of used slots.
struct PoolExerciser {
  static const unsigned int RING_SIZE = 8;

  PacketMem mem;

  // single producer (the allocator), single consumer (the freer)
  AmRtpPacket *ring[RING_SIZE];
  std::atomic<unsigned int> ring_head; // written by the allocator only
  std::atomic<unsigned int> ring_tail; // written by the freer only
  std::atomic<bool> done;

  // holders of each slot: a slot handed out while still held means the same
  // AmRtpPacket went to two owners
  std::atomic<unsigned int> holders[MAX_PACKETS];
  std::atomic<unsigned int> handed_out_twice;

  unsigned int failed_allocations; // allocator thread only

  PoolExerciser() : ring_head(0), ring_tail(0), done(false), handed_out_twice(0), failed_allocations(0) {
    for (unsigned int i = 0; i < MAX_PACKETS; i++) {
      holders[i].store(0);
    }
  }

  void take(AmRtpPacket *p) {
    if (holders[p - mem.packets].fetch_add(1) != 0) {
      handed_out_twice.fetch_add(1);
    }
  }

  void release(AmRtpPacket *p) {
    holders[p - mem.packets].fetch_sub(1);
    mem.freePacket(p);
  }

  // the RTP receiver
  void allocate(unsigned int rounds) {
    for (unsigned int i = 0; i < rounds; i++) {
      AmRtpPacket *p = mem.newPacket();
      if (!p) {
        failed_allocations++;
        continue;
      }
      take(p);

      unsigned int head = ring_head.load(std::memory_order_relaxed);
      if ((i & 1) || head - ring_tail.load(std::memory_order_acquire) == RING_SIZE) {
        release(p); // dropped by the receiver itself
        continue;
      }

      ring[head % RING_SIZE] = p;
      ring_head.store(head + 1, std::memory_order_release);
    }
    done.store(true);
  }

  // the media processor
  void consume() {
    for (;;) {
      unsigned int tail = ring_tail.load(std::memory_order_relaxed);
      if (tail == ring_head.load(std::memory_order_acquire)) {
        if (done.load() && tail == ring_head.load(std::memory_order_acquire)) {
          return;
        }
        continue;
      }

      AmRtpPacket *p = ring[tail % RING_SIZE];
      ring_tail.store(tail + 1, std::memory_order_release);
      release(p);
    }
  }
};

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
    for (std::set<AmRtpPacket *>::iterator it = handed_out.begin(); it != handed_out.end(); ++it) {
      mem.freePacket(*it);
    }
    fct_chk_eq_int(mem.usedCount(), 0);
    fct_chk(mem.newPacket() != NULL);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(packet_pool_counts_concurrent_alloc_free) {
    PoolExerciser ex;
    std::thread media_processor(&PoolExerciser::consume, &ex);
    ex.allocate(500000);
    media_processor.join();

    fct_chk_eq_int(ex.failed_allocations, 0);
    fct_chk_eq_int(ex.handed_out_twice.load(), 0);
    fct_chk_eq_int(ex.mem.usedCount(), 0);

    // the whole pool is available again
    std::set<AmRtpPacket *> slots;
    for (unsigned int i = 0; i < MAX_PACKETS; i++) {
      AmRtpPacket *p = ex.mem.newPacket();
      fct_req(p != NULL);
      slots.insert(p);
    }
    fct_chk_eq_int((int)slots.size(), MAX_PACKETS);
    fct_chk_eq_int(ex.mem.usedCount(), MAX_PACKETS);
  }
  FCT_TEST_END();

  // resume() runs in the session's thread, e.g. when a re-INVITE re-initializes
  // the stream, while the media processor may be in AmRtpStream::receive()
  // reading a packet it took out with nextPacket(). It used to mark every slot
  // free with mem.clear(), so the receiver refilled that packet underneath the
  // media processor, which then freed the slot of a packet still buffered.
  FCT_TEST_BGN(resume_leaves_packet_in_use_alone) {
    DtmfSession s;
    std::unique_ptr<TestStream> stream(new TestStream(&s));

    stream->receive(0, 1000);
    AmRtpPacket *held = NULL;
    fct_req(stream->nextPacket(held) == 1 && held != NULL);
    fct_req(held->timestamp == 1000);

    stream->resume();

    // the receiver keeps going and fills the rest of the pool
    for (unsigned int i = 1; i < MAX_PACKETS; i++) {
      stream->receive(0, 1000 + i * 160);
    }
    fct_chk_eq_int(held->timestamp, 1000);
    fct_chk_eq_int(stream->packetsInUse(), MAX_PACKETS);

    stream->done(held);
    fct_chk_eq_int(stream->packetsInUse(), MAX_PACKETS - 1);
  }
  FCT_TEST_END();

  // The receiver's last-resort recovery when the pool is exhausted (issue #92)
  // runs while the media processor may hold a packet just as well. Telephone
  // events fill the pool here, as reuseBufferedPacket() cannot recycle them.
  FCT_TEST_BGN(out_of_buffers_recovery_leaves_packet_in_use_alone) {
    DtmfSession s;
    std::unique_ptr<TestStream> stream(new TestStream(&s));

    stream->receive(TE_PT, 1000);
    AmRtpPacket *held = NULL;
    fct_req(stream->nextPacket(held) == 1 && held != NULL);
    fct_req(held->timestamp == 1000);

    for (unsigned int i = 1; i < MAX_PACKETS; i++) {
      stream->receive(TE_PT, 1000 + i * 160);
    }
    fct_req(stream->packetsInUse() == MAX_PACKETS);

    // no free slot left: the queued events are dropped to make room
    stream->receive(0, 100000);
    fct_chk_eq_int(held->timestamp, 1000);
    fct_chk_eq_int(stream->packetsInUse(), 2);

    stream->done(held);
    fct_chk_eq_int(stream->packetsInUse(), 1);
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
