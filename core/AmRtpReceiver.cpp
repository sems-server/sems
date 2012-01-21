/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmRtpReceiver.h"
#include "AmRtpStream.h"
#include "AmRtpPacket.h"
#include "log.h"
#include "AmConfig.h"

#include <errno.h>

// Not on Solaris!
#if !defined (__SVR4) && !defined (__sun)
#include <strings.h>
#endif

#include <sys/time.h>
#include <sys/poll.h>

#ifndef MAX_RTP_SESSIONS
#define MAX_RTP_SESSIONS 2048
#endif 

#define RTP_POLL_TIMEOUT 50 /*50 ms*/


// AmRtpReceiver* AmRtpReceiver::_instance=0;

// AmRtpReceiver* AmRtpReceiver::instance()
// {
//   if(!_instance)
//     _instance = new AmRtpReceiver();

//   return _instance;
// }

// bool AmRtpReceiver::haveInstance() {
//   return NULL != _instance;
// }

_AmRtpReceiver::_AmRtpReceiver()
{
  n_receivers = AmConfig::RTPReceiverThreads;
  receivers = new AmRtpReceiverThread[n_receivers];
}

_AmRtpReceiver::~_AmRtpReceiver()
{
  delete [] receivers;
}

AmRtpReceiverThread::AmRtpReceiverThread()
  : stop_requested(false)
{
  fds  = new struct pollfd[MAX_RTP_SESSIONS];
  nfds = 0;
}

AmRtpReceiverThread::~AmRtpReceiverThread()
{
  delete [] (fds);
  INFO("RTP receiver has been recycled.\n");
}

void AmRtpReceiverThread::on_stop()
{
  INFO("requesting RTP receiver to stop.\n");
  stop_requested.set(true);
}

void AmRtpReceiverThread::stop_and_wait()
{
  if(!is_stopped()) {
    stop();
    
    while(!is_stopped()) 
      usleep(10000);
  }
}

void _AmRtpReceiver::dispose() 
{
  for(unsigned int i=0; i<n_receivers; i++){
    receivers[i].stop_and_wait();
  }
}

void AmRtpReceiverThread::run()
{
  unsigned int   tmp_nfds = 0;
  struct pollfd* tmp_fds  = new struct pollfd[MAX_RTP_SESSIONS];

  while(!stop_requested.get()){
	
    streams_mut.lock();
    tmp_nfds = nfds;
    memcpy(tmp_fds,fds,nfds*sizeof(struct pollfd));
    streams_mut.unlock();

    int ret = poll(tmp_fds,tmp_nfds,RTP_POLL_TIMEOUT);
    if(ret < 0)
      ERROR("AmRtpReceiver: poll: %s\n",strerror(errno));

    if(ret < 1)
      continue;

    for(unsigned int i=0; i<tmp_nfds; i++) {

      if(!(tmp_fds[i].revents & POLLIN))
	continue;

      streams_mut.lock();
      Streams::iterator it = streams.find(tmp_fds[i].fd);
      if(it != streams.end()) {
	it->second.stream->recvPacket();
      }
      streams_mut.unlock();      
    }
  }

  delete[] (tmp_fds);
}

void AmRtpReceiverThread::addStream(int sd, AmRtpStream* stream)
{
  streams_mut.lock();

  if(nfds >= MAX_RTP_SESSIONS){
    streams_mut.unlock();
    ERROR("maximum number of sessions reached (%i)\n",
	  MAX_RTP_SESSIONS);
    throw string("maximum number of sessions reached");
  }

  fds[nfds].fd      = sd;
  fds[nfds].events  = POLLIN;
  fds[nfds].revents = 0;

  streams.insert(std::make_pair(sd,StreamInfo(nfds,stream)));
  nfds++;

  streams_mut.unlock();
}

void AmRtpReceiverThread::removeStream(int sd)
{
  streams_mut.lock();

  Streams::iterator sit = streams.find(sd);
  if(sit == streams.end()) {
    streams_mut.unlock();
    return;
  }

  unsigned int i = sit->second.index;
  if(--nfds && (i < nfds)) {
    fds[i] = fds[nfds];
    sit = streams.find(fds[nfds].fd);
    if(sit != streams.end()) {
      sit->second.index = i;
    }
  }
  streams.erase(sd);

  streams_mut.unlock();
}

void _AmRtpReceiver::start()
{
  for(unsigned int i=0; i<n_receivers; i++)
    receivers[i].start();
}

void _AmRtpReceiver::addStream(int sd, AmRtpStream* stream)
{
  unsigned int i = sd % n_receivers;
  receivers[i].addStream(sd,stream);
}

void _AmRtpReceiver::removeStream(int sd)
{
  unsigned int i = sd % n_receivers;
  receivers[i].removeStream(sd);
}
