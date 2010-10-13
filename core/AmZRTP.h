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

#include "zrtp/zrtp.h"
#include "AmThread.h"
#include "AmEvent.h"

#include <string>

#include "zrtp/zrtp.h"
extern zrtp_global_ctx_t zrtp_global;      // persistent storage for libzrtp data
extern zrtp_zid_t zrtp_instance_zid;

struct AmZRTPEvent : public AmEvent {
  zrtp_stream_ctx_t* stream_ctx;
 AmZRTPEvent(int event_id, zrtp_stream_ctx_t* stream_ctx)
   : AmEvent(event_id), stream_ctx(stream_ctx) { }
  ~AmZRTPEvent() { }
};

struct AmZRTP { 
   static int zrtp_cache_save_cntr;
   static std::string cache_path;
   static AmMutex zrtp_cache_mut;
   static int init();
   static zrtp_global_ctx_t zrtp_global;    
   static zrtp_zid_t zrtp_instance_zid;
   static void freeSession(zrtp_conn_ctx_t* zrtp_session);
}; 

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
