#include "fct.h"

#include "log.h"

#include "AmAudio.h"
#include "AmAudioFile.h"
#include "AmConfig.h"
#include "AmPlugIn.h"
#include "AmUtils.h"
#include "amci/amci.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

#include <dirent.h>
#include <unistd.h>

// AmAudio::get() and AmAudioFile::read() must refuse requests that do not fit
// the AUDIO_BUFFER_SIZE halves of the 'samples' DblBuffer before read() or
// fread() is reached. The checks below are on return values and on whether the
// read was attempted, so that a regression fails the suite without relying on
// a sanitizer to spot the overflow.

namespace {

// test-private codec ids, well clear of amci/codecs.h
const int TST_CODEC_L16 = 9001;  // 2 bytes per sample, no decode step
const int TST_CODEC_G711 = 9002; // 1 byte per sample, decode doubles the size

const int MAX_SAMPLES = PCM16_B2S(AUDIO_BUFFER_SIZE);

long tst_codec_init(const char *, const char **, amci_codec_fmt_info_t **) { return 0; }

unsigned int tst_l16_b2s(long, unsigned int b) { return b / 2; }
unsigned int tst_l16_s2b(long, unsigned int s) { return s * 2; }
unsigned int tst_g711_b2s(long, unsigned int b) { return b; }
unsigned int tst_g711_s2b(long, unsigned int s) { return s; }

int tst_g711_decode(unsigned char *out, unsigned char *in, unsigned int size, unsigned int, unsigned int,
                    long) {
  for (unsigned int i = 0; i < size; i++) {
    out[2 * i] = in[i];
    out[2 * i + 1] = 0;
  }
  return (int)(size * 2);
}

// id, encode, decode, plc, init, destroy, bytes2samples, samples2bytes, negotiate_fmt
amci_codec_t tst_codec_l16 = {TST_CODEC_L16, NULL,        NULL,        NULL, tst_codec_init,
                              NULL,          tst_l16_b2s, tst_l16_s2b, NULL};
amci_codec_t tst_codec_g711 = {TST_CODEC_G711, NULL,         tst_g711_decode, NULL, tst_codec_init,
                               NULL,           tst_g711_b2s, tst_g711_s2b,    NULL};

// header-less file format; rate and channels come from the test
int tst_file_rate = 8000;
int tst_file_channels = 1;

int tst_file_open(FILE *, amci_file_desc_t *fd, int, long) {
  fd->rate = tst_file_rate;
  fd->channels = tst_file_channels;
  fd->data_size = -1;
  return 0;
}

int tst_file_close(FILE *, amci_file_desc_t *, int, long, amci_codec_t *) { return 0; }

// amci_inoutfmt_t has non-const char* members
char tst_fmt_name[] = "SemsTestAmAudioRaw";
char tst_fmt_ext[] = "semstestraw";
char tst_fmt_mime[] = "audio/x-sems-test-raw";

amci_subtype_t tst_subtypes[] = {
    {1, "l16", 8000, 1, TST_CODEC_L16}, {2, "g711", 8000, 1, TST_CODEC_G711}, {-1, NULL, -1, -1, -1}};

// name, ext, email_content_type, open, on_close, mem_open, mem_close, subtypes
amci_inoutfmt_t tst_fmt = {tst_fmt_name,   tst_fmt_ext, tst_fmt_mime, tst_file_open,
                           tst_file_close, NULL,        NULL,         tst_subtypes};

void register_test_formats() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  AmPlugIn::instance()->addCodec(&tst_codec_l16);
  AmPlugIn::instance()->addCodec(&tst_codec_g711);
  AmPlugIn::instance()->addFileFormat(&tst_fmt);
}

// Resampling is not what these tests are about, and the internal resampler
// reads sinc[i][256] (core/resample/resample.cpp), which aborts a UBSan
// build. With resampling unavailable get() returns the decoded frame as is.
struct NoResampling {
  AmAudio::ResamplingImplementationType saved;

  NoResampling() : saved(AmConfig::ResamplingImplementationType) {
    AmConfig::ResamplingImplementationType = AmAudio::UNAVAILABLE;
  }

  ~NoResampling() { AmConfig::ResamplingImplementationType = saved; }
};

// AmAudio whose read() records the request. A request that would not fit the
// sample buffer, encoded or decoded, is refused without touching the buffer,
// so code missing the bounds checks fails the assertions instead of
// corrupting memory.
class TripwireAudio : public AmAudio {
public:
  int reads;
  unsigned int last_size;

  TripwireAudio(int codec_id, unsigned int rate, int channels = 1)
      : AmAudio(new AmAudioFormat(codec_id, rate)), reads(0), last_size(0), codec_id(codec_id) {
    fmt->channels = channels;
  }

  void setInputRate(unsigned int rate) { fmt->setRate(rate); }

  int pull(int output_sample_rate, unsigned int nb_samples) {
    return AmAudio::get(0, buffer, output_sample_rate, nb_samples);
  }

protected:
  int read(unsigned int, unsigned int size) {
    reads++;
    last_size = size;
    unsigned long long decoded = codec_id == TST_CODEC_G711 ? 2ULL * size : size;
    if (size > AUDIO_BUFFER_SIZE || decoded > AUDIO_BUFFER_SIZE) {
      return 0;
    }
    memset((unsigned char *)samples, 0x11, size);
    return (int)size;
  }

  int write(unsigned int, unsigned int size) { return (int)size; }

private:
  int codec_id;
  unsigned char buffer[AUDIO_BUFFER_SIZE];
};

class TestFile : public AmAudioFile {
public:
  int rawRead(unsigned int size) { return read(0, size); }
  FILE *stream() { return fp; }
};

// number of descriptors this process holds, or -1 when /proc is unavailable
int count_open_fds() {
  DIR *d = opendir("/proc/self/fd");
  if (!d) {
    return -1;
  }
  int n = 0;
  while (readdir(d) != NULL) {
    n++;
  }
  closedir(d);
  return n;
}

// opens a header-less test file holding 'len' bytes of payload
TestFile *open_test_file(const char *subtype, int rate, int channels, unsigned int len) {
  register_test_formats();
  tst_file_rate = rate;
  tst_file_channels = channels;
  FILE *fp = tmpfile();
  if (!fp) {
    return NULL;
  }
  for (unsigned int i = 0; i < len; i++) {
    fputc(i & 0x7f, fp);
  }
  TestFile *f = new TestFile();
  // fpopen() takes over fp once the format is found, and tst_file_open() never
  // fails, so a failure here leaves fp to us
  if (f->fpopen(std::string("test.semstestraw|") + subtype, AmAudioFile::Read, fp) != 0) {
    delete f;
    fclose(fp);
    return NULL;
  }
  return f;
}

} // namespace

FCTMF_SUITE_BGN(test_amaudio) {

  // fct_chk_eq_int() evaluates its arguments twice, so calls under test are
  // made on their own line

  FCT_TEST_BGN(get_regular_frames) {
    register_test_formats();
    NoResampling no_resampling;
    const int rates[] = {8000, 16000, 32000, 44100, 48000};
    const int ptimes[] = {10, 20, 30, 60};
    const int codecs[] = {TST_CODEC_L16, TST_CODEC_G711};
    for (int c = 0; c < 2; c++) {
      for (int i = 0; i < 5; i++) {
        for (int o = 0; o < 5; o++) {
          for (int p = 0; p < 4; p++) {
            TripwireAudio a(codecs[c], rates[i]);
            int nb = rates[o] * ptimes[p] / 1000;
            int nb_in = nb * rates[i] / rates[o];
            int ret = a.pull(rates[o], nb);
            fct_chk_eq_int(ret, nb_in * 2);
            fct_chk_eq_int(a.reads, 1);
            fct_chk_eq_int((int)a.last_size, codecs[c] == TST_CODEC_L16 ? nb_in * 2 : nb_in);
          }
        }
      }
    }

    for (int i = 0; i < 5; i++) {
      TripwireAudio s(TST_CODEC_L16, rates[i], 2);
      int nb = rates[i] / 50;
      int ret = s.pull(rates[i], nb);
      fct_chk_eq_int(ret, nb * 2);
      fct_chk_eq_int((int)s.last_size, nb * 4);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(get_rejects_invalid_rates) {
    register_test_formats();
    int ret;
    TripwireAudio out(TST_CODEC_L16, 8000);
    ret = out.pull(0, 160);
    fct_chk_eq_int(ret, -1);
    ret = out.pull(-8000, 160);
    fct_chk_eq_int(ret, -1);
    // the scaled sample count truncates to 0, which no later check catches
    ret = out.pull(-16000, 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(out.reads, 0);

    TripwireAudio in(TST_CODEC_L16, 8000);
    in.setInputRate(0);
    ret = in.pull(8000, 160);
    fct_chk_eq_int(ret, -1);
    // getSampleRate() returns the unsigned rate as int
    in.setInputRate((unsigned int)-1);
    ret = in.pull(8000, 160);
    fct_chk_eq_int(ret, -1);
    in.setInputRate((unsigned int)-8000);
    ret = in.pull(8000, 160);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(in.reads, 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(get_bounds_scaled_sample_count) {
    register_test_formats();
    NoResampling no_resampling;
    int ret;
    TripwireAudio l16(TST_CODEC_L16, 8000);
    ret = l16.pull(8000, MAX_SAMPLES);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    fct_chk_eq_int((int)l16.last_size, AUDIO_BUFFER_SIZE);
    ret = l16.pull(8000, MAX_SAMPLES + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(l16.reads, 1);

    // the encoded frame would fit, the decoded one would not
    TripwireAudio g711(TST_CODEC_G711, 8000);
    ret = g711.pull(8000, MAX_SAMPLES);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    fct_chk_eq_int((int)g711.last_size, MAX_SAMPLES);
    ret = g711.pull(8000, MAX_SAMPLES + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(g711.reads, 1);

    // the bound applies to the sample count at the input rate
    TripwireAudio down(TST_CODEC_G711, 16000);
    ret = down.pull(8000, MAX_SAMPLES / 2);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    fct_chk_eq_int((int)down.last_size, MAX_SAMPLES);
    ret = down.pull(8000, MAX_SAMPLES / 2 + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(down.reads, 1);

    TripwireAudio skew(TST_CODEC_L16, 48000);
    ret = skew.pull(8000, 8000);
    fct_chk_eq_int(ret, -1);
    ret = skew.pull(8000, 100000);
    fct_chk_eq_int(ret, -1);
    // out of int range; unchecked, it wraps to a small byte count
    ret = skew.pull(8000, UINT_MAX);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(skew.reads, 0);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(get_bounds_encoded_size) {
    register_test_formats();
    NoResampling no_resampling;
    int ret;
    // stereo L16 is 4 bytes per sample: the sample count is within bounds,
    // the encoded frame is not
    TripwireAudio s(TST_CODEC_L16, 8000, 2);
    ret = s.pull(8000, MAX_SAMPLES / 2);
    fct_chk_eq_int(ret, MAX_SAMPLES);
    fct_chk_eq_int((int)s.last_size, AUDIO_BUFFER_SIZE);
    ret = s.pull(8000, MAX_SAMPLES / 2 + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk_eq_int(s.reads, 1);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(file_read_bounds_encoded_size) {
    TestFile *f = open_test_file("l16", 8000, 1, AUDIO_BUFFER_SIZE);
    fct_req(f != NULL);
    int ret = f->rawRead(AUDIO_BUFFER_SIZE);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    // at the end of the payload, so an fread() attempt would set EOF
    clearerr(f->stream());
    ret = f->rawRead(AUDIO_BUFFER_SIZE + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk(!feof(f->stream()));
    fct_chk(ftell(f->stream()) == AUDIO_BUFFER_SIZE);
    delete f;
  }
  FCT_TEST_END();

  FCT_TEST_BGN(file_read_bounds_decoded_size) {
    int ret;
    TestFile *m = open_test_file("g711", 8000, 1, AUDIO_BUFFER_SIZE * 2);
    fct_req(m != NULL);
    ret = m->rawRead(MAX_SAMPLES);
    fct_chk_eq_int(ret, MAX_SAMPLES);
    ret = m->rawRead(MAX_SAMPLES + 1);
    fct_chk_eq_int(ret, -1);
    fct_chk(ftell(m->stream()) == MAX_SAMPLES);
    delete m;

    TestFile *s = open_test_file("g711", 8000, 2, AUDIO_BUFFER_SIZE * 2);
    fct_req(s != NULL);
    ret = s->rawRead(MAX_SAMPLES);
    fct_chk_eq_int(ret, MAX_SAMPLES);
    ret = s->rawRead(MAX_SAMPLES + 2);
    fct_chk_eq_int(ret, -1);
    fct_chk(ftell(s->stream()) == MAX_SAMPLES);
    delete s;

    // stereo L16 decodes to its own size
    TestFile *l = open_test_file("l16", 8000, 2, AUDIO_BUFFER_SIZE);
    fct_req(l != NULL);
    ret = l->rawRead(AUDIO_BUFFER_SIZE);
    fct_chk_eq_int(ret, AUDIO_BUFFER_SIZE);
    delete l;
  }
  FCT_TEST_END();

  FCT_TEST_BGN(file_get_regular_frames) {
    const int rates[] = {8000, 16000, 32000, 44100, 48000};
    const char *subtypes[] = {"l16", "g711"};
    unsigned char buf[AUDIO_BUFFER_SIZE];
    for (int st = 0; st < 2; st++) {
      for (int ch = 1; ch <= 2; ch++) {
        for (int i = 0; i < 5; i++) {
          int nb = rates[i] / 50;
          int frame = nb * ch * (st == 0 ? 2 : 1);
          TestFile *f = open_test_file(subtypes[st], rates[i], ch, 3 * frame);
          fct_chk(f != NULL);
          if (!f) {
            continue;
          }
          for (int n = 0; n < 3; n++) {
            int ret = f->get(0, buf, rates[i], nb);
            fct_chk_eq_int(ret, nb * 2);
          }
          fct_chk(ftell(f->stream()) == 3 * frame);
          delete f;
        }
      }
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(open_unknown_extension_keeps_no_descriptor) {
    register_test_formats();

    // open() opens the file itself, so nobody but AmAudioFile can close it
    // again when the format cannot be determined
    std::string path = "/tmp/sems_test_amaudio_" + int2str((unsigned int)getpid()) + ".semstestnofmt";
    FILE *seed = fopen(path.c_str(), "w");
    fct_req(seed != NULL);
    fputc('x', seed);
    fclose(seed);

    int before = count_open_fds();
    for (int i = 0; i < 16; i++) {
      AmAudioFile f;
      int ret = f.open(path, AmAudioFile::Read);
      fct_chk_eq_int(ret, -1);
    }
    int after = count_open_fds();
    unlink(path.c_str());

    if (before >= 0 && after >= 0) {
      fct_chk_eq_int(after, before);
    }
  }
  FCT_TEST_END();

  FCT_TEST_BGN(fpopen_unknown_extension_leaves_the_stream_to_the_caller) {
    register_test_formats();

    // fpopen() is handed a stream the caller keeps a handle to: it must not
    // be closed behind the caller's back
    FILE *fp = tmpfile();
    fct_req(fp != NULL);

    AmAudioFile f;
    int ret = f.fpopen("test.semstestnofmt", AmAudioFile::Read, fp);
    fct_chk_eq_int(ret, -1);
    // still usable, i.e. not closed by the failed fpopen()
    fct_chk_eq_int(fseek(fp, 0L, SEEK_SET), 0);
    fclose(fp);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(file_get_rejects_skewed_rate) {
    // empty payload: AmAudioFile::read() zero-fills a short read up to the
    // requested size, so code without the checks must not find data here
    TestFile *f = open_test_file("l16", 1000000, 1, 0);
    fct_req(f != NULL);
    unsigned char buf[AUDIO_BUFFER_SIZE];
    int ret = f->get(0, buf, 8000, 160);
    fct_chk_eq_int(ret, -1);
    fct_chk(!feof(f->stream()));
    delete f;
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
