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

#ifdef WITH_ZRTP

#include "AmZRTP.h"

#include "AmSession.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include <stdlib.h>

#define ZRTP_CACHE_SAVE_INTERVAL 1

string AmZRTP::cache_path = "zrtp_cache.dat";
int AmZRTP::zrtp_cache_save_cntr = 0;
AmMutex AmZRTP::zrtp_cache_mut;

zrtp_global_ctx_t AmZRTP::zrtp_global;      // persistent storage for libzrtp data
zrtp_zid_t AmZRTP::zrtp_instance_zid = {"defaultsems"}; // todo: generate one

int AmZRTP::init() {
  AmConfigReader cfg;
  string cfgname=add2path(AmConfig::ModConfigPath, 1,  "zrtp.conf");
  if(cfg.loadFile(cfgname)) {
    ERROR("No %s config file present.\n", 
	  cfgname.c_str());
    return -1;
  }
  cache_path = cfg.getParameter("cache_path");
  string zid = cfg.getParameter("zid");
  if (zid.length() != sizeof(zrtp_zid_t)) {
    ERROR("ZID of this instance MUST be set for ZRTP.\n");
    ERROR("ZID needs to be %u characters long.\n", 
	  sizeof(zrtp_zid_t));
    return -1;
  }
  for (int i=0;i<12;i++)
    zrtp_instance_zid[i]=zid[i];

  DBG("initializing ZRTP library with ZID '%s', cache path '%s'.\n",
      zid.c_str(), cache_path.c_str());
  if ( zrtp_status_ok != zrtp_init(&zrtp_global, "zrtp_sems") ) {
    ERROR("Some error during zrtp initialization\n");
    return -1;
  }
  zrtp_add_entropy(&zrtp_global, NULL, 0);
  DBG("ZRTP initialized ok.\n");

  return 0;
}

void AmZRTP::freeSession(zrtp_conn_ctx_t* zrtp_session) {
  zrtp_done_session_ctx(zrtp_session);
  free(zrtp_session);
  // save zrtp cache
  zrtp_cache_mut.lock();
  if (!((++zrtp_cache_save_cntr) % ZRTP_CACHE_SAVE_INTERVAL)) {
    if (zrtp_cache_user_down() != zrtp_status_ok) {
      ERROR("while writing ZRTP cache.\n");
    }
  }
  zrtp_cache_mut.unlock();
}

void zrtp_get_cache_path(char *path, uint32_t length) {
  snprintf(path, length, "%s", AmZRTP::cache_path.c_str());
}


// void zrtp_get_cache_path(char *path, uint32_t length) {
// }


void zrtp_event_callback(zrtp_event_t event, zrtp_stream_ctx_t *stream_ctx)
{
  if (NULL==stream_ctx) {
    ERROR("event received without stream context.\n");
    return;
  }

  AmSession* sess = reinterpret_cast<AmSession*>(stream_ctx->stream_usr_data);
  if (NULL==sess) {
    ERROR("event received without session set up.\n");
    return;
  }

  sess->postEvent(new AmZRTPEvent(event, stream_ctx));
}

void zrtp_play_alert(zrtp_stream_ctx_t* ctx) {
  INFO("zrtp_play_alert: ALERT!\n");
  ctx->need_play_alert = zrtp_play_no;
}

int zrtp_send_rtp( const zrtp_stream_ctx_t* stream_ctx,
		   char* packet, unsigned int length) {
  if (NULL==stream_ctx) {
    ERROR("trying to send packet without stream context.\n");
    return -1;
  }

  AmSession* sess = reinterpret_cast<AmSession*>(stream_ctx->stream_usr_data);
  if (NULL==sess) {
    ERROR("trying to send packet without session set up.\n");
    return -1;
  }

  return sess->rtp_str.send_raw(packet, length);  
}


#define BUFFER_LOG_SIZE 256
void zrtp_print_log(log_level_t level, const char* format, ...)
{
	char buffer[BUFFER_LOG_SIZE];
    va_list arg;

    va_start(arg, format);
    vsnprintf(buffer, BUFFER_LOG_SIZE, format, arg);
    va_end( arg );
    int sems_lvl = L_ERR;
    switch(level) {
    case ZRTP_LOG_DEBUG:   sems_lvl = L_DBG; break;
    case ZRTP_LOG_INFO:    sems_lvl = L_INFO; break;
    case ZRTP_LOG_WARNING: sems_lvl = L_WARN; break;
    case ZRTP_LOG_ERROR:   sems_lvl = L_ERR; break;
    case ZRTP_LOG_FATAL:   sems_lvl = L_ERR; break;
    case ZRTP_LOG_ALL:   sems_lvl = L_ERR; break;
    }
    _LOG(sems_lvl, "*** %s", buffer);
}


#endif



