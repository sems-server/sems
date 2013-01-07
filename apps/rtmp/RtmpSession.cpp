/*
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

#include "RtmpSession.h"
#include "RtmpAudio.h"
#include "RtmpConnection.h"

const unsigned int __dlg_status2rtmp_call[AmSipDialog::__max_Status]  = {
  RTMP_CALL_NOT_CONNECTED, // Disconnected
  RTMP_CALL_IN_PROGRESS, //"Trying",
  RTMP_CALL_IN_PROGRESS, //"Proceeding",
  RTMP_CALL_DISCONNECTING, //"Cancelling",
  RTMP_CALL_IN_PROGRESS, //"Early",
  RTMP_CALL_CONNECTED, //"Connected",
  RTMP_CALL_DISCONNECTING //"Disconnecting"
};

RtmpSession::RtmpSession(RtmpConnection* c)
  : AmSession(), 
    rtmp_audio(new RtmpAudio(c->getSenderPtr())),
    rtmp_connection(c)
{
}

RtmpSession::~RtmpSession()
{
  clearConnection();
  delete rtmp_audio;
}

void RtmpSession::sendCallState()
{
  m_rtmp_conn.lock();
  if(rtmp_connection){
    DBG("Dialog status: %s\n",dlg->getStatusStr());
    unsigned int rtmp_call_status = __dlg_status2rtmp_call[dlg->getStatus()];
    rtmp_connection->SendCallStatus(rtmp_call_status);
  }
  m_rtmp_conn.unlock();
}

void RtmpSession::clearConnection()
{
  m_rtmp_conn.lock();
  if(rtmp_connection){
    rtmp_connection->setSessionPtr(NULL);
    rtmp_connection = NULL;
  }
  m_rtmp_conn.unlock();
}

void RtmpSession::onBeforeDestroy()
{
  clearConnection();
  AmSession::onBeforeDestroy();
}

void RtmpSession::onSessionStart()
{
  bool start_session = true;

  m_rtmp_conn.lock();
  if(rtmp_connection)
    rtmp_connection->SendCallStatus(RTMP_CALL_CONNECT_STREAMS);
  else
    start_session = false;
  m_rtmp_conn.unlock();

  if(!start_session) {
    setStopped();
    return;
  }

  DBG("enabling adaptive buffer\n");
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
  DBG("plugging rtmp_audio into in&out\n");
  setInOut(rtmp_audio,rtmp_audio);
  
  AmSession::onSessionStart();
}

void RtmpSession::onBye(const AmSipRequest& req)
{
  sendCallState();
  AmSession::onBye(req);
}

void RtmpSession::onSipReply(const AmSipRequest& req,
			     const AmSipReply& reply, 
			     AmBasicSipDialog::Status old_dlg_status)
{
  AmSession::onSipReply(req,reply,old_dlg_status);

  sendCallState();

  if(dlg->getStatus() == AmSipDialog::Disconnected) {
    setStopped();
  }
}

void RtmpSession::onInvite(const AmSipRequest& req)
{
  DBG("status str: %s\n",dlg->getStatusStr());

  if(dlg->getStatus() != AmSipDialog::Trying){
    AmSession::onInvite(req);
    return;
  }

  //TODO: start client response timer
  m_rtmp_conn.lock();
  rtmp_connection->NotifyIncomingCall(req.user);
  m_rtmp_conn.unlock();

  dlg->reply(req,180,"Ringing");
}

void RtmpSession::process(AmEvent* ev)
{
  RtmpSessionEvent* rtmp_ev = dynamic_cast<RtmpSessionEvent*>(ev);
  if(rtmp_ev){
    switch(rtmp_ev->getEvType()){
    case RtmpSessionEvent::Disconnect:
      dlg->bye();
      setStopped();
      return;
    case RtmpSessionEvent::Accept:
      AmSipRequest* inv_req = dlg->getUASPendingInv();
      if(!inv_req){
	//Error: no pending INVITE
	sendCallState();
	return;
      }
      dlg->reply(*inv_req,200,"OK");
      sendCallState();
      return;
    }
  }

  AmSession::process(ev);
}

void RtmpSession::bufferPacket(const RTMPPacket& p)
{
  rtmp_audio->bufferPacket(p);
}

void RtmpSession::setConnectionPtr(RtmpConnection* c)
{
  //warning: this is not executed from event handler threads!!!
  m_rtmp_conn.lock();
  DBG("resetting sender ptr used by rtmp_audio (c=%p)\n",c);
  if(c){
    rtmp_audio->setSenderPtr(c->getSenderPtr());
  }
  else {
    rtmp_audio->setSenderPtr(NULL);
    disconnect();
  }
  rtmp_connection = c;
  m_rtmp_conn.unlock();
}

// sets the outgoing stream ID for RTMP audio packets
void  RtmpSession::setPlayStreamID(unsigned int stream_id)
{
  rtmp_audio->setPlayStreamID(stream_id);
}

void RtmpSession::disconnect()
{
  postEvent(new RtmpSessionEvent(RtmpSessionEvent::Disconnect));
}

void RtmpSession::accept()
{
  postEvent(new RtmpSessionEvent(RtmpSessionEvent::Accept));
}
