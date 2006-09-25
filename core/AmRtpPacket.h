/*
 * $Id: AmRtpPacket.h,v 1.6.2.1 2005/03/01 17:20:08 rco Exp $
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
#ifndef _AmRtpPacket_h_
#define _AmRtpPacket_h_

#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>

class AmRtpPacketTracer;

/** \brief RTP packet implementation */
class AmRtpPacket {

    unsigned char  buffer[4096];
    unsigned int   b_size;

    unsigned char* data;
    unsigned int   d_size;

public:
    unsigned char  payload;
    bool           marker;
    unsigned short sequence;    
    unsigned int   timestamp;   
    unsigned int   ssrc;

    struct timeval recv_time;

#ifdef SUPPORT_IPV6
    struct sockaddr_storage addr;
#else
    struct sockaddr_in addr;
#endif
    

    AmRtpPacket();
    ~AmRtpPacket();

#ifdef SUPPORT_IPV6
    void setAddr(struct sockaddr_storage* a);
    void getAddr(struct sockaddr_storage* a);
#else
    void setAddr(struct sockaddr_in* a);
    void getAddr(struct sockaddr_in* a);
#endif

    // returns -1 if error, else 0
    int compile(unsigned char* data_buf, unsigned int size);
    int send(int sd);
    int recv(int sd);

    int parse();

    unsigned int   getDataSize() { return d_size; }
    unsigned char* getData() { return data; }

    void copy(const AmRtpPacket* p);

    friend class AmRtpPacketTracer;
};

#endif

// Local Variables:
// mode:C++
// End:

