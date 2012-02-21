/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LowcFE_h_
#define _LowcFE_h_

#include "AmAudio.h"

typedef float Float;

#define PITCH_MIN  ((unsigned int) (sample_rate / 200))  /* minimum allowed pitch, 200 Hz */
#define PITCH_MAX  ((unsigned int) (sample_rate / 66.6)) /* maximum allowed pitch, 66 Hz */
#define PITCHDIFF  (PITCH_MAX - PITCH_MIN)
#define POVERLAPMAX   (PITCH_MAX >> 2)              /* maximum pitch OLA window */
#define HISTORYLEN    (PITCH_MAX * 3 + POVERLAPMAX) /* history buff length*/
#define NDEC          2                             /* 2:1 decimation */
#define CORRLEN       20 * sample_rate / 1000 /* 20 msec correlation length */
#define CORRBUFLEN    (CORRLEN + PITCH_MAX)         /* correlation buffer length */
#define CORRMINPOWER  (((Float)250.) * (sample_rate / 8000))                 /* minimum power */
#define EOVERLAPINCR  32                            /* end OLA increment per frame,4 ms */
#define FRAMESZ       (10 * sample_rate / 1000) /* 10 msec */
#define ATTENFAC      ((Float).2)                   /* attenu. factor per 10 ms frame */
#define ATTENINCR     (ATTENFAC/(Float)(FRAMESZ))   /* attenuation per sample */

/** \brief LowcFE erased frame generator for fec (plc) */
class LowcFE {

 public:
  LowcFE(unsigned int sample_rate);
  ~LowcFE();
  void dofe(short *s); /* synthesize speech for erasure */
  void addtohistory(short *s); /* add a good frame to history buf */

 protected:
  unsigned int sample_rate;
  int erasecnt; /* consecutive erased frames */
  int poverlap; /* overlap based on pitch */
  int poffset; /* offset into pitch period */
  int pitch; /* pitch estimate */
  int pitchblen; /* current pitch buffer length */
  Float *pitchbufend; /* end of pitch buffer */
  Float *pitchbufstart; /* start of pitch buffer */
  Float *pitchbuf; /* buffer for cycles of speech */
  Float *lastq; /* saved last quarter wavelength */
  short *history; /* history buffer */

  void scalespeech(short *out);
  void getfespeech(short *out, int sz);
  void savespeech(short *s);
  int findpitch();
  void overlapadd(Float *l, Float *r, Float *o, int cnt);
  void overlapadd(short *l, short *r, short *o, int cnt);
  void overlapaddatend(short *s, short *f, int cnt);
  void convertsf(short *f, Float *t, int cnt);
  void convertfs(Float *f, short *t, int cnt);
  void copyf(Float *f, Float *t, int cnt);
  void copys(short *f, short *t, int cnt);
  void zeros(short *s, int cnt);
};


#endif
