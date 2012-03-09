/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
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
/** @file AmConferenceChannel.h */
#ifndef AmConferenceChannel_h
#define AmConferenceChannel_h

#include "AmAudio.h"
#include "AmConferenceStatus.h"

/** 
 * \brief one channel of a conference
 * 
 * A ConferenceChannel is one channel, i.e. to/from one 
 * participant, in a conference. 
 */
class AmConferenceChannel: public AmAudio
{
  bool                own_channel;
  int                 channel_id;
  string              conf_id;
  AmConferenceStatus* status;

 protected:
  // Fake implement AmAudio's pure virtual methods
  // this avoids to copy the samples locally by implementing only get/put
  int read(unsigned int user_ts, unsigned int size){ return -1; }
  int write(unsigned int user_ts, unsigned int size){ return -1; }

  // override AmAudio
  int get(unsigned long long system_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int nb_samples);
  int put(unsigned long long system_ts, unsigned char* buffer, 
	  int input_sample_rate, unsigned int size);

 public:
  AmConferenceChannel(AmConferenceStatus* status, 
		      int channel_id, bool own_channel);

  ~AmConferenceChannel();

  string getConfID() { return conf_id; }
};

#endif
