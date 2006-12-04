/*
 * $Id: AmDtmfDetector.h,v 1.1.2.1 2005/06/01 12:00:24 rco Exp $
 *
 * Copyright (C) 2006 Sippy Software, Inc.
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

#include "AmJitterBuffer.h"
#include "AmRtpPacket.h"
#include "log.h"
#include "SampleArray.h"

#define INITIAL_JITTER	    2
#define MAX_JITTER	    200 // 2 seconds
#define RESYNC_THRESHOLD    10

template <typename T>
RingBuffer<T>::RingBuffer(unsigned int size)
    : m_buffer(new T[size]), m_size(size)
{
    memset(m_buffer, 0, sizeof(T)*size);
}

template <typename T>
void RingBuffer<T>::get(unsigned int idx, T *dest)
{
    memcpy(dest, &m_buffer[idx % m_size], sizeof(T));
}

template <typename T>
void RingBuffer<T>::put(unsigned int idx, const T *src)
{
    memcpy(&m_buffer[idx % m_size], src, sizeof(T));
}

template <typename T>
void RingBuffer<T>::clear(unsigned int idx)
{
    memset(&m_buffer[idx % m_size], 0, sizeof(T));
}

AmJitterBuffer::AmJitterBuffer(unsigned int frame_size)
    : m_tsInited(false), m_tsDeltaInited(false), m_ringBuffer(MAX_JITTER),
      m_delayCount(0), m_jitter(INITIAL_JITTER), m_frameSize(frame_size)
{
}

void AmJitterBuffer::put(const AmRtpPacket *p)
{
    m_mutex.lock();
    //m_inputBuffer[p->timestamp].copy(p);
	//    DBG("Putting pkt at %u me = %ld\n", p->timestamp / m_frameSize, (long) this);
    if (m_tsInited && ts_less()(p->timestamp + m_jitter * m_frameSize, m_lastTs)) {
	unsigned int delay = p->timestamp - m_lastTs;
	if (delay > m_jitter * m_frameSize && m_jitter < MAX_JITTER) {
	    m_jitter += (delay / m_frameSize - m_jitter) / 2 + 1;
	    if (m_jitter > MAX_JITTER)
		m_jitter = MAX_JITTER;
	}
	// Packet arrived too late to be put into buffer
	if (ts_less()(p->timestamp + m_jitter * m_frameSize, m_lastTs)) {
	    m_mutex.unlock();
	    return;
	}
    }
    m_ringBuffer.put(p->timestamp / m_frameSize, p);
    if (!m_tsInited) {
	m_lastTs = p->timestamp;
	m_tsInited = true;
    }
    else if (ts_less()(m_lastTs, p->timestamp)) {
	m_lastTs = p->timestamp;
    }

    m_mutex.unlock();
}

bool AmJitterBuffer::get(AmRtpPacket& p, unsigned int ts)
{
    bool retval = true;

    m_mutex.lock();
    if (!m_tsInited) {
	m_mutex.unlock();
	return false;
    }
    if (!m_tsDeltaInited) {
	m_tsDelta = m_lastTs - ts;
	m_tsDeltaInited = true;
    }
    else {
	unsigned int new_delta = m_lastTs - ts;
	if (ts_less()(m_tsDelta, new_delta)) {
	    /* 
	     * New packet arrived earlier than expected -
	     *  immediate resync required 
	     */
	    DBG("Jitter buffer resynced forward\n");
	    m_ringBuffer.clear((ts + m_tsDelta - m_jitter * m_frameSize) / m_frameSize);
	    ++m_tsDelta;
	    m_delayCount = 0;
	}
	else if (ts_less()(new_delta, m_tsDelta)) {
	    /* New packet hasn't arrived yet */
	    if (m_delayCount > RESYNC_THRESHOLD) {
		DBG("Jitter buffer resynced backward\n");
		--m_tsDelta;
//		m_tsDelta = new_delta;
//		m_delayCount = 0;
	    }
	    else
		++m_delayCount;
	}
	else {
	    /* New packet arrived at proper time */
	    m_delayCount = 0;
	}
    }
    unsigned int get_ts = ts + m_tsDelta - m_jitter * m_frameSize;
    m_ringBuffer.get(get_ts / m_frameSize, &p);
	//    DBG("Getting pkt at %u, res ts = %u\n", get_ts / m_frameSize, p.timestamp);
    m_ringBuffer.clear(get_ts / m_frameSize);
    if (!p.timestamp) 
	retval = false;

    m_mutex.unlock();
    return retval;
}
