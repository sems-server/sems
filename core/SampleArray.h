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

#ifndef _SampleArray_h_
#define _SampleArray_h_

/* MUST be a power of 2 */
#define SIZE_MIX_BUFFER   (1<<14)
/** \brief comparator for user timestamps */
struct ts_less
{
  bool operator()(const unsigned int& l, 
		  const unsigned int& r) const;
};

/** \brief comparator for system timestamps 
 * Note that system timestamps overflow at 48 bit boundaries.
 */
struct sys_ts_less
{
  bool operator()(const unsigned long long& l, 
		  const unsigned long long& r) const;
};

/** \brief timed array of samples */
template <typename T>
class SampleArray
{
public:
  //protected:

  T samples[SIZE_MIX_BUFFER];
  unsigned int last_ts;
  bool         init;

  void clear_all();
  void clear(unsigned int start_ts,unsigned int end_ts);
  void write(unsigned int ts, T* buffer, unsigned int size);
  void read(unsigned int ts, T* buffer, unsigned int size);

  //public:
  SampleArray();

  /**
   * @param size buffer size in [samples].
   */
  void put(unsigned int ts, T* buffer, unsigned int size);

  /**
   * @param buf_size buffer size in [samples].
   */
  void get(unsigned int ts, T* buffer, unsigned int buf_size);
};

// 32 bit sample
typedef int IntSample;

// 32 bit sample array
typedef SampleArray<IntSample> SampleArrayInt;

// 16 bit sample
typedef short ShortSample;

// 16 bit sample array
typedef SampleArray<ShortSample> SampleArrayShort;

typedef short ShortSample;

#include "SampleArray.cc"

#endif
// Local Variables:
// mode:C++
// End:

