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

#ifndef _RtmpSession_h_
#define _RtmpSession_h_

#include "Rtmp.h"
#include "AmSession.h"

#include "librtmp/rtmp.h"

class RtmpSessionEvent
  : public AmEvent
{
public:
  enum EvType {
    Disconnect,
    Accept
  };

  RtmpSessionEvent(EvType t)
    : AmEvent((int)t) {}

  EvType getEvType() { return (EvType)event_id; }
};

class RtmpAudio;
class RtmpConnection;

class RtmpSession 
  : public AmSession
{
  RtmpAudio*      rtmp_audio;

  RtmpConnection* rtmp_connection;
  AmMutex         m_rtmp_conn;

private:
  void sendCallState();
  void clearConnection();

public:
  RtmpSession(RtmpConnection* c);
  ~RtmpSession();

  // @see AmSession
  void onSessionStart();
  void onBye(const AmSipRequest& req);
  void onBeforeDestroy();
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);

  void onInvite(const AmSipRequest& req);

  // @see AmEventHandler
  void process(AmEvent*);

  // forwards the packet the RtmpAudio
  void bufferPacket(const RTMPPacket& p);

  // sets the connection pointer
  void setConnectionPtr(RtmpConnection* c);

  // sets the outgoing stream ID for RTMP audio packets
  void setPlayStreamID(unsigned int stream_id);

  // Sends the disconnect event to the session to terminate it
  void disconnect();

  // Sends the accept event to the session
  void accept();
};

#endif
