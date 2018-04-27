#ifndef SEMS_CODEC2_H
#define SEMS_CODEC2_H

#include "amci.h"
#include "codecs.h"
#include <codec2/codec2.h>


/**
 *  A structure represents codec2's basic data in SEMS. Basically we create this
 * object in sems_codec2_create() function and destroy in sems_codec2_destroy()
 * function. Also, we pass to other functions as an argument address of object,
 * see h_codec and h_inst arguments for details.
 *
 * Data members:
 * - samples_per_frame: A number of samples per frame.
 * - bits_per_frame: A number of bits per frame.
 * - nbyte: Rounded bits to byte, basically (bits_per_frame + 7) / 8;
 * - codec2: The main object of codec2, we should free it manually.
 */
struct codec2_encoder {
  int samples_per_frame;
  int bits_per_frame;
  int nbyte;
  struct CODEC2 *codec2;
};



/**
 *  Function creates/initializes Codec2.
 *
 * Arguments:
 * - bps: Bit rate encoding number, currently we support only 3200, 2400, 1600
 *       and 1400 bit rate encoding.
 * - amci_codec_fmt_info_t: Encoding format description info, basically we need
 *                         frame length, frame size and encoded frame size.
 *
 * Returns an address of created codec2 structure.
 */
long sems_codec2_create(const int bps, amci_codec_fmt_info_t** format_description);



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
                             amci_codec_fmt_info_t** format_description);
long sems_codec2_2400_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description);
long sems_codec2_1600_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description);
long sems_codec2_1400_create(const char* format_parameters,
                             const char** format_parameters_out,
                             amci_codec_fmt_info_t** format_description);



/**
 * Functions destroys/frees Codec2 structure created by sems_codec2_create function.
 *
 * Arguments:
 * - h_inst: An address of created codec2 structure.
 */
void sems_codec2_destroy(long h_inst);



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
                   unsigned int channels, unsigned int rate, long h_codec);



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
                   unsigned int channels, unsigned int rate, long h_codec);

#endif

