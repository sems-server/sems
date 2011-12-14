/*
 * Copyright (C) 2010 Stefan Sayer
 * Copyright (C) 2011 Raphael Coeffic
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

#include "AmDtmfSender.h"
#include "AmRtpStream.h"

#include "rtp/telephone_event.h"

AmDtmfSender::AmDtmfSender()
  : sending_state(DTMF_SEND_NONE)
{
}

/** Add a DTMF event to the send queue */
void AmDtmfSender::queueEvent(int event, unsigned int duration_ms, unsigned int sample_rate)
{
  send_queue_mut.lock();
  send_queue.push(std::make_pair(event, duration_ms * sample_rate / 1000));
  send_queue_mut.unlock();
  DBG("enqueued DTMF event %i duration %u\n", event, duration_ms);
}

/** Processes the send queue according to the timestamp */
void AmDtmfSender::sendPacket(unsigned int ts, unsigned int remote_pt, AmRtpStream* stream)
{
  while (true) {
    switch(sending_state) {
    case DTMF_SEND_NONE: {
      send_queue_mut.lock();
      if (send_queue.empty()) {
	send_queue_mut.unlock();
	return;
      }
      current_send_dtmf = send_queue.front();
      current_send_dtmf_ts = ts;
      send_queue.pop();
      send_queue_mut.unlock();
      sending_state = DTMF_SEND_SENDING;
      current_send_dtmf_ts = ts;
      DBG("starting to send DTMF\n");
    } break;
      
    case DTMF_SEND_SENDING: {
      if (ts_less()(ts, current_send_dtmf_ts + current_send_dtmf.second)) {
	// send packet
	//if (!remote_telephone_event_pt.get())
	//return;

	dtmf_payload_t dtmf;
	dtmf.event = current_send_dtmf.first;
	dtmf.e = dtmf.r = 0;
	dtmf.duration = htons(ts - current_send_dtmf_ts);
	dtmf.volume = 20;

	DBG("sending DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u\n",
	    dtmf.event,dtmf.e,dtmf.r,dtmf.volume,ntohs(dtmf.duration),current_send_dtmf_ts);

	stream->compile_and_send(remote_pt, dtmf.duration == 0, 
				 current_send_dtmf_ts, 
				 (unsigned char*)&dtmf, sizeof(dtmf_payload_t)); 
	return;

      } else {
	DBG("ending DTMF\n");
	sending_state = DTMF_SEND_ENDING;
	send_dtmf_end_repeat = 0;
      }
    } break;
      
    case DTMF_SEND_ENDING:  {
      if (send_dtmf_end_repeat >= 3) {
	DBG("DTMF send complete\n");
	sending_state = DTMF_SEND_NONE;
      } else {
	send_dtmf_end_repeat++;
	// send packet with end bit set, duration = event duration
	//if (!remote_telephone_event_pt.get())
	//return;

	dtmf_payload_t dtmf;
	dtmf.event = current_send_dtmf.first;
	dtmf.e = 1; 
	dtmf.r = 0;
	dtmf.duration = htons(current_send_dtmf.second);
	dtmf.volume = 20;

	DBG("sending DTMF: event=%i; e=%i; r=%i; volume=%i; duration=%i; ts=%u\n",
	    dtmf.event,dtmf.e,dtmf.r,dtmf.volume,ntohs(dtmf.duration),current_send_dtmf_ts);

	stream->compile_and_send(remote_pt, false, current_send_dtmf_ts, 
				 (unsigned char*)&dtmf, sizeof(dtmf_payload_t)); 
	return;
      }
    } break;
    };
  }
}

