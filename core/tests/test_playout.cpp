#include "fct.h"

#include "log.h"

#include "AmPlayoutBuffer.h"
#include "AmRtpAudio.h"
#include "SampleArray.h"

#include <cstring>

// AmPlayoutBuffer::write() resyncs by moving r_ts/w_ts back onto the local
// reference clock. The SampleArray behind it keeps its own write head
// (last_ts), which has to follow, otherwise SampleArray::put() drops every
// later packet as "too old" once the backwards jump exceeds SIZE_MIX_BUFFER
// and never recovers, because put() only ever raises last_ts.
//
// The checks below are on the samples read back, so a regression shows up as
// silence (or as data from the pre-resync timeline) rather than needing a
// sanitizer.

namespace {

const unsigned int PLAYOUT_RATE = 8000;
const unsigned int FRAME = 160;         // 20 ms at 8 kHz
const short MARK_PRE = 0x0101;          // written before the resync
const short MARK_POST = 0x4242;         // written after it

// The playout buffer only needs the PLC hooks; neither is reached by this
// test (no timestamp gap is ever presented to it), so they just record calls.
class NoPlc : public AmPLCBuffer {
public:
  unsigned int conceal_calls;

  NoPlc() : conceal_calls(0) {}

  void add_to_history(int16_t *, unsigned int) {}

  unsigned int conceal_loss(unsigned int, unsigned char *) {
    conceal_calls++;
    return 0;
  }
};

void fill(short *buf, unsigned int len, short value) {
  for (unsigned int i = 0; i < len; i++)
    buf[i] = value;
}

bool all_equal(const short *buf, unsigned int len, short value) {
  for (unsigned int i = 0; i < len; i++) {
    if (buf[i] != value)
      return false;
  }
  return true;
}

} // namespace

FCTMF_SUITE_BGN(test_playout) {

  // Feeding packets whose mapped RTP timestamp stays put while samples keep
  // arriving advances w_ts (one frame per packet) past the reference clock.
  // A following resync snaps w_ts back by more than SIZE_MIX_BUFFER, which is
  // exactly the case SampleArray::put() treats as a stale packet.
  FCT_TEST_BGN(resync_keeps_accepting_packets) {
    NoPlc plc;
    AmPlayoutBuffer pb(&plc, PLAYOUT_RATE);

    short in[FRAME];
    fill(in, FRAME, MARK_PRE);

    // ref_ts and rtp_ts both held at 0: mapped_ts == ref_ts, so no resync,
    // while w_ts and the sample array's last_ts advance by FRAME per packet.
    const unsigned int frames = (SIZE_MIX_BUFFER / FRAME) + 20; // > SIZE_MIX_BUFFER samples
    for (unsigned int i = 0; i < frames; i++)
      pb.write(0, 0, in, FRAME, false);

    // Now a forward jump of the remote timestamp of more than MAX_DELAY
    // (one second of samples) forces the resync.
    fill(in, FRAME, MARK_POST);
    pb.write(0, PLAYOUT_RATE * 3, in, FRAME, false);

    short out[FRAME];
    memset(out, 0, sizeof(out));
    u_int32_t got = pb.read(0, out, FRAME);

    fct_chk_eq_int((int)got, (int)FRAME);
    // the post-resync frame has to come back, not silence and not the
    // samples of the old timeline
    fct_chk(all_equal(out, FRAME, MARK_POST));
    fct_chk(!all_equal(out, FRAME, 0));
    fct_chk(!all_equal(out, FRAME, MARK_PRE));
  }
  FCT_TEST_END();

  // A resync that moves the write pointer forward must keep working too: the
  // sample array has to drop the stale window instead of mixing it in.
  FCT_TEST_BGN(forward_resync_keeps_accepting_packets) {
    NoPlc plc;
    AmPlayoutBuffer pb(&plc, PLAYOUT_RATE);

    short in[FRAME];
    fill(in, FRAME, MARK_PRE);
    pb.write(PLAYOUT_RATE * 4, PLAYOUT_RATE * 4, in, FRAME, false);

    // mapped_ts now more than MAX_DELAY/2 behind ref_ts
    fill(in, FRAME, MARK_POST);
    const unsigned int ref = PLAYOUT_RATE * 4 + PLAYOUT_RATE * 3;
    pb.write(ref, PLAYOUT_RATE * 4 + FRAME, in, FRAME, false);

    short out[FRAME];
    memset(out, 0, sizeof(out));
    u_int32_t got = pb.read(ref, out, FRAME);

    fct_chk_eq_int((int)got, (int)FRAME);
    fct_chk(all_equal(out, FRAME, MARK_POST));
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
