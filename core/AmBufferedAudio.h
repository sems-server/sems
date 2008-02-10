/*
 * $Id: AmAudio.h 711 2008-02-10 18:52:44Z sayer $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
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

#ifndef _AmBufferedAudio_H
#define _AmBufferedAudio_H

#include "AmAudio.h"
/**
 * AmAudio with buffered output
 */
class AmBufferedAudio : public AmAudio {

  unsigned char* output_buffer;
  size_t output_buffer_size, low_buffer_thresh, full_buffer_thresh;
  size_t r, w;

  bool eof;
  int err_code;

  void input_get_audio(unsigned int user_ts);
  inline void allocateBuffer();
  inline void releaseBuffer();

 protected:
  AmBufferedAudio(size_t output_buffer_size, size_t low_buffer_thresh, size_t full_buffer_thresh);
  ~AmBufferedAudio();
  
  void clearBufferEOF();
  void setBufferSize(size_t _output_buffer_size, size_t _low_buffer_thresh, size_t _full_buffer_thresh);

 public:
  virtual int get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples);

};
#endif

// Local Variables:
// mode:C++
// End:
