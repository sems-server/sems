/*
 * $Id$
 *
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AmAudioMixIn.h"
#include "SampleArray.h"

AmAudioMixIn::AmAudioMixIn(AmAudio* A, AmAudioFile* B, 
			   unsigned int s, double l,
			   bool finish_b_while_mixing) 
  :   A(A),B(B), s(s), l(l), 
      mixing(false), next_start_ts_i(false),
      finish_b_while_mixing(finish_b_while_mixing)
{
}
AmAudioMixIn::~AmAudioMixIn() { }

int AmAudioMixIn::get(unsigned int user_ts, unsigned char* buffer, 
		       unsigned int nb_samples) {
  if (!mixing) {
    if (!next_start_ts_i) {
      next_start_ts_i = true;
      next_start_ts = user_ts + s*DEFAULT_SAMPLE_RATE;
    } else {
      if (ts_less()(next_start_ts, user_ts)) {
	DBG("starting mix-in\n");
	mixing = true;
	next_start_ts = user_ts + s*DEFAULT_SAMPLE_RATE;
      }
    }
  } 
  
  if (!mixing) {
    return A->get(user_ts, buffer, nb_samples);
  } else {
    if (l < 0.01) { // epsilon 
      // only play back from B
      int res = B->get(user_ts, buffer, nb_samples);
      if (res <= 0) { // B empty
	res = A->get(user_ts, buffer, nb_samples);
	mixing = false;
	B->rewind();
      }
      return  res;
    } else {      // mix the two
      int res = 0;
      short* pdest = (short*)buffer;
      // get audio from A
      int len = A->get(user_ts, (unsigned char*)mix_buf, nb_samples);

      if ((len<0) && !finish_b_while_mixing) { // A finished
	return len;
      }
      for (int i=0;i<len;i++) {
	pdest[i]=(short)(((double)mix_buf[i])*(1.0-l));
      }

      res = len;

      // clear the rest
      unsigned int len_from_a = 0;
      if (res>0)
	len_from_a=(unsigned int)res;
      
      if (nb_samples<<1 != len_from_a)
	memset((void*)&pdest[len_from_a>>1], 0, 
	       (nb_samples<<1) - len_from_a);

      // add audio from B
      len = B->get(user_ts, (unsigned char*)mix_buf, nb_samples);

      if (len<0) { // B finished
	mixing = false;
	B->rewind();
      } else {
	for (int i=0;i<len;i++) {
	  pdest[i]+=(short)(((double)mix_buf[i])*l);
	}
	if (len>res) // audio from B is longer than from A
	  res = len;
      }
      return res;
    }
  }
}

int AmAudioMixIn::put(unsigned int user_ts, unsigned char* buffer, unsigned int size) {
  ERROR("writing not supported\n");
  return -1;
}
