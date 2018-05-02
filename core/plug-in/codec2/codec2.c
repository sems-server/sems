#include "codec2.h"
#include "amci.h"
#include "codecs.h"
#include "../../log.h"
#include <codec2/codec2.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/**
 * In order to tell SEMS to buffer the data and invoke the pcm16_2_xyz function only after 20ms
 * or 40ms we should specify parameters (frame length, frame size and encoded frame size) here.
 */
static amci_codec_fmt_info_t codec2_fmt_description_20ms_3200[] = { {AMCI_FMT_FRAME_LENGTH, 20},
                                                                    {AMCI_FMT_FRAME_SIZE, 160},
                                                                    {AMCI_FMT_ENCODED_FRAME_SIZE, 8}, {0,0}};

static amci_codec_fmt_info_t codec2_fmt_description_20ms_2400[] = { {AMCI_FMT_FRAME_LENGTH, 20},
                                                                    {AMCI_FMT_FRAME_SIZE, 160},
                                                                    {AMCI_FMT_ENCODED_FRAME_SIZE, 6}, {0,0}};

static amci_codec_fmt_info_t codec2_fmt_description_40ms_1600[] = { {AMCI_FMT_FRAME_LENGTH, 40},
                                                                    {AMCI_FMT_FRAME_SIZE, 320},
                                                                    {AMCI_FMT_ENCODED_FRAME_SIZE, 8}, {0,0}};

static amci_codec_fmt_info_t codec2_fmt_description_40ms_1400[] = { {AMCI_FMT_FRAME_LENGTH, 40},
                                                                    {AMCI_FMT_FRAME_SIZE, 320},
                                                                    {AMCI_FMT_ENCODED_FRAME_SIZE, 7}, {0,0}};



BEGIN_EXPORTS( "codec2" , AMCI_NO_MODULEINIT, AMCI_NO_MODULEDESTROY )

  BEGIN_CODECS
    CODEC( CODEC_CODEC2_3200, pcm16_2_codec2, codec2_2_pcm16,
           AMCI_NO_CODEC_PLC,
           sems_codec2_3200_create,
           sems_codec2_destroy,
           NULL, NULL )
    CODEC( CODEC_CODEC2_2400, pcm16_2_codec2, codec2_2_pcm16,
           AMCI_NO_CODEC_PLC,
           sems_codec2_2400_create,
           sems_codec2_destroy,
           NULL, NULL )
    CODEC( CODEC_CODEC2_1600, pcm16_2_codec2, codec2_2_pcm16,
           AMCI_NO_CODEC_PLC,
           sems_codec2_1600_create,
           sems_codec2_destroy,
           NULL, NULL )
    CODEC( CODEC_CODEC2_1400, pcm16_2_codec2, codec2_2_pcm16,
           AMCI_NO_CODEC_PLC,
           sems_codec2_1400_create,
           sems_codec2_destroy,
           NULL, NULL )
  END_CODECS

  BEGIN_PAYLOADS
    PAYLOAD( -1, "CODEC2_3200", 8000, 8000, 1, CODEC_CODEC2_3200, AMCI_PT_AUDIO_FRAME )
    PAYLOAD( -1, "CODEC2_2400", 8000, 8000, 1, CODEC_CODEC2_2400, AMCI_PT_AUDIO_FRAME )
    PAYLOAD( -1, "CODEC2_1600", 8000, 8000, 1, CODEC_CODEC2_1600, AMCI_PT_AUDIO_FRAME )
    PAYLOAD( -1, "CODEC2_1400", 8000, 8000, 1, CODEC_CODEC2_1400, AMCI_PT_AUDIO_FRAME )
  END_PAYLOADS

  BEGIN_FILE_FORMATS
  END_FILE_FORMATS

END_EXPORTS



/**
 * Function creates/initializes Codec2.
 *
 * Arguments:
 * - bps: Bit rate encoding number, currently we support only 3200, 2400, 1600
 *        and 1400 bit rate encoding.
 * - amci_codec_fmt_info_t: Encoding format description info, basically we need
 *                          frame length, frame size and encoded frame size.
 *
 * Returns an address of created codec2 structure.
 */
long sems_codec2_create(const int bps,
                        amci_codec_fmt_info_t** format_description) {

  struct codec2_encoder* c2enc = (struct codec2_encoder*)malloc(sizeof(struct codec2_encoder));
  if (c2enc == NULL) {
    ERROR("CODEC2 codec failed to allocate memory.\n");
    return -1;
  }

  int mode = 0;
  if (bps == 3200) {
    mode = CODEC2_MODE_3200;
    *format_description = codec2_fmt_description_20ms_3200;
  } else if (bps == 2400) {
    mode = CODEC2_MODE_2400;
    *format_description = codec2_fmt_description_20ms_2400;
    } else if (bps == 1600) {
      mode = CODEC2_MODE_1600;
      *format_description = codec2_fmt_description_40ms_1600;
      } else if (bps == 1400) {
        mode = CODEC2_MODE_1400;
        *format_description = codec2_fmt_description_40ms_1400;
        } else {
          ERROR("Error in mode: %s", bps);
          ERROR("Mode must be 3200, 2400, 1600 or 1400\n");
          free(c2enc);
          return -1;
        }

  struct CODEC2* codec2 = codec2_create(mode);
  if (codec2 == NULL) {
    ERROR("Failed to create CODEC2.\n");
    free(c2enc);
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



/**
 * Below four functions create/initialize Codec2 based on bit rate encoding.
 * 3200, 2400, 1600 or 1400.
 *
 * Arguments:
 * - format_parameters: For now we do not use format parameters.
 * - format_parameters_out: For now we do not use output format parameters.
 * - amci_codec_fmt_info_t: Encoding format description info, basically we need
 *                          frame length, frame size and encoded frame size.
 *
 * Each function returns an address of created codec2 structure.
 */
long sems_codec2_3200_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description) {
  return sems_codec2_create(3200, format_description);
}

long sems_codec2_2400_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description) {
  return sems_codec2_create(2400, format_description);
}

long sems_codec2_1600_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description) {
  return sems_codec2_create(1600, format_description);
}

long sems_codec2_1400_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description) {
  return sems_codec2_create(1400, format_description);
}



/**
 * Functions destroys/frees Codec2 structure created by sems_codec2_create function.
 *
 * Arguments:
 * - h_inst: An address of created codec2 structure.
 */
void sems_codec2_destroy(long h_inst) {

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_inst;

  codec2_destroy(c2enc->codec2);
  free(c2enc);
}



/**
 * Function encodes buffer using Codec2 library.
 *
 * Arguments:
 * - out_buf: An output buffer where will be written encoded data.
 * - in_buf: A raw data, which will be encoded using Codec2's encode function.
 * - size: Size of in_buf.
 * - channels: A channel, which must be 1 as we use one channel.
 * - rate: It must be 8000.
 * - h_codec: An address of codec2 structure created by sems_codec2_create function.
 *
 * Returns a length of encoded data in bytes.
 */
int pcm16_2_codec2(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
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
  int out_buffer_offset = 0;
  int in_buffer_offset = 0;
  while (blocks.quot--) {
    codec2_encode(codec2, out_buf + out_buffer_offset, in_buf + in_buffer_offset);
    out_buffer_offset += c2enc->nbyte;
    in_buffer_offset += c2enc->samples_per_frame;
  }

  return out_buffer_offset;
}



/**
 * Function decodes received data using Codec2 library.
 *
 * Arguments:
 * - out_buf: An output buffer where will be written decoded data.
 * - in_buf: An encoded data, which will be decoded using Codec2's decode function.
 * - size: Size of in_buf.
 * - channels: A channel, which must be 1 as we use one channel.
 * - rate: It must be 8000.
 * - h_codec: An address of codec2 structure created by sems_codec2_create function.
 */
int codec2_2_pcm16(unsigned char* out_buf, unsigned char* in_buf, unsigned int size,
                          unsigned int channels, unsigned int rate, long h_codec) {

  // Codec2 decode;

  if (!h_codec) {
    ERROR("Codec2 codec was not initialized.\n");
    return -1;
  }

  struct codec2_encoder* c2enc = (struct codec2_encoder*)h_codec;
  struct CODEC2* codec2 = c2enc->codec2;

  int frames = 0;
  int out_buffer_offset = 0;
  int in_buffer_offset = 0;
  unsigned char* bits_to_decode = malloc(c2enc->nbyte * sizeof(unsigned char));

  memcpy(bits_to_decode, in_buf + in_buffer_offset, c2enc->nbyte);
  frames++;

  /*
   * We do not know where frame boundaries are, but the minimum frame size is
   * c2enc->nbyte.
   */
  int frame_count = size / c2enc->nbyte;
  while (frame_count) {
    codec2_decode(codec2, out_buf + out_buffer_offset, bits_to_decode);

    out_buffer_offset += c2enc->samples_per_frame;
    in_buffer_offset += c2enc->nbyte;
    frame_count--;

    if (frame_count) {
      memcpy(bits_to_decode, in_buf + in_buffer_offset, c2enc->nbyte);
      frames++;
    }
  }

  // Cleanup.
  free(bits_to_decode);

  // We multiply it by two (sizeof(short)), because size of per frame is 2 bytes (short).
  return frames * (sizeof(short) * c2enc->samples_per_frame);
}

