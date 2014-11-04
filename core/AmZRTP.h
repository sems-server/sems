/*
 * Copyright (C) 2008 iptego GmbH
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

#ifndef _AM_ZRTP_H
#define _AM_ZRTP_H

#ifdef WITH_ZRTP

#include "libzrtp/zrtp.h"
#include "AmThread.h"
#include "AmEvent.h"

#include <string>

class AmSession;

struct AmZRTPSecurityEvent : public AmEvent {
  zrtp_stream_t* stream_ctx;
 AmZRTPSecurityEvent(int event_id, zrtp_stream_t* stream_ctx)
   : AmEvent(event_id), stream_ctx(stream_ctx) { }
  ~AmZRTPSecurityEvent() { }
};

struct AmZRTPProtocolEvent : public AmEvent {
  zrtp_stream_t* stream_ctx;
 AmZRTPProtocolEvent(int event_id, zrtp_stream_t* stream_ctx)
   : AmEvent(event_id), stream_ctx(stream_ctx) { }
  ~AmZRTPProtocolEvent() { }
};


struct AmZRTP { 
  static int zrtp_cache_save_cntr;
  static std::string cache_path;
  static AmMutex zrtp_cache_mut;
  static int init();
  static zrtp_global_t* zrtp_global;
  static zrtp_config_t zrtp_config;
  static zrtp_zid_t zrtp_instance_zid;

  static int on_send_packet(const zrtp_stream_t *stream, char *packet, unsigned int length);
  static void on_zrtp_secure(zrtp_stream_t *stream);
  static void on_zrtp_security_event(zrtp_stream_t *stream, zrtp_security_event_t event);
  static void on_zrtp_protocol_event(zrtp_stream_t *stream, zrtp_protocol_event_t event);
}; 

struct AmZRTPSessionState {

  AmZRTPSessionState();
  ~AmZRTPSessionState();

  int initSession(AmSession* s);
  void freeSession();

  int startStreams(uint32_t ssrc);
  int stopStreams();

  zrtp_profile_t  zrtp_profile;
  zrtp_session_t* zrtp_session; // ZRTP session
  zrtp_stream_t*  zrtp_audio;   // ZRTP stream for audio
};

const char* zrtp_protocol_event_desc(zrtp_protocol_event_t e);
const char* zrtp_security_event_desc(zrtp_security_event_t e);

#if defined(__cplusplus)
extern "C" {
#endif

  void zrtp_get_cache_path(char *path, uint32_t length);
  zrtp_status_t zrtp_cache_user_down();

#if defined(__cplusplus)
}
#endif

#endif // WITH_ZRTP

#endif //_AM_ZRTP_H
