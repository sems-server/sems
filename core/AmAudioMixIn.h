/*
 * $Id: AmAudioMixIn.h 322 2007-05-02 18:49:26Z sayer $
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
/** @file AmAudioMixIn.h */
#ifndef _AM_AUDIO_MIX_IN_H
#define _AM_AUDIO_MIX_IN_H

#include "AmAudio.h"
#include "AmAudioFile.h"


#define MAX_PACKETLENGTH_MS   30
#define MAX_BUF_SAMPLES  8000 * MAX_PACKETLENGTH_MS / 1000
#define DEFAULT_SAMPLE_RATE 8000 // eh...

/**
 * \brief \ref AmAudio to mix in every n seconds a file
 * 
 * AmAudio that plays Audio A and 
 * every s seconds mixes in AudioFile B with level l.
 * If l == 0, playback of A is not continued when playing B,
 * which means that it continues right where it was before 
 * playback of B started.
 *
 */
class AmAudioMixIn : public AmAudio {
  AmAudio* A;
  AmAudioFile* B;
  unsigned int s;
  double l;
  bool finish_b_while_mixing;

  bool mixing;

  unsigned int next_start_ts;
  bool next_start_ts_i;

  short mix_buf[MAX_BUF_SAMPLES];  // 240


 public:
  AmAudioMixIn(AmAudio* A, AmAudioFile* B, 
	       unsigned int s, double l, 
	       bool finish_b_while_mixing = false);
  ~AmAudioMixIn();
 protected:
  // not used
  int read(unsigned int user_ts, unsigned int size){ return -1; }
  int write(unsigned int user_ts, unsigned int size){ return -1; }
    
  // override AmAudio
  int get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples);
  int put(unsigned int user_ts, unsigned char* buffer, unsigned int size);
};


#endif // _AM_AUDIO_MIX_IN_H
