/*
 * $Id: AmRtpReceiver.cpp,v 1.1.2.1 2005/03/01 17:20:08 rco Exp $
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

#include "AmRtpReceiver.h"
#include "AmRtpStream.h"
#include "AmRtpPacket.h"
#include "log.h"

#include <errno.h>
#include <strings.h>
#include <sys/time.h>

AmRtpReceiver* AmRtpReceiver::_instance=0;

AmRtpReceiver* AmRtpReceiver::instance()
{
    if(!_instance)
	_instance = new AmRtpReceiver();

    return _instance;
}

AmRtpReceiver::AmRtpReceiver()
{
    FD_ZERO(&fds);
}

AmRtpReceiver::~AmRtpReceiver()
{
}

void AmRtpReceiver::on_stop()
{
}

void AmRtpReceiver::run()
{
    struct timeval timeout;
    fd_set         cur_set;
    int            max_fd;

    while(true){
	
	
	fds_mut.lock();
 	memcpy(&cur_set,&fds,sizeof(fd_set));
	fds_mut.unlock();

	max_fd = -1;
	timeout.tv_sec  = 0;
	timeout.tv_usec = 100000/* 100 ms */;

	streams_mut.lock();
	if(!streams.empty())
	    max_fd = streams.begin()->first;
	streams_mut.unlock();
	
	int ret = select(++max_fd,&cur_set,NULL,NULL,&timeout);
	if(ret == -1)
	    ERROR("AmRtpReceiver: select: %s\n",strerror(errno));

	if(ret < 1){
	    //DBG("select timeout max_fd+1 = %i\n",max_fd);
	    continue;
	}

	streams_mut.lock();
	for(Streams::iterator it = streams.begin();
	    it != streams.end(); ++it) {

	    if(FD_ISSET(it->first,&cur_set)){
		AmRtpPacket* p = new AmRtpPacket();
		if(p->recv(it->first) > 0){
		    
		    if(p->parse() == -1){
			ERROR("while parsing RTP packet.\n");
			delete p;
			streams_mut.unlock();
			continue;
		    }
		    
		    gettimeofday(&p->recv_time,NULL);
		    it->second->bufferPacket(p);
		}
	    }
	}
	streams_mut.unlock();
    }
}

void AmRtpReceiver::addStream(int sd, AmRtpStream* stream)
{
    streams_mut.lock();
    streams.insert(std::make_pair(sd,stream));
    streams_mut.unlock();

    fds_mut.lock();
    FD_SET(sd,&fds);
    fds_mut.unlock();
}

void AmRtpReceiver::removeStream(int sd)
{
    streams_mut.lock();
    streams.erase(sd);
    streams_mut.unlock();
    
    fds_mut.lock();
    FD_CLR(sd,&fds);
    fds_mut.unlock();
}
