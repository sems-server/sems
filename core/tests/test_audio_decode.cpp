#include "fct.h"

#include "log.h"

#include "AmApi.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmThread.h"
#include "amci/amci.h"

#include <cstring>

// AmAudio::decode() hands a frame to the codec, which writes PCM16 into the
// AUDIO_BUFFER_SIZE bytes of samples.back_buffer() without being told that
// size. A frame that decodes to more than that has to be refused before the
// codec runs.
//
// The stand-in codecs count their decode() calls and refuse to write anything
// that would not fit, so a missing check fails the assertions instead of
// corrupting memory.

namespace {

// test-private codec ids, well clear of amci/codecs.h and test_amaudio.cpp
const int TST_CODEC_EXPANDING = 9101; // two samples per byte over all channels, like G.722
const int TST_CODEC_PLAIN = 9102;     // one sample per byte and no bytes2samples(), like G.711

const unsigned int MAX_SAMPLES = PCM16_B2S(AUDIO_BUFFER_SIZE);

int decode_calls = 0;

long tst_codec_init(const char *, const char **, amci_codec_fmt_info_t **) { return 0; }

unsigned int tst_expanding_b2s(long, unsigned int b) { return 2 * b; }
unsigned int tst_expanding_s2b(long, unsigned int s) { return s / 2; }

// writes nb_samples samples, the samples of all channels together
int write_pcm(unsigned char *out, unsigned long long nb_samples) {
  decode_calls++;
  if (nb_samples > MAX_SAMPLES) {
    return -1;
  }
  memset(out, 0x22, (size_t)(2 * nb_samples));
  return (int)(2 * nb_samples);
}

int tst_expanding_decode(unsigned char *out, unsigned char *, unsigned int size, unsigned int, unsigned int,
                         long) {
  return write_pcm(out, 2ULL * size);
}

int tst_plain_decode(unsigned char *out, unsigned char *, unsigned int size, unsigned int, unsigned int,
                     long) {
  return write_pcm(out, size);
}

// id, encode, decode, plc, init, destroy, bytes2samples, samples2bytes, negotiate_fmt
amci_codec_t tst_codec_expanding = {TST_CODEC_EXPANDING, NULL, tst_expanding_decode, NULL, tst_codec_init,
                                    NULL, tst_expanding_b2s, tst_expanding_s2b, NULL};
amci_codec_t tst_codec_plain = {TST_CODEC_PLAIN, NULL, tst_plain_decode, NULL, tst_codec_init,
                                NULL, NULL, NULL, NULL};

void register_test_codecs() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  AmPlugIn::instance()->addCodec(&tst_codec_expanding);
  AmPlugIn::instance()->addCodec(&tst_codec_plain);
}

// counts the warnings and errors logged while armed
class LogCounter : public AmLoggingFacility {
public:
  LogCounter() : AmLoggingFacility("test_audio_decode"), armed(false), warnings(0), errors(0) {}

  int onLoad() override { return 0; }

  void log(int level, pid_t, pthread_t, const char *, const char *, int, char *) override {
    AmLock lock(mut);
    if (!armed) {
      return;
    }
    if (level == L_WARN) {
      warnings++;
    } else if (level == L_ERR) {
      errors++;
    }
  }

  void arm() {
    AmLock lock(mut);
    armed = true;
    warnings = errors = 0;
  }

  // stops counting; @return the number of warnings logged since arm()
  int disarm(int *errors_out) {
    AmLock lock(mut);
    armed = false;
    *errors_out = errors;
    return warnings;
  }

private:
  AmMutex mut;
  bool armed;
  int warnings;
  int errors;
};

LogCounter *log_counter() {
  static LogCounter *counter = NULL;
  if (!counter) {
    counter = new LogCounter();
    register_log_hook(counter);
  }
  return counter;
}

class DecodeAudio : public AmAudio {
public:
  DecodeAudio(int codec_id, int channels) : AmAudio(new AmAudioFormat(codec_id, 8000)) {
    fmt->channels = channels;
  }

  using AmAudio::decode;

protected:
  int read(unsigned int, unsigned int size) override { return (int)size; }
  int write(unsigned int, unsigned int size) override { return (int)size; }
};

} // namespace

FCTMF_SUITE_BGN(test_audio_decode) {

  // fct_chk_eq_int() evaluates its arguments twice, so decode() is called on
  // its own line

  FCT_TEST_BGN(expanding_codec_frame_that_fits) {
    register_test_codecs();
    DecodeAudio a(TST_CODEC_EXPANDING, 1);
    decode_calls = 0;
    int ret = a.decode(MAX_SAMPLES / 2);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    fct_chk_eq_int(decode_calls, 1);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(expanding_codec_frame_that_does_not_fit) {
    register_test_codecs();
    // one byte too many, and the largest payload an RTP packet can carry
    const unsigned int sizes[] = {MAX_SAMPLES / 2 + 1, 4084};
    for (int i = 0; i < 2; i++) {
      DecodeAudio a(TST_CODEC_EXPANDING, 1);
      decode_calls = 0;
      int ret = a.decode(sizes[i]);
      fct_xchk(ret == -1 && decode_calls == 0, "%u bytes: ret=%d, codec called %d times", sizes[i], ret,
               decode_calls);
    }
  }
  FCT_TEST_END();

  // the codec writes the samples of both channels, so the limit is the same as
  // for mono
  FCT_TEST_BGN(stereo_frame_counts_all_channels) {
    register_test_codecs();
    DecodeAudio a(TST_CODEC_EXPANDING, 2);
    decode_calls = 0;
    int ret = a.decode(MAX_SAMPLES / 2 + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(decode_calls, 0);

    ret = a.decode(MAX_SAMPLES / 2);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    fct_chk_eq_int(decode_calls, 1);
  }
  FCT_TEST_END();

  // G.711 has no bytes2samples(): its frames are decoded without any warning
  FCT_TEST_BGN(codec_without_bytes2samples_decodes_quietly) {
    register_test_codecs();
    LogCounter *counter = log_counter();
    DecodeAudio a(TST_CODEC_PLAIN, 1);
    decode_calls = 0;

    counter->arm();
    int ret = a.decode(160);
    int errors = 0;
    int warnings = counter->disarm(&errors);

    fct_chk_eq_int(ret, 320);
    fct_chk_eq_int(decode_calls, 1);
    fct_chk_eq_int(warnings, 0);
    fct_chk_eq_int(errors, 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(codec_without_bytes2samples_frame_that_does_not_fit) {
    register_test_codecs();
    DecodeAudio a(TST_CODEC_PLAIN, 1);
    decode_calls = 0;
    int ret = a.decode(MAX_SAMPLES);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    fct_chk_eq_int(decode_calls, 1);

    ret = a.decode(MAX_SAMPLES + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(decode_calls, 1);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
