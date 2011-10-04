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

#ifndef _RtmpSender_h_
#define _RtmpSender_h_

#include "AmThread.h"
#include <queue>
#include <string>
using std::queue;
using std::string;

#include "librtmp/rtmp.h"

class RtmpSender
  : public AmThread
{
  // sender queue
  queue<RTMPPacket> q_send;
  AmMutex           m_q_send;
  AmCondition<bool> has_work;

  // ptr to RtmpConnection::rtmp
  RTMP* p_rtmp;

  // execution control
  AmSharedVar<bool> running;

  int SendChangeChunkSize();

protected:
  void run();
  void on_stop();

public:
  RtmpSender(RTMP* r);
  ~RtmpSender();
  
  // adds a packet to the sender queue
  int push_back(const RTMPPacket& p);

  int SendCtrl(short nType, unsigned int nObject, unsigned int nTime);
  int SendResultNumber(double txn, double ID);
  int SendConnectResult(double txn);
  int SendRegisterResult(double txn, const char* str);
  int SendErrorResult(double txn, const char* str);
  int SendPause(int DoPause, int iTime);

  int SendPlayStart();
  int SendPlayStop();
  int SendStreamBegin();
  int SendStreamEOF();

  int SendCallStatus(int status);
  int NotifyIncomingCall(const string& uri);

};

#endif
