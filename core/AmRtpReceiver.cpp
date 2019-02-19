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
#include "AmRtpMuxStream.h"
#include "AmRtpPacket.h"
#include "log.h"
#include "AmConfig.h"

#include <errno.h>

// Not on Solaris!
#if !defined (__SVR4) && !defined (__sun)
#include <strings.h>
#endif

_AmRtpReceiver::_AmRtpReceiver()
  : mux_stream(NULL)
{
  n_receivers = AmConfig::RTPReceiverThreads;
  receivers = new AmRtpReceiverThread[n_receivers];
}

_AmRtpReceiver::~_AmRtpReceiver()
{
  if (mux_stream) {
    receivers[0].removeStream(mux_stream->getLocalSocket(), mux_stream->getLocalPort());
  }
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
  if (pipe(fake_fds)<0) {
    DBG("error creating bogus pipe\n");
  }
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

void AmRtpReceiverThread::_rtp_receiver_buf_cb(evutil_socket_t sd, 
					       short what, void* arg)
{
  AmRtpReceiverThread::RtpPacket* r_pkt =
    static_cast<AmRtpReceiverThread::RtpPacket*>(arg);

  if (NULL != r_pkt->stream) {
    r_pkt->thread->streams_mut.lock();
    if (NULL != r_pkt->stream) {
      r_pkt->stream->recvPacket(-1, r_pkt->pkt, r_pkt->len);
    }
    r_pkt->thread->streams_mut.unlock();
  }
  delete r_pkt;
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

  streams_ports[stream->getLocalPort()] = stream;
  streams_mut.unlock();

  // This must be done when 
  // streams_mut is NOT locked
  event_add(ev_read,NULL);
}

void AmRtpReceiverThread::removeStream(int sd, int local_port)
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
  streams_ports.erase(local_port);
  streams_mut.unlock();
}

int AmRtpReceiverThread::recvdPacket(bool need_lock, int local_port, unsigned char* buf, size_t len) {
  // pass packet to correct recevier thread
  AmRtpReceiverThread::RtpPacket* r_pkt = new AmRtpReceiverThread::RtpPacket();
  r_pkt->len = len;
  r_pkt->pkt = new unsigned char[len];
  memcpy(r_pkt->pkt, buf, len);

  event* ev_read = event_new(ev_base, -1 /* no fd */, 0 /* no events */,
			     AmRtpReceiverThread::_rtp_receiver_buf_cb, r_pkt);
  r_pkt->ev_read = ev_read;
  r_pkt->thread = this;

  if (need_lock)
    streams_mut.lock();

  std::map<int, AmRtpStream*>::iterator it = streams_ports.find(local_port);
  if (it != streams_ports.end())
    r_pkt->stream = it->second;
  else {
    ERROR("could not find stream for local port %i\n", local_port);
  }
  if (need_lock)
    streams_mut.unlock();
  // possibly call event_add here
  event_active(ev_read, EV_READ, 0);

  return 0;
}

void _AmRtpReceiver::start()
{
  for(unsigned int i=0; i<n_receivers; i++)
    receivers[i].start();
}

void _AmRtpReceiver::startRtpMuxReceiver()
{
  if (AmConfig::RtpMuxPort) {
    DBG("Starting RTP MUX listener on port %d\n", AmConfig::RtpMuxPort);
    mux_stream = new AmRtpMuxStream();
    mux_stream->setLocalIP(AmConfig::RtpMuxIP);
    mux_stream->setLocalPort(AmConfig::RtpMuxPort);

    receivers[0].addStream(mux_stream->getLocalSocket(), mux_stream);
    DBG("added mux_stream [%p] to RTP receiver\n", mux_stream);
  } else {
    DBG("Not starting RTP MUX listener\n");
  }
}

void _AmRtpReceiver::addStream(int sd, AmRtpStream* stream)
{
  unsigned int i = stream->getLocalPort()  % n_receivers;
  receivers[i].addStream(sd,stream);
}

void _AmRtpReceiver::removeStream(int sd, int local_port)
{
  unsigned int i = local_port % n_receivers;
  receivers[i].removeStream(sd, local_port);
}

int _AmRtpReceiver::recvdPacket(int recvd_port, int local_port, unsigned char* buf, size_t len) {
  unsigned int i = local_port % n_receivers;
  // need to lock if received on different receiver than the stream is handled by
  unsigned int c = recvd_port % n_receivers;

  return receivers[i].recvdPacket(i != c, local_port, buf, len);
}
