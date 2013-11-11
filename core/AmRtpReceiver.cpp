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
  // libevent event base
  ev_base = event_base_new();
}

AmRtpReceiverThread::~AmRtpReceiverThread()
{
  event_base_free(ev_base);
  INFO("RTP receiver has been recycled.\n");
}

void AmRtpReceiverThread::on_stop()
{
  INFO("requesting RTP receiver to stop.\n");
  event_base_loopbreak(ev_base);
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
  // fake event to prevent the event loop from exiting
  int fake_fds[2];
  pipe(fake_fds);
  struct event* ev_default =
    event_new(ev_base,fake_fds[0],
	      EV_READ|EV_PERSIST,
	      NULL,NULL);
  event_add(ev_default,NULL);

  // run the event loop
  event_base_loop(ev_base,0);

  // clean-up fake fds/event
  event_free(ev_default);
  close(fake_fds[0]);
  close(fake_fds[1]);
}

void AmRtpReceiverThread::_rtp_receiver_read_cb(evutil_socket_t sd, 
						short what, void* arg)
{
  AmRtpReceiverThread::StreamInfo* p_si =
    static_cast<AmRtpReceiverThread::StreamInfo*>(arg);

  p_si->thread->streams_mut.lock();
  if(!p_si->stream) {
    // we are about to get removed...
    p_si->thread->streams_mut.unlock();
    return;
  }
  p_si->stream->recvPacket(sd);
  p_si->thread->streams_mut.unlock();
}

void AmRtpReceiverThread::addStream(int sd, AmRtpStream* stream)
{
  streams_mut.lock();
  if(streams.find(sd) != streams.end()) {
    ERROR("trying to insert existing stream [%p] with sd=%i\n",
	  stream,sd);
    streams_mut.unlock();
    return;
  }

  StreamInfo& si = streams[sd];
  si.stream = stream;
  event* ev_read = event_new(ev_base,sd,EV_READ|EV_PERSIST,
			     AmRtpReceiverThread::_rtp_receiver_read_cb,&si);
  si.ev_read = ev_read;
  si.thread = this;
  streams_mut.unlock();

  // This must be done when 
  // streams_mut is NOT locked
  event_add(ev_read,NULL);
}

void AmRtpReceiverThread::removeStream(int sd)
{
  streams_mut.lock();
  Streams::iterator sit = streams.find(sd);
  if(sit == streams.end()) {
    streams_mut.unlock();
    return;
  }

  StreamInfo& si = sit->second;
  if(!si.stream || !si.ev_read){
    streams_mut.unlock();
    return;
  }

  si.stream = NULL;
  event* ev_read = si.ev_read;
  si.ev_read = NULL;

  streams_mut.unlock();

  // This must be done while
  // streams_mut is NOT locked
  event_free(ev_read);

  streams_mut.lock();
  // this must be done AFTER event_free()
  // so that the StreamInfo does not get
  // deleted while in recvPaket()
  // (see recv callback)
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
