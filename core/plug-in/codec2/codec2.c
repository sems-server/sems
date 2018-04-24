#include "codec2.h"
#include "amci.h"
#include "codecs.h"
#include "../../log.h"
#include <codec2/codec2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static long sems_codec2_create();

static int sems_codec2_destroy(long h_inst);

static int pcm16_2_codec2(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec);

static int codec2_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec);



BEGIN_EXPORTS( "codec2" , AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

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



static long sems_codec2_create(/*const char* bps*/) {

  struct codec2_encoder* c2enc = (struct codec2_encoder*)malloc(sizeof(struct codec2_encoder));
  if (c2enc == NULL) {
    ERROR("CODEC2 codec failed to allocate memory.\n");
    return -1;
  }

  // For now mode is hard-coded.
  const char* bps = "3200";

  int mode = 0;
  if (strcmp(bps, "3200") == 0)
    mode = CODEC2_MODE_3200;
  else if (strcmp(bps, "2400") == 0)
    mode = CODEC2_MODE_2400;
  else if (strcmp(bps, "1600") == 0)
    mode = CODEC2_MODE_1600;
  else if (strcmp(bps, "1400") == 0)
    mode = CODEC2_MODE_1400;
  else if (strcmp(bps, "1300") == 0)
    mode = CODEC2_MODE_1300;
  else if (strcmp(bps, "1200") == 0)
    mode = CODEC2_MODE_1200;
  else if (strcmp(bps, "700") == 0)
    mode = CODEC2_MODE_700;
  else if (strcmp(bps, "700B") == 0)
    mode = CODEC2_MODE_700B;
  else if (strcmp(bps, "700C") == 0)
    mode = CODEC2_MODE_700C;
  else {
        ERROR("Error in mode: %s", bps);
        ERROR("Mode must be 3200, 2400, 1600, 1400, 1300, 1200, 700, 700B or 700C\n");
        return -1;
  }

  struct CODEC2* codec2 = codec2_create(mode);
  if (codec2 == NULL) {
    ERROR("Failed to create CODEC2.\n");
    return -1;
  }

  c2enc->samples_per_frame = codec2_samples_per_frame(codec2);
  c2enc->bits_per_frame = codec2_bits_per_frame(codec2);
  c2enc->nbyte = (c2enc->bits_per_frame + 7) / 8;
  c2enc->codec2 = codec2;

  // We do not use --softdec and --natural codec2 options.
  const int gray = 1;
  codec2_set_natural_or_gray(codec2, gray);

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
    return -1;
  }

  if ((channels != 1) || (rate != 8000)) {
    ERROR("Unsupported input format for Codec2 encoder.\n");
    return -1;
  }

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_codec;
  struct CODEC2* codec2 = c2enc->codec2;

  const int in_sample_count = (size / 2);
  if (in_sample_count < c2enc->samples_per_frame) {
    ERROR("Size of input buffer's frame is less than size of frame in codec2.\n");
    return -1;
  }

  div_t blocks;
  blocks = div(size >> 1, c2enc->samples_per_frame);
  if (blocks.rem) {
    ERROR("pcm16_2_codec2: not integral number of blocks %d.%d\n", blocks.quot, blocks.rem);
    return -1;
  }

  // For each chunk of c2enc->samples_per_frame bytes, encode a single frame.
  int buffer_offset = 0;
  while (blocks.quot--) {
    codec2_encode(codec2, out_buf + buffer_offset, in_buf + buffer_offset);
    buffer_offset += c2enc->nbyte;
  }

  return buffer_offset;
}



static int codec2_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec) {

  // Codec2 decode;

  if (!h_codec) {
    ERROR("Codec2 codec was not initialized.\n");
    return -1;
  }

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_codec;
  struct CODEC2* codec2 = c2enc->codec2;

  // TODO
  // We need to make sure that the input buffer we get from SEMS contains c2enc->samples_per_frame samples.

  codec2_decode(codec2, out_buf, in_buf);

  // We multiply it by two, because size of per frame is 2 bytes.
  return (sizeof(short) * c2enc->samples_per_frame);
}

