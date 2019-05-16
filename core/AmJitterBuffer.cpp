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
 
#include "AmAudio.h"
#include "AmJitterBuffer.h"
#include "log.h"
#include "SampleArray.h"

//
// Warning: 
// This jitter buffer seems to increase the buffer size, if jitter (delay variation) is 
// present in the stream, or for example a temporary delay spike is detected, 
// but it does not adapt the buffer to shrink again if the situation has improved. 
// The adaptive playout buffer method usually shows much better results
// in minimizing the total playout delay (using more processing power though).
//

bool Packet::operator < (const Packet& p) const
{
  return ts_less()(m_ts, p.m_ts);
}

void Packet::init(const ShortSample *data, unsigned int size, unsigned int ts)
{
  size = PCM16_S2B(size);
  if (size > sizeof(m_data))
    size = sizeof(m_data);
  m_size = PCM16_B2S(size);
  memcpy(m_data, data, size);
  m_ts = ts;
}

PacketAllocator::PacketAllocator()
{
  m_free_packets = m_packets;
  for (int i = 1; i < MAX_JITTER / 80; ++i) {
    m_packets[i - 1].m_next = &m_packets[i];
  }
  m_packets[MAX_JITTER / 80 - 1].m_next = NULL;
}

Packet *PacketAllocator::alloc(const ShortSample *data, unsigned int size, unsigned int ts)
{
  if (m_free_packets == NULL)
    return NULL;
  Packet *retval = m_free_packets;
  m_free_packets = retval->m_next;

  retval->init(data, size, ts);

  retval->m_next = retval->m_prev = NULL;
  return retval;
}

void PacketAllocator::free(Packet *p)
{
  p->m_prev = NULL;
  p->m_next = m_free_packets;
  m_free_packets = p;
}

AmJitterBuffer::AmJitterBuffer()
  : m_head(NULL), m_tail(NULL), m_tsInited(false), m_lastTs(0),
    m_lastResyncTs(0),m_lastAudioTs(0),m_tsDelta(0),m_tsDeltaInited(false),
    m_delayCount(0), m_jitter(INITIAL_JITTER), m_forceResync(false)
{
}

void AmJitterBuffer::put(const ShortSample *data, unsigned int size, unsigned int ts, bool begin_talk)
{
  m_mutex.lock();
  if (begin_talk)
    m_forceResync = true;
  if (m_tsInited && !m_forceResync && ts_less()(m_lastTs + m_jitter, ts))
    {
      unsigned int delay = ts - m_lastTs;
      if (delay > m_jitter && m_jitter < MAX_JITTER)
       	{
	  m_jitter += (delay - m_jitter) / 2;
	  if (m_jitter > MAX_JITTER)
	    m_jitter = MAX_JITTER;
#ifdef DEBUG_PLAYOUTBUF
	  DBG("Jitter buffer delay increased to %u\n", m_jitter);
#endif
	}
      // Packet arrived too late to be put into buffer
      if (ts_less()(ts + m_jitter, m_lastTs)) {
	m_mutex.unlock();
	return;
      }
    }
  Packet *elem = m_allocator.alloc(data, size, ts);
  if (elem == NULL) {
    elem = m_head;
    m_head = m_head->m_next;
    m_head->m_prev = NULL;
    elem->init(data, size, ts);
  }
  if (m_tail == NULL)
    {
      m_tail = m_head = elem;
      elem->m_next = elem->m_prev = NULL;
    }
  else {
    if (*m_tail < *elem) // elem is later than tail - put it in tail
      {
	m_tail->m_next = elem;
	elem->m_prev = m_tail;
	m_tail = elem;
	elem->m_next = NULL;
      }
    else { // elem is out of order - place it properly
      Packet *i;
      for (i = m_tail; i->m_prev && *elem < *(i->m_prev); i = i->m_prev);
      elem->m_prev = i->m_prev;
      if (i->m_prev)
	i->m_prev->m_next = elem;
      else
	m_head = elem;
      i->m_prev = elem;
      elem->m_next = i;
    }
  }
  if (!m_tsInited) {
    m_lastTs = ts;
    m_tsInited = true;
  }
  else if (ts_less()(m_lastTs, ts) || m_forceResync) {
    m_lastTs = ts;
  }

  m_mutex.unlock();
}

/**
 * This method will return from zero to several packets.
 * To get all the packets for the single ts the caller must call this
 * method with the same ts till the return value will become false.
 */
bool AmJitterBuffer::get(unsigned int ts, unsigned int ms, ShortSample *out_buf, 
			 unsigned int *out_size, unsigned int *out_ts)
{
  bool retval = true;

  m_mutex.lock();
  if (!m_tsInited) {
    m_mutex.unlock();
    return false;
  }
  if (!m_tsDeltaInited || m_forceResync) {
    m_tsDelta = m_lastTs - ts + ms;
    m_tsDeltaInited = true;
    m_lastAudioTs = ts;
    m_forceResync = false;
#ifdef DEBUG_PLAYOUTBUF
    DBG("Jitter buffer: initialized tsDelta with %u\n", m_tsDelta);
    m_tsDeltaStart = m_tsDelta;
#endif
  }
  else if (m_lastAudioTs != ts && m_lastResyncTs != m_lastTs) {
    if (ts_less()(ts + m_tsDelta, m_lastTs)) {
      /* 
       * New packet arrived earlier than expected -
       *  immediate resync required 
       */
      m_tsDelta += m_lastTs - ts + ms;
#ifdef DEBUG_PLAYOUTBUF
      DBG("Jitter buffer resynced forward (-> %d rel)\n", 
	  m_tsDelta - m_tsDeltaStart);
#endif
      m_delayCount = 0;
    } else if (ts_less()(m_lastTs, ts + m_tsDelta - m_jitter / 2)) {
      /* New packet hasn't arrived yet */
      if (m_delayCount > RESYNC_THRESHOLD) {
	unsigned int d = m_tsDelta -(m_lastTs - ts + ms);
	m_tsDelta -= d / 2;
#ifdef DEBUG_PLAYOUTBUF
	DBG("Jitter buffer resynced backward (-> %d rel)\n", 
	    m_tsDelta - m_tsDeltaStart);
#endif
      }
      else
	++m_delayCount;
    }
    else {
      /* New packet arrived at proper time */
      m_delayCount = 0;
    }
    m_lastResyncTs = m_lastTs;
  }
  m_lastAudioTs = ts;
  unsigned int get_ts = ts + m_tsDelta - m_jitter;
  //    DBG("Getting pkt at %u, res ts = %u\n", get_ts / m_frameSize, p.timestamp);
  // First of all throw away all too old packets from the head
  Packet *tmp;
  for (tmp = m_head; tmp && ts_less()(tmp->ts() + tmp->size(), get_ts); )
    {
      m_head = tmp->m_next;
      if (m_head == NULL)
	m_tail = NULL;
      else
	m_head->m_prev = NULL;
      m_allocator.free(tmp);
      tmp = m_head;
    }
  // Get the packet from the head
  if (m_head && ts_less()(m_head->ts(), get_ts + ms))
    {
      tmp = m_head;
      m_head = tmp->m_next;
      if (m_head == NULL)
	m_tail = NULL;
      else
	m_head->m_prev = NULL;
      memcpy(out_buf, tmp->data(), PCM16_S2B(tmp->size()));
      // Map RTP timestamp to internal audio timestamp
      *out_ts = tmp->ts() - m_tsDelta + m_jitter;
      *out_size = tmp->size();
      m_allocator.free(tmp);
    }
  else
    retval = false;

  m_mutex.unlock();
  return retval;
}
