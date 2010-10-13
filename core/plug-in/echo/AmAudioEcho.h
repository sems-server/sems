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

#ifndef _AmAudioEcho_h_
#define _AmAudioEcho_h_

#include "AmAudio.h"
#include "SampleArray.h"

/** \brief audio device that echos audio from input to output */
class AmAudioEcho : public AmAudio
{
  SampleArrayShort timed_buffer;

  int read(unsigned int user_ts, unsigned int size);
  int write(unsigned int user_ts, unsigned int size);

public:
  AmAudioEcho();
  ~AmAudioEcho();
};

#endif

