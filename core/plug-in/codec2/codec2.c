#include "codec2.h"
#include "amci.h"
#include "codecs.h"
#include "../../log.h"
#include <codec2/codec2.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static long sems_codec2_create();

static int sems_codec2_destroy(long h_inst);

static int pcm16_2_codec2(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec);

static int codec2_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec);



BEGIN_EXPORTS( "sems_codec2" , AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

  BEGIN_CODECS
    CODEC( CODEC_CODEC2, pcm16_2_codec2, codec2_2_pcm16,
           AMCI_NO_CODEC_PLC,
           sems_codec2_create,
           sems_codec2_destroy,
           NULL, NULL )
  END_CODECS

  BEGIN_PAYLOADS
    PAYLOAD( -1, "CODEC2", 8000, 8000, 1, CODEC_CODEC2, AMCI_PT_AUDIO_FRAME )
  END_PAYLOADS

  BEGIN_FILE_FORMATS
  END_FILE_FORMATS

END_EXPORTS



static long sems_codec2_create() {

  struct codec2_encoder* c2enc = (struct codec2_encoder*)malloc(sizeof(struct codec2_encoder));
  if (c2enc == NULL) {
    ERROR("CODEC2 codec failed to allocate memory.\n");
    return 0;
  }

  // For now mode is hard-coded.
  int mode = CODEC2_MODE_3200;

  struct CODEC2* codec2 = codec2_create(mode);
  if (codec2 == NULL) {
    ERROR("Failed to create CODEC2.\n");
    return 0;
  }

  c2enc->samples_per_frame = codec2_samples_per_frame(codec2);
  c2enc->bits_per_frame = codec2_bits_per_frame(codec2);
  c2enc->nbyte = (c2enc->bits_per_frame + 7) / 8;
  c2enc->codec2 = codec2;

  return (long)c2enc;
}


static int sems_codec2_destroy(long h_inst) {

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_inst;

  codec2_destroy(c2enc->codec2);
  free(c2enc);
}


static int pcm16_2_codec2(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec) {

  // Codec2 encode;

  if (!h_codec) {
    ERROR("Codec2 codec was not initialized.\n");
    return 0;
  }

  if ((channels != 1) || (rate != 8000)) {
    ERROR("Unsupported input format for Codec2 encoder.\n");
    return 0;
  }

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_codec;
  struct CODEC2* codec2 = c2enc->codec2;

  // We do not use --softdec and --natural codec2 options.
  int gray = 1;
  codec2_set_natural_or_gray(codec2, gray);

  codec2_encode(codec2, out_buf, in_buf);

  return c2enc->nbyte;
}


static int codec2_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec) {

  // Codec2 decode;

  if (!h_codec) {
    ERROR("Codec2 codec was not initialized.\n");
    return 0;
  }

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_codec;
  struct CODEC2* codec2 = c2enc->codec2;

  // TODO
  // We need to make sure that the input buffer we get from SEMS contains c2enc->samples_per_frame samples.

  // We do not use --softdec and --natural codec2 options.
  int gray = 1;
  codec2_set_natural_or_gray(codec2, gray);

  codec2_decode(codec2, out_buf, in_buf);

  return c2enc->samples_per_frame;
}

