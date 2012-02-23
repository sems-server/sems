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

#ifndef _RtmpAudio_h_
#define _RtmpAudio_h_

#include "AmAudio.h"
#include "AmPlayoutBuffer.h"
#include "AmRtpAudio.h"

#include "librtmp/rtmp.h"
#include <speex/speex.h>

#include <queue>
using std::queue;

class RtmpSender;

class RtmpAudio
  : public AmAudio,
    public AmPLCBuffer
{
  struct SpeexState {
    void *state;
    SpeexBits bits;
  };

  RtmpSender* sender;
  AmMutex     m_sender;

  queue<RTMPPacket> q_recv;
  AmMutex           m_q_recv;

  AmAdaptivePlayout  playout_buffer;
  unsigned int       play_stream_id;

  bool         recv_offset_i;
  unsigned int recv_rtp_offset;
  unsigned int recv_rtmp_offset;

  bool         send_offset_i;
  unsigned int send_rtmp_offset;

  SpeexState dec_state;
  SpeexState enc_state;

  void init_codec();
  int wb_decode(unsigned int size);
  int wb_encode(unsigned int size);

  void process_recv_queue(unsigned int ref_ts);

public:
  RtmpAudio(RtmpSender* s);
  ~RtmpAudio();

  /* @see AmAudio */
  int get(unsigned int user_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int nb_samples);
  int put(unsigned int user_ts, unsigned char* buffer, 
	  int output_sample_rate, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
  int write(unsigned int user_ts, unsigned int size);

  void bufferPacket(const RTMPPacket& p);

  /* @see AmPLCBuffer */
  void add_to_history(int16_t *, unsigned int);
  unsigned int conceal_loss(unsigned int, unsigned char *);

  /* 
   * Called by RtmpSession when 
   * the connection has been released 
   * or changed.
   */
  void setSenderPtr(RtmpSender* s);

  /* 
   * Called by RtmpSession when 
   * the client has called the play 
   * method to propagate the stream ID.
   */
  void setPlayStreamID(unsigned int stream_id);
};

#endif
