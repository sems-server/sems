#ifndef SEMS_CODEC2_H
#define SEMS_CODEC2_H

#include <codec2/codec2.h>

/**
 *  A structure represents codec2's basic data in SEMS. Basically we create this
 * object in sems_codec2_create() function and destroy in sems_codec2_destroy()
 * function. Also, we pass to other functios as an argument address of object,
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

#endif

