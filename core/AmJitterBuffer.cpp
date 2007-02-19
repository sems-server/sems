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

#include "AmRtpStream.h"
#include "AmJitterBuffer.h"
#include "AmRtpPacket.h"
#include "log.h"
#include "SampleArray.h"

bool Packet::operator < (const Packet& p) const
{
    return ts_less()(m_packet.timestamp, p.m_packet.timestamp);
}

PacketAllocator::PacketAllocator()
{
    m_free_packets = m_packets;
    for (int i = 1; i < MAX_JITTER / 80; ++i) {
	m_packets[i - 1].m_next = &m_packets[i];
    }
    m_packets[MAX_JITTER / 80 - 1].m_next = NULL;
}

Packet *PacketAllocator::alloc(const AmRtpPacket *p)
{
    if (m_free_packets == NULL)
	return NULL;
    Packet *retval = m_free_packets;
    m_free_packets = retval->m_next;
    memcpy(&retval->m_packet, p, sizeof(*p));
    retval->m_next = retval->m_prev = NULL;
    return retval;
}

void PacketAllocator::free(Packet *p)
{
    p->m_prev = NULL;
    p->m_next = m_free_packets;
    m_free_packets = p;
}

AmJitterBuffer::AmJitterBuffer(AmRtpStream *owner)
    : m_tsInited(false), m_tsDeltaInited(false), m_delayCount(0),
      m_jitter(INITIAL_JITTER), m_owner(owner),
	  m_tail(NULL), m_head(NULL), m_forceResync(false)
{
}

void AmJitterBuffer::put(const AmRtpPacket *p)
{
    m_mutex.lock();
    if (p->marker)
	m_forceResync = true;
    if (m_tsInited && !m_forceResync && ts_less()(m_lastTs + m_jitter, p->timestamp))
    {
	unsigned int delay = p->timestamp - m_lastTs;
	if (delay > m_jitter && m_jitter < MAX_JITTER)
       	{
	    m_jitter += (delay - m_jitter) / 2;
	    if (m_jitter > MAX_JITTER)
		m_jitter = MAX_JITTER;
	    // DBG("Jitter buffer delay increased to %u\n", m_jitter);
	}
	// Packet arrived too late to be put into buffer
	if (ts_less()(p->timestamp + m_jitter, m_lastTs)) {
	    m_mutex.unlock();
	    return;
	}
    }
    Packet *elem = m_allocator.alloc(p);
    if (elem == NULL) {
	elem = m_head;
	m_head = m_head->m_next;
	m_head->m_prev = NULL;
	memcpy(&elem->m_packet, p, sizeof(*p));
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
	m_lastTs = p->timestamp;
	m_tsInited = true;
    }
    else if (ts_less()(m_lastTs, p->timestamp) || m_forceResync) {
	m_lastTs = p->timestamp;
    }

    m_mutex.unlock();
}

/**
 * This method will return from zero to several packets.
 * To get all the packets for the single ts the caller must call this
 * method with the same ts till the return value will become false.
 */
bool AmJitterBuffer::get(AmRtpPacket& p, unsigned int ts, unsigned int ms)
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
	m_forceResync = true;
    }
    else if (m_lastAudioTs != ts && m_lastResyncTs != m_lastTs) {
	if (ts_less()(ts + m_tsDelta, m_lastTs)) {
	    /* 
	     * New packet arrived earlier than expected -
	     *  immediate resync required 
	     */
	    m_tsDelta += m_lastTs - ts + ms;
		//		DBG("Jitter buffer resynced forward (-> %u)\n", m_tsDelta);
	    m_delayCount = 0;
	}
	else if (ts_less()(m_lastTs, ts + m_tsDelta - m_jitter / 2)) {
	    /* New packet hasn't arrived yet */
	    if (m_delayCount > RESYNC_THRESHOLD) {
		unsigned int d = m_tsDelta -(m_lastTs - ts + ms);
		m_tsDelta -= d / 2;
		//		DBG("Jitter buffer resynced backward (-> %u)\n", m_tsDelta);
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
    for (tmp = m_head; tmp && ts_less()(tmp->m_packet.timestamp + m_owner->bytes2samples(tmp->m_packet.getDataSize()), get_ts); )
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
    if (m_head && ts_less()(m_head->m_packet.timestamp, get_ts + ms))
    {
	tmp = m_head;
	m_head = tmp->m_next;
	if (m_head == NULL)
	    m_tail = NULL;
	else
	    m_head->m_prev = NULL;
	memcpy(&p, &tmp->m_packet, sizeof(p));
	// Map RTP timestamp to internal audio timestamp
	p.timestamp -= m_tsDelta - m_jitter;
	m_allocator.free(tmp);
    }
    else
	retval = false;

    m_mutex.unlock();
    return retval;
}
