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

#include "LowcFE.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void LowcFE::convertsf(short *f, Float *t, int cnt)
{
  for (int i = 0; i < cnt; i++)
    t[i] = (Float)f[i];
}

void LowcFE::convertfs(Float *f, short *t, int cnt)
{
  for (int i = 0; i < cnt; i++){
    t[i] = (short)f[i];
  }
}

void LowcFE::copyf(Float *f, Float *t, int cnt)
{
  for (int i = 0; i < cnt; i++)
    t[i] = f[i];
}

void LowcFE::copys(short *f, short *t, int cnt)
{
  for (int i = 0; i < cnt; i++)
    t[i] = f[i];
}

void LowcFE::zeros(short *s, int cnt)
{
  for (int i = 0; i < cnt; i++)
    s[i] = 0;
}

LowcFE::LowcFE(unsigned int sample_rate)
  : sample_rate(sample_rate), erasecnt(0), pitchbufend(0)
{
  pitchbuf = new Float[HISTORYLEN];
  lastq = new Float[POVERLAPMAX];
  history = new short[HISTORYLEN];
  memset(pitchbuf, 0, sizeof(Float) * HISTORYLEN);
  memset(lastq, 0, sizeof(Float) * POVERLAPMAX);
  zeros(history, HISTORYLEN);
}

LowcFE::~LowcFE()
{
  delete[] history;
  delete[] lastq;
  delete[] pitchbuf;
}

/*
 * Save a frames worth of new speech in the history buffer.
 * Return the output speech delayed by POVERLAPMAX.
 */
void LowcFE::savespeech(short *s)
{
  /* make room for new signal */
  copys(&history[FRAMESZ], history, HISTORYLEN - FRAMESZ);
  /* copy in the new frame */
  copys(s, &history[HISTORYLEN - FRAMESZ], FRAMESZ);
  /* copy out the delayed frame */
  copys(&history[HISTORYLEN - FRAMESZ - POVERLAPMAX], s, FRAMESZ);
}

/*
 * A good frame was received and decoded.
 * If right after an erasure, do an overlap add with the synthetic signal.
 * Add the frame to history buffer.
 */
void LowcFE::addtohistory(short *s)
{
  if (erasecnt) {
    short overlapbuf[FRAMESZ];
    /*
     * longer erasures require longer overlaps
     * to smooth the transition between the synthetic
     * and real signal.
     */
    unsigned int olen = poverlap + (erasecnt - 1) * EOVERLAPINCR;
    if (olen > FRAMESZ)
      olen = FRAMESZ;
    getfespeech(overlapbuf, olen);
    overlapaddatend(s, overlapbuf, olen);
    erasecnt = 0;
  }
  savespeech(s);
}

/*
 * Generate the synthetic signal.
 * At the beginning of an erasure determine the pitch, and extract
 * one pitch period from the tail of the signal. Do an OLA for 1/4
 * of the pitch to smooth the signal. Then repeat the extracted signal
 * for the length of the erasure. If the erasure continues for more than
 * 10 ms, increase the number of periods in the pitchbuffer. At the end
 * of an erasure, do an OLA with the start of the first good frame.
 * The gain decays as the erasure gets longer.
 */
void LowcFE::dofe(short *out)
{
  pitchbufend = &pitchbuf[HISTORYLEN];

  if (erasecnt == 0) {
    convertsf(history, pitchbuf, HISTORYLEN); /* get history */
    pitch = findpitch(); /* find pitch */
    poverlap = pitch >> 2; /* OLA 1/4 wavelength */
    /* save original last poverlap samples */
    copyf(pitchbufend - poverlap, lastq, poverlap);
    poffset = 0; /* create pitch buffer with 1 period */
    pitchblen = pitch;
    pitchbufstart = pitchbufend - pitchblen;
    overlapadd(lastq, pitchbufstart - poverlap,
	       pitchbufend - poverlap, poverlap);
    /* update last 1/4 wavelength in history buffer */
    convertfs(pitchbufend - poverlap, &history[HISTORYLEN-poverlap],
	      poverlap);
    getfespeech(out, FRAMESZ); /* get synthesized speech */
  } else if (erasecnt == 1 || erasecnt == 2) {
    /* tail of previous pitch estimate */
    short tmp[POVERLAPMAX];
    int saveoffset = poffset; /* save offset for OLA */
    getfespeech(tmp, poverlap); /* continue with old pitchbuf */
    /* add periods to the pitch buffer */
    poffset = saveoffset;
    while (poffset > pitch)
      poffset -= pitch;
    pitchblen += pitch; /* add a period */
    pitchbufstart = pitchbufend - pitchblen;
    overlapadd(lastq, pitchbufstart - poverlap,
	       pitchbufend - poverlap, poverlap);
    /* overlap add old pitchbuffer with new */
    getfespeech(out, FRAMESZ);
    overlapadd(tmp, out, out, poverlap);
    scalespeech(out);
  } else if (erasecnt > 5) {
    zeros(out, FRAMESZ);
  } else {
    getfespeech(out, FRAMESZ);
    scalespeech(out);
  }
  erasecnt++;
  savespeech(out);
}

/*
 * Estimate the pitch.
 * l - pointer to first sample in last 20 msec of speech.
 * r - points to the sample PITCH_MAX before l
 */
int LowcFE::findpitch()
{
  int i, j, k;
  int bestmatch;
  Float bestcorr;
  Float corr; /* correlation */
  Float energy; /* running energy */
  Float scale; /* scale correlation by average power */
  Float *rp; /* segment to match */
  Float *l = pitchbufend - CORRLEN;
  Float *r = pitchbufend - CORRBUFLEN;

  /* coarse search */
  rp = r;
  energy = 0.f;
  corr = 0.f;
  for (i = 0; i < (int)(CORRLEN); i += NDEC) {
    energy += rp[i] * rp[i];
    corr += rp[i] * l[i];
  }
  scale = energy;
  if (scale < CORRMINPOWER){
    scale = CORRMINPOWER;
  }
  corr = corr / (Float)sqrt(scale);
  bestcorr = corr;
  bestmatch = 0;
  for (j = NDEC; j <= (int)(PITCHDIFF); j += NDEC) {
    energy -= rp[0] * rp[0];
    energy += rp[CORRLEN] * rp[CORRLEN];
    rp += NDEC;
    corr = 0.f;
    for (i = 0; i < (int)(CORRLEN); i += NDEC)
      corr += rp[i] * l[i];
    scale = energy;
    if (scale < CORRMINPOWER)
      scale = CORRMINPOWER;
    corr /= (Float)sqrt(scale);
    if (corr >= bestcorr) {
      bestcorr = corr;
      bestmatch = j;
    }
  }
  /* fine search */
  j = bestmatch - (NDEC - 1);
  if (j < 0)
    j = 0;
  k = bestmatch + (NDEC - 1);
  if (k > (int)(PITCHDIFF))
    k = PITCHDIFF;
  rp = &r[j];
  energy = 0.f;
  corr = 0.f;
  for (i = 0; i < (int)(CORRLEN); i++) {
    energy += rp[i] * rp[i];
    corr += rp[i] * l[i];
  }
  scale = energy;
  if (scale < CORRMINPOWER)
    scale = CORRMINPOWER;

  corr = corr / (Float)sqrt(scale);
  bestcorr = corr;
  bestmatch = j;
  for (j++; j <= k; j++) {
    energy -= rp[0] * rp[0];
    energy += rp[CORRLEN] * rp[CORRLEN];
    rp++;
    corr = 0.f;
    for (i = 0; i < (int)(CORRLEN); i++)
      corr += rp[i] * l[i];
    scale = energy;
    if (scale < CORRMINPOWER)
      scale = CORRMINPOWER;
    corr = corr / (Float)sqrt(scale);
    if (corr > bestcorr) {
      bestcorr = corr;
      bestmatch = j;
    }
  }
  return PITCH_MAX - bestmatch;
}

/*
 * Get samples from the circular pitch buffer. Update poffset so
 * when subsequent frames are erased the signal continues.
 */
void LowcFE::getfespeech(short *out, int sz)
{
  while (sz) {
    int cnt = pitchblen - poffset;
    if (cnt > sz)
      cnt = sz;
    convertfs(&pitchbufstart[poffset], out, cnt);
    poffset += cnt;
    if (poffset == pitchblen)
      poffset = 0;
    out += cnt;
    sz -= cnt;
  }
}

void LowcFE::scalespeech(short *out)
{
  Float g = (Float)1. - (erasecnt - 1) * ATTENFAC;
  for (unsigned int i = 0; i < FRAMESZ; i++) {
    out[i] = (short)(out[i] * g);
    g -= ATTENINCR;
  }
}

/*
 * Overlap add left and right sides
 */
void LowcFE::overlapadd(Float *l, Float *r, Float *o, int cnt)
{
  Float incr = (Float)1. / cnt;
  Float lw = (Float)1. - incr;
  Float rw = incr;
  for (int i = 0; i < cnt; i++) {
    Float t = lw * l[i] + rw * r[i];
    if (t > 32767.)
      t = 32767.;
    else if (t < -32768.)
      t = -32768.;
    o[i] = t;
    lw -= incr;
    rw += incr;
  }
}

void LowcFE::overlapadd(short *l, short *r, short *o, int cnt)
{
  Float incr = (Float)1. / cnt;
  Float lw = (Float)1. - incr;
  Float rw = incr;
  for (int i = 0; i < cnt; i++) {
    Float t = lw * l[i] + rw * r[i];
    if (t > 32767.)
      t = 32767.;
    else if (t < -32768.)
      t = -32768.;
    o[i] = (short)t;
    lw -= incr;
    rw += incr;
  }
}

/*
 * Overlap add the erasure tail with the start of the first good frame
 * Scale the synthetic speech by the gain factor before the OLA.
 */
void LowcFE::overlapaddatend(short *s, short *f, int cnt)
{
  Float incr = (Float)1. / cnt;
  Float gain = (Float)1. - (erasecnt - 1) * ATTENFAC;
  if (gain < 0.)
    gain = (Float)0.;
  Float incrg = incr * gain;
  Float lw = ((Float)1. - incr) * gain;
  Float rw = incr;
  for (int i = 0; i < cnt; i++) {
    Float t = lw * f[i] + rw * s[i];
    if (t > 32767.)
      t = 32767.;
    else if (t < -32768.)
      t = -32768.;
    s[i] = (short)t;
    lw -= incrg;
    rw += incr;
  }
}


