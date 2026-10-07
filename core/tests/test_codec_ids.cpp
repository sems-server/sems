#include "fct.h"

#include "log.h"

#include "AmPlugIn.h"
#include "amci/amci.h"
#include "amci/codecs.h"

#include <stddef.h>

// The ids in amci/codecs.h are the keys under which AmPlugIn holds codecs, so
// their only requirement is that they differ from each other. CODEC_G729 and
// CODEC_ULAW16 were both 14: the wideband G.711 block was appended in 2012
// continuing from CODEC_G722_NB (13) without noticing that the G.729 module
// had taken 14 in 2010.
//
// Sharing an id is not a question of one codec silently losing out:
// AmPlugIn::addCodec() refuses the second registration and
// AmPlugIn::loadAudioPlugIn() turns that into an error for the whole plug-in,
// abandoning its payloads and file formats too. Which of the two plug-ins pays
// is readdir() order over the module directory.
//
// No plug-in registers the wideband G.711 codecs today, so the collision was
// still dormant - the table below keeps it that way for the next codec added.

namespace {

struct codec_id {
  const char *name;
  int id;
};

// every id defined in amci/codecs.h
const codec_id codec_ids[] = {
    {"CODEC_PCM16", CODEC_PCM16},
    {"CODEC_ULAW", CODEC_ULAW},
    {"CODEC_ALAW", CODEC_ALAW},
    {"CODEC_GSM0610", CODEC_GSM0610},
    {"CODEC_ILBC", CODEC_ILBC},
    {"CODEC_MP3", CODEC_MP3},
    {"CODEC_TELEPHONE_EVENT", CODEC_TELEPHONE_EVENT},
    {"CODEC_G726_16", CODEC_G726_16},
    {"CODEC_G726_24", CODEC_G726_24},
    {"CODEC_G726_32", CODEC_G726_32},
    {"CODEC_G726_40", CODEC_G726_40},
    {"CODEC_L16", CODEC_L16},
    {"CODEC_G722_NB", CODEC_G722_NB},
    {"CODEC_G729", CODEC_G729},
    {"CODEC_CELT32", CODEC_CELT32},
    {"CODEC_CELT44", CODEC_CELT44},
    {"CODEC_CELT48", CODEC_CELT48},
    {"CODEC_CELT32_2", CODEC_CELT32_2},
    {"CODEC_CELT44_2", CODEC_CELT44_2},
    {"CODEC_CELT48_2", CODEC_CELT48_2},
    {"CODEC_SPEEX_NB", CODEC_SPEEX_NB},
    {"CODEC_SPEEX_WB", CODEC_SPEEX_WB},
    {"CODEC_SPEEX_UB", CODEC_SPEEX_UB},
    {"CODEC_SILK_NB", CODEC_SILK_NB},
    {"CODEC_SILK_MB", CODEC_SILK_MB},
    {"CODEC_SILK_WB", CODEC_SILK_WB},
    {"CODEC_SILK_UB", CODEC_SILK_UB},
    {"CODEC_iSAC_WB", CODEC_iSAC_WB},
    {"CODEC_ULAW16", CODEC_ULAW16},
    {"CODEC_ALAW16", CODEC_ALAW16},
    {"CODEC_ULAW32", CODEC_ULAW32},
    {"CODEC_ALAW32", CODEC_ALAW32},
    {"CODEC_ULAW48", CODEC_ULAW48},
    {"CODEC_ALAW48", CODEC_ALAW48},
    {"CODEC_OPUS", CODEC_OPUS},
    {"CODEC_CODEC2_3200", CODEC_CODEC2_3200},
    {"CODEC_CODEC2_2400", CODEC_CODEC2_2400},
    {"CODEC_CODEC2_1600", CODEC_CODEC2_1600},
    {"CODEC_CODEC2_1400", CODEC_CODEC2_1400},
};

const size_t nb_codec_ids = sizeof(codec_ids) / sizeof(codec_ids[0]);

// test-private codec ids, well clear of amci/codecs.h and the other suites
const int TST_CODEC_TAKEN = 9201;
const int TST_CODEC_FREE = 9202;

long tst_codec_init(const char *, const char **, amci_codec_fmt_info_t **) { return 0; }

// id, encode, decode, plc, init, destroy, bytes2samples, samples2bytes, negotiate_fmt
amci_codec_t tst_codec_taken = {TST_CODEC_TAKEN, NULL, NULL, NULL, tst_codec_init, NULL, NULL, NULL, NULL};
amci_codec_t tst_codec_same_id = {TST_CODEC_TAKEN, NULL, NULL, NULL, tst_codec_init, NULL, NULL, NULL, NULL};
amci_codec_t tst_codec_free = {TST_CODEC_FREE, NULL, NULL, NULL, tst_codec_init, NULL, NULL, NULL, NULL};

struct registrations {
  int taken;   // first codec to claim TST_CODEC_TAKEN
  int same_id; // second codec claiming TST_CODEC_TAKEN
  int free_id; // another codec on an id of its own
};

// addCodec() mutates the AmPlugIn singleton and there is no way to take a
// codec back out, so register only once and assert on the results - a suite
// body runs more than once (see register_test_codecs() in test_audio_decode.cpp
// for the same idiom)
const registrations &register_once() {
  static registrations res;
  static bool done = false;
  if (!done) {
    done = true;
    res.taken = AmPlugIn::instance()->addCodec(&tst_codec_taken);
    res.same_id = AmPlugIn::instance()->addCodec(&tst_codec_same_id);
    res.free_id = AmPlugIn::instance()->addCodec(&tst_codec_free);
  }
  return res;
}

} // namespace

FCTMF_SUITE_BGN(test_codec_ids) {

  // the ids of amci/codecs.h must stay distinct - a duplicate costs one of the
  // two plug-ins its whole registration, see the comment above
  FCT_TEST_BGN(codec_ids_are_unique) {
    for (size_t i = 0; i < nb_codec_ids; i++) {
      for (size_t j = i + 1; j < nb_codec_ids; j++) {
        fct_xchk(codec_ids[i].id != codec_ids[j].id, "%s and %s share codec id %d", codec_ids[i].name,
                 codec_ids[j].name, codec_ids[i].id);
      }
    }
  }
  FCT_TEST_END();

  // the wideband G.711 block moved, so CODEC_G729 keeps the 14 it has held
  // since the module was added: renumbering a codec that is actually
  // registered was not needed to resolve the collision
  FCT_TEST_BGN(g729_keeps_its_id_and_ulaw16_moved_off_it) {
    fct_chk_eq_int(CODEC_G729, 14);
    fct_chk(CODEC_ULAW16 != CODEC_G729);
  }
  FCT_TEST_END();

  // why uniqueness matters: the second codec claiming an id is refused, and
  // loadAudioPlugIn() fails its whole plug-in on that
  FCT_TEST_BGN(addCodec_refuses_an_id_already_held) {
    const registrations &res = register_once();
    fct_chk_eq_int(res.taken, 0);
    fct_chk_eq_int(res.same_id, -1);
    // a free id is still accepted, so it was the id that was refused
    fct_chk_eq_int(res.free_id, 0);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
