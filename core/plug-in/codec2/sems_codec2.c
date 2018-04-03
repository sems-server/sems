#include "amci.h"
#include "codecs.h"
#include "../../log.h"
#include <codec2/codec2.h>

#include <stdio.h>
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
  printf("Create Codec2\n");

  // For now it is hard-coded.
  int mode = CODEC2_MODE_3200;
  struct CODEC2* codec2 = codec2_create(mode);

  return (long)codec2; 
}


static int sems_codec2_destroy(long h_inst) {
  printf("Destroy Codec2\n");

  codec2_destroy(h_inst);
}


static int pcm16_2_codec2(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec) {

  printf("Codec2 encode\n");

  struct CODEC2* codec2 = (struct CODEC2*)h_codec;

  const int nsam = codec2_samples_per_frame(codec2);
  const int nbit = codec2_bits_per_frame(codec2);
  const int nbyte = (nbit + 7) / 8;

  // We do not use --softdec and --natural codec2 options.
  int gray = 1;
  codec2_set_natural_or_gray(codec2, gray);

  codec2_encode(codec2, out_buf, in_buf);

  return nbyte;
}


static int codec2_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec) {

  printf("Codec2 decode\n");

  struct CODEC2* codec2 = (struct CODEC2*)h_codec;

  const int nsam = codec2_samples_per_frame(codec2);
  const int nbit = codec2_bits_per_frame(codec2);
  const int nbyte = (nbit + 7) / 8;

  // We do not use --softdec and --natural codec2 options.
  int gray = 1;
  codec2_set_natural_or_gray(codec2, gray);

  // assert(nbyte == size);
  /*
  int ret = (nbyte == size);
  int frames = 0;
  float ber_est = 0.0;
  while (ret) {
    frames++;
  } */

  codec2_decode(codec2, out_buf, in_buf);

  return nsam;
}

