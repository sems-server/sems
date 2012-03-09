/*
 * $Id$
 *
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

#ifndef _SampleArray_cc_
#define _SampleArray_cc_

#include "log.h"
#include <string.h>

// inline bool cmp_ts(unsigned int t1, unsigned int t2)
// {
//     // t1 < t2
//     return (t1 - t2 > (unsigned int)(1<<31));
// }

inline bool ts_less::operator()(const unsigned int& l, 
				const unsigned int& r) const
{
  return (l - r > (unsigned int)(1<<31));
}

inline bool sys_ts_less::operator()(const unsigned long long& l, 
				    const unsigned long long& r) const
{
  return (((l - r) & 0xFFFFFFFFFFFFLL) > (1LL<<47));
}

template <typename T>
SampleArray<T>::SampleArray()
  : init(false)
{
}

template <typename T>
void SampleArray<T>::clear_all()
{
  memset(samples,0,sizeof(samples));
}

template <typename T>
void SampleArray<T>::clear(unsigned int start_ts,unsigned int end_ts)
{
  if(end_ts - start_ts >= SIZE_MIX_BUFFER){
    clear_all();
    return;
  }

  unsigned int start_off = start_ts & (SIZE_MIX_BUFFER-1);
  unsigned int end_off = end_ts & (SIZE_MIX_BUFFER-1);

  T* sp = samples + start_off;
  if(start_off < end_off)
    memset(sp,0,(end_off-start_off)*sizeof(T));
  else {
    memset(sp,0,(SIZE_MIX_BUFFER-start_off)*sizeof(T));
    memset(samples,0,end_off*sizeof(T));
  }
}

template <typename T>
void SampleArray<T>::write(unsigned int ts, T* buffer, unsigned int size)
{
  unsigned int off = ts & (SIZE_MIX_BUFFER-1);

  T* sp = samples + off;
  if(off+size <= SIZE_MIX_BUFFER)
    memcpy(sp,buffer,size*sizeof(T));
  else {
    unsigned int s = SIZE_MIX_BUFFER-off;
    memcpy(sp,buffer,s*sizeof(T));

    buffer += s;
    size -= s;
    memcpy(samples,buffer,size*sizeof(T));
  }
}

template <typename T>
void SampleArray<T>::read(unsigned int ts, T* buffer, unsigned int size)
{
  unsigned int off = ts & (SIZE_MIX_BUFFER-1);
    
  T* sp = samples + off;
  if(off+size <= SIZE_MIX_BUFFER){
    memcpy(buffer,sp,size*sizeof(T));
  }
  else {
    unsigned int s = SIZE_MIX_BUFFER - off;
    memcpy(buffer,sp,s*sizeof(T));

    buffer += s;;
    size -= s;
    memcpy(buffer,samples,size*sizeof(T));
  }
}

template <typename T>
void SampleArray<T>::put(unsigned int ts, T* buffer, unsigned int size)
{
  //assert(size <= SIZE_MIX_BUFFER);

  if(!init){
    clear_all();
    last_ts = ts;
    init = true;
  }

  if(ts_less()(ts,last_ts-SIZE_MIX_BUFFER)){
    DBG("throwing away too old packet (ts=%u; last_ts=%u).\n",ts,last_ts);
    return;
  }

  if(ts_less()(last_ts,ts))
    clear(last_ts,ts);

  write(ts,buffer,size);
  if(ts_less()(last_ts,ts+size))
    last_ts = ts+size;
}

template <typename T>
void SampleArray<T>::get(unsigned int ts, T* buffer, unsigned int size)
{
  //assert(size <= SIZE_MIX_BUFFER);

  if(!init || 
     !ts_less()(ts,last_ts) ||
     !ts_less()(last_ts-SIZE_MIX_BUFFER,ts+size)){

    // !init ||
    // (ts+size <= last_ts-SIZE_MIX_BUFFER) ||
    // (ts >= last_ts)
    memset(buffer,0,size*sizeof(T));
    return;
  }

  // init &&
  // (ts+size > last_ts-SIZE_MIX_BUFFER) &&
  // (ts < last_ts)

  if(ts_less()(ts,last_ts-SIZE_MIX_BUFFER)){

    // ts < last_ts-SIZE_MIX_BUFFER
    unsigned int s = last_ts-SIZE_MIX_BUFFER-ts;
    memset(buffer,0,s*sizeof(T));

    ts += s; buffer += s; size -= s;
    read(ts,buffer,size);
  } 
  else if(ts_less()(last_ts,ts+size)){

    // ts+size > last_ts
    unsigned int s = last_ts-ts;
    read(ts,buffer,s);
	
    buffer += s; size -= s;
    memset(buffer,0,size*sizeof(T));
  }
  else {
    // ts+size <= last_ts
    read(ts,buffer,size);
  }
}

#endif
