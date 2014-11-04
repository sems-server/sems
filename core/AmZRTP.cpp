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
#define SEMS_CLIENT_ID "SEMS"

int AmZRTP::zrtp_cache_save_cntr = 0;
AmMutex AmZRTP::zrtp_cache_mut;

zrtp_global_t* AmZRTP::zrtp_global;      // persistent storage for libzrtp data
zrtp_config_t AmZRTP::zrtp_config;
zrtp_zid_t AmZRTP::zrtp_instance_zid = {"defaultsems"};

void zrtp_log(int level, char *data, int len, int offset) {
  int sems_lvl = L_DBG;
  if (level==2)
    sems_lvl = L_WARN; // ??
  else if (level==1)
    sems_lvl = L_INFO; // ??
  
  if (sems_lvl==L_DBG && !AmConfig::enable_zrtp_debuglog)
    return;

  _LOG(sems_lvl, "%.*s", len, data);
}

int AmZRTP::init() {
  zrtp_log_set_log_engine(zrtp_log);

  AmConfigReader cfg;
  string cfgname=add2path(AmConfig::ModConfigPath, 1,  "zrtp.conf");
  if(cfg.loadFile(cfgname)) {
    ERROR("No %s config file present.\n", cfgname.c_str());
    return -1;
  }

  cache_path = cfg.getParameter("cache_path");
  if (cfg.hasParameter("zid_hex")) {
    string zid_hex = cfg.getParameter("zid_hex");
    if (zid_hex.size() != 2*sizeof(zrtp_instance_zid)) {
      ERROR("zid_hex config parameter in zrtp.conf must be %lu characters long.\n", 
	    sizeof(zrtp_zid_t)*2);
      return -1;
    }

    for (size_t i=0;i<sizeof(zrtp_instance_zid);i++) {
      unsigned int h;
      if (reverse_hex2int(zid_hex.substr(i*2, 2), h)) {
	ERROR("in zid_hex in zrtp.conf: '%s' is no hex number\n", zid_hex.substr(i*2, 2).c_str());
	return -1;
      }

      zrtp_instance_zid[i]=h % 0xff;
    }

  } else if (cfg.hasParameter("zid")) {
    string zid = cfg.getParameter("zid");
    WARN("zid parameter in zrtp.conf is only supported for backwards compatibility. Please use zid_hex\n");
    if (zid.length() != sizeof(zrtp_zid_t)) {
      ERROR("zid config parameter in zrtp.conf must be %lu characters long.\n", 
	    sizeof(zrtp_zid_t));
      return -1;
    }
    for (size_t i=0;i<zid.length();i++)
      zrtp_instance_zid[i]=zid[i];
  } else {
    // generate one
    string zid_hex;
    for (size_t i=0;i<sizeof(zrtp_instance_zid);i++) {
      zrtp_instance_zid[i]=get_random() % 0xff;
      zid_hex+=char2hex(zrtp_instance_zid[i], true);
    }

    WARN("Generated random ZID. To support key continuity through key cache "
	 "on the peers, add this to zrtp.conf: 'zid_hex=\"%s\"'", zid_hex.c_str());
  }


  DBG("initializing ZRTP library with cache path '%s'.\n", cache_path.c_str());

  zrtp_config_defaults(&zrtp_config);

  strcpy(zrtp_config.client_id, SEMS_CLIENT_ID);
  memcpy((char*)zrtp_config.zid, (char*)zrtp_instance_zid, sizeof(zrtp_zid_t));
  zrtp_config.lic_mode = ZRTP_LICENSE_MODE_UNLIMITED;
  
  strncpy(zrtp_config.cache_file_cfg.cache_path, cache_path.c_str(), 256);

  zrtp_config.cb.misc_cb.on_send_packet           = AmZRTP::on_send_packet;
  zrtp_config.cb.event_cb.on_zrtp_secure          = AmZRTP::on_zrtp_secure;
  zrtp_config.cb.event_cb.on_zrtp_security_event  = AmZRTP::on_zrtp_security_event;
  zrtp_config.cb.event_cb.on_zrtp_protocol_event  = AmZRTP::on_zrtp_protocol_event;

  if ( zrtp_status_ok != zrtp_init(&zrtp_config, &zrtp_global) ) {
    ERROR("Error during ZRTP initialization\n");
    return -1;
  }

  size_t rand_bytes = cfg.getParameterInt("random_entropy_bytes", 172);
  if (rand_bytes) {
    INFO("adding %zd bytes entropy from /dev/random to ZRTP entropy pool\n", rand_bytes);
    FILE* fd = fopen("/dev/random", "r");
    if (!fd) {
      ERROR("opening /dev/random for adding entropy to the pool\n");
      return -1;
    }
    void* p = malloc(rand_bytes);
    if (p==NULL)
      return -1;

    size_t read_bytes = fread(p, 1, rand_bytes, fd);
    if (read_bytes != rand_bytes) {
      ERROR("reading %zd bytes from /dev/random\n", rand_bytes);
      return -1;
    }
    zrtp_entropy_add(zrtp_global, (const unsigned char*)p, read_bytes);
    free(p);
  }


  // zrtp_add_entropy(zrtp_global, NULL, 0); // fixme
  DBG("ZRTP initialized ok.\n");

  return 0;
}

AmZRTPSessionState::AmZRTPSessionState()
  : zrtp_session(NULL), zrtp_audio(NULL)
{
  // copy default profile
  zrtp_profile_defaults(&zrtp_profile, AmZRTP::zrtp_global);
}

int AmZRTPSessionState::initSession(AmSession* session) {
  DBG("Initializing ZRTP stream...\n");

  // Allocate zrtp session
  zrtp_status_t status =
    zrtp_session_init( AmZRTP::zrtp_global,
		       &zrtp_profile,
		       ZRTP_SIGNALING_ROLE_UNKNOWN, // fixme
		       &zrtp_session);
  if (zrtp_status_ok != status) {
    // Check error code and debug logs
    return status;
  }

  // Set call-back pointer to our parent structure
  zrtp_session_set_userdata(zrtp_session, session);

  // Attach audio stream
  status = zrtp_stream_attach(zrtp_session, &zrtp_audio);
  if (zrtp_status_ok != status) {
    // Check error code and debug logs
    return status;
  }
  zrtp_stream_set_userdata(zrtp_audio, session);
  return 0;
}

int AmZRTPSessionState::startStreams(uint32_t ssrc){
  if (NULL == zrtp_audio)
    return -1;

  zrtp_status_t status = zrtp_stream_start(zrtp_audio, ssrc);
  if (zrtp_status_ok != status) {
    ERROR("starting ZRTP stream\n");
    return -1;
  }
  return 0;
}

int AmZRTPSessionState::stopStreams(){
  if (NULL == zrtp_audio)
    return -1;

  zrtp_status_t status = zrtp_stream_stop(zrtp_audio);
  if (zrtp_status_ok != status) {
    ERROR("stopping ZRTP stream\n");
    return -1;
  }
  return 0;
}

void AmZRTPSessionState::freeSession() {
  if (NULL == zrtp_session)
    return;

  zrtp_session_down(zrtp_session);

  // // save zrtp cache
  // zrtp_cache_mut.lock();
  // if (!((++zrtp_cache_save_cntr) % ZRTP_CACHE_SAVE_INTERVAL)) {
  //   if (zrtp_cache_user_down() != zrtp_status_ok) {
  //     ERROR("while writing ZRTP cache.\n");
  //   }
  // }
  // zrtp_cache_mut.unlock();
}

AmZRTPSessionState::~AmZRTPSessionState() {

}

// void zrtp_get_cache_path(char *path, uint32_t length) {
// }

int AmZRTP::on_send_packet(const zrtp_stream_t *stream, char *packet, unsigned int length) {
  DBG("on_send_packet(stream [%p], len=%u)\n", stream, length);
  if (NULL==stream) {
    ERROR("on_send_packet without stream context.\n");
    return -1;
  }

  void* udata = zrtp_stream_get_userdata(stream);
  if (NULL == udata) {
    ERROR("ZRTP on_send_packet without session context.\n");
    return -1;
  }
  AmSession* sess = reinterpret_cast<AmSession*>(udata);

  return sess->RTPStream()->send_raw(packet, length);
}

void AmZRTP::on_zrtp_secure(zrtp_stream_t *stream) {
  DBG("on_zrtp_secure(stream [%p])\n", stream);

  // if (NULL==stream) {
  //   ERROR("event received without stream context.\n");
  //   return;
  // }

  // void* udata = zrtp_stream_get_userdata(stream);
  // if (NULL == udata) {
  //   ERROR("ZRTP on_send_packet without session set context.\n");
  //   return;
  // }
  // AmSession* sess = reinterpret_cast<AmSession*>(udata);

  // sess->onZrtpSecure();
}

void AmZRTP::on_zrtp_security_event(zrtp_stream_t *stream, zrtp_security_event_t event) {
  DBG("on_zrtp_security_event(stream [%p])\n", stream);
  if (NULL==stream) {
    ERROR("event received without stream context.\n");
    return;
  }
  void* udata = zrtp_stream_get_userdata(stream);
  if (NULL == udata) {
    ERROR("ZRTP on_send_packet without session set context.\n");
    return;
  }
  AmSession* sess = reinterpret_cast<AmSession*>(udata);
  sess->postEvent(new AmZRTPSecurityEvent(event, stream));
}

void AmZRTP::on_zrtp_protocol_event(zrtp_stream_t *stream, zrtp_protocol_event_t event) {
  DBG("on_zrtp_protocol_event(stream [%p])\n", stream);
  if (NULL==stream) {
    ERROR("event received without stream context.\n");
    return;
  }
  void* udata = zrtp_stream_get_userdata(stream);
  if (NULL == udata) {
    ERROR("ZRTP on_send_packet without session set context.\n");
    return;
  }
  AmSession* sess = reinterpret_cast<AmSession*>(udata);
  sess->postEvent(new AmZRTPProtocolEvent(event, stream));
}

const char* zrtp_protocol_event_desc(zrtp_protocol_event_t e) {
  switch (e) {
  case ZRTP_EVENT_UNSUPPORTED: return "ZRTP_EVENT_UNSUPPORTED";

  case ZRTP_EVENT_IS_CLEAR: return "ZRTP_EVENT_IS_CLEAR";
  case ZRTP_EVENT_IS_INITIATINGSECURE: return "ZRTP_EVENT_IS_INITIATINGSECURE";
  case ZRTP_EVENT_IS_PENDINGSECURE: return "ZRTP_EVENT_IS_PENDINGSECURE";
  case ZRTP_EVENT_IS_PENDINGCLEAR: return "ZRTP_EVENT_IS_PENDINGCLEAR";
  case ZRTP_EVENT_NO_ZRTP: return "ZRTP_EVENT_NO_ZRTP";
  case ZRTP_EVENT_NO_ZRTP_QUICK: return "ZRTP_EVENT_NO_ZRTP_QUICK";
  case ZRTP_EVENT_IS_CLIENT_ENROLLMENT: return "ZRTP_EVENT_IS_CLIENT_ENROLLMENT";
  case ZRTP_EVENT_NEW_USER_ENROLLED: return "ZRTP_EVENT_NEW_USER_ENROLLED";
  case ZRTP_EVENT_USER_ALREADY_ENROLLED: return "ZRTP_EVENT_USER_ALREADY_ENROLLED";
  case ZRTP_EVENT_USER_UNENROLLED: return "ZRTP_EVENT_USER_UNENROLLED";
  case ZRTP_EVENT_LOCAL_SAS_UPDATED: return "ZRTP_EVENT_LOCAL_SAS_UPDATED";
  case ZRTP_EVENT_REMOTE_SAS_UPDATED: return "ZRTP_EVENT_REMOTE_SAS_UPDATED";
  case ZRTP_EVENT_IS_SECURE: return "ZRTP_EVENT_IS_SECURE";
  case ZRTP_EVENT_IS_SECURE_DONE: return "ZRTP_EVENT_IS_SECURE_DONE";
  case ZRTP_EVENT_IS_PASSIVE_RESTRICTION: return "ZRTP_EVENT_IS_PASSIVE_RESTRICTION";
  case ZRTP_EVENT_COUNT: return "ZRTP_EVENT_COUNT"; // ?
  default:    return "UNKNOWN_ZRTP_PROTOCOL_EVENT";
  }
};

const char* zrtp_security_event_desc(zrtp_security_event_t e) {
  switch (e) {
  case ZRTP_EVENT_PROTOCOL_ERROR: return "ZRTP_EVENT_PROTOCOL_ERROR";
  case ZRTP_EVENT_WRONG_SIGNALING_HASH: return "ZRTP_EVENT_WRONG_SIGNALING_HASH";
  case ZRTP_EVENT_WRONG_MESSAGE_HMAC: return "ZRTP_EVENT_WRONG_MESSAGE_HMAC";
  case ZRTP_EVENT_MITM_WARNING: return "ZRTP_EVENT_MITM_WARNING";
  default:    return "UNKNOWN_ZRTP_SECURITY_EVENT";
  }
}

#endif
