/*
 * Copyright (C) 2006 Sippy Software, Inc.
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
/** @file AmJitterBuffer.h */
#ifndef _AmJitterBuffer_h_
#define _AmJitterBuffer_h_

#include "amci/amci.h"
#include "AmAudio.h"
#include "AmThread.h"
#include "SampleArray.h"

#define INITIAL_JITTER	    80 * SYSTEM_SAMPLECLOCK_RATE / 1000 // 80 miliseconds
#define MAX_JITTER	    2  * SYSTEM_SAMPLECLOCK_RATE // 2 seconds
#define RESYNC_THRESHOLD    5 // resync backward if RESYNC_THRESHOLD packets arrive late

class Packet {
  ShortSample m_data[AUDIO_BUFFER_SIZE * 2];
  unsigned int m_size;
  unsigned int m_ts;
 public:
  Packet *m_next;
  Packet *m_prev;
  void init(const ShortSample *data, unsigned int size, unsigned int ts);

  unsigned int size() const { return m_size; }
  unsigned int ts() const { return m_ts; }
  ShortSample *data() { return m_data; }

  bool operator < (const Packet&) const;
};

class PacketAllocator
{
 private:
  Packet m_packets[MAX_JITTER / 80];
  Packet *m_free_packets;

 public:
  PacketAllocator();
  Packet *alloc(const ShortSample *data, unsigned int size, unsigned int ts);
  void free(Packet *p);
};

class AmJitterBuffer
{
 private:
  AmMutex m_mutex;
  PacketAllocator m_allocator;
  Packet *m_head;
  Packet *m_tail;
  bool m_tsInited;
  unsigned int m_lastTs;
  unsigned int m_lastResyncTs;
  unsigned int m_lastAudioTs;
  unsigned int m_tsDelta;
  bool m_tsDeltaInited;
  int m_delayCount;
  unsigned int m_jitter;
  //    AmRtpStream *m_owner;
  bool m_forceResync;

#ifdef DEBUG_PLAYOUTBUF
  unsigned int m_tsDeltaStart;
#endif

 public:
  AmJitterBuffer();
  void put(const ShortSample *data, unsigned int size, unsigned int ts, bool begin_talk);
  bool get(unsigned int ts, unsigned int ms, ShortSample *out, unsigned int *size, unsigned int *out_ts);
};

#endif // _AmJitterBuffer_h_
