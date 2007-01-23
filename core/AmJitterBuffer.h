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

#ifndef _AmJitterBuffer_h_
#define _AmJitterBuffer_h_

#include "AmThread.h"
#include "AmRtpPacket.h"

class AmRtpStream;

#define INITIAL_JITTER	    640 // 80 miliseconds
#define MAX_JITTER	    16000 // 2 seconds
#define RESYNC_THRESHOLD    10

class Packet {
public:
    AmRtpPacket m_packet;
    Packet *m_next;
    Packet *m_prev;

    bool operator < (const Packet&) const;
};

class PacketAllocator
{
private:
    Packet m_packets[MAX_JITTER / 80];
    Packet *m_free_packets;

public:
    PacketAllocator();
    Packet *alloc(const AmRtpPacket *);
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
    AmRtpStream *m_owner;

public:
    AmJitterBuffer(AmRtpStream *owner);
    void put(const AmRtpPacket *);
    bool get(AmRtpPacket &, unsigned int ts, unsigned int ms);
};

#endif // _AmJitterBuffer_h_
