/*
 * Copyright (C) 2014 Stefan Sayer
 *
 * Parts of the development of this module was kindly sponsored by AMTEL Inc.
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
#include "ModZrtp.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "DSMCoreModule.h"

SC_EXPORT(MOD_CLS_NAME);


MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

#ifdef WITH_ZRTP
  DEF_CMD("zrtp.setEnabled", ZRTPSetEnabledAction);
  DEF_CMD("zrtp.setAllowclear", ZRTPSetAllowclearAction);
  DEF_CMD("zrtp.setAutosecure", ZRTPSetAutosecureAction);
  DEF_CMD("zrtp.setDisclosebit", ZRTPSetDisclosebitAction);
  DEF_CMD("zrtp.getSAS", ZRTPGetSASAction);
  DEF_CMD("zrtp.getSessionInfo", ZRTPGetSessionInfoAction);
  DEF_CMD("zrtp.setVerified", ZRTPSetVerifiedAction);
  DEF_CMD("zrtp.setUnverified", ZRTPSetUnverifiedAction);
  DEF_CMD("zrtp.setSignalingHash", ZRTPSetSignalingHash);
  DEF_CMD("zrtp.getSignalingHash", ZRTPGetSignalingHash);
#endif

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {
#ifdef WITH_ZRTP

  if (cmd == "zrtp.protocolEvent")
    return new TestDSMCondition(params, DSMCondition::ZRTPProtocolEvent);

  if (cmd == "zrtp.securityEvent")
    return new TestDSMCondition(params, DSMCondition::ZRTPSecurityEvent);

#endif
} MOD_CONDITIONEXPORT_END;

#ifdef WITH_ZRTP

EXEC_ACTION_START(ZRTPSetEnabledAction) {
  bool b = resolveVars(arg, sess, sc_sess, event_params) == DSM_TRUE;
  DBG("setting ZRTP to %sabled\n", b?"en":"dis");
  sess->enable_zrtp = b;
} EXEC_ACTION_END;

EXEC_ACTION_START(ZRTPSetAllowclearAction) {
  bool b = resolveVars(arg, sess, sc_sess, event_params) == DSM_TRUE;
  DBG("setting ZRTP allowclear %sabled\n", b?"en":"dis");
  sess->zrtp_session_state.zrtp_profile.allowclear = b;
} EXEC_ACTION_END;


EXEC_ACTION_START(ZRTPSetAutosecureAction) {
  bool b = resolveVars(arg, sess, sc_sess, event_params) == DSM_TRUE;
  DBG("setting ZRTP autosecure %sabled\n", b?"en":"dis");
  sess->zrtp_session_state.zrtp_profile.autosecure = b;
} EXEC_ACTION_END;


EXEC_ACTION_START(ZRTPSetDisclosebitAction) {
  bool b = resolveVars(arg, sess, sc_sess, event_params) == DSM_TRUE;
  DBG("setting ZRTP disclose_bit %sabled\n", b?"en":"dis");
  sess->zrtp_session_state.zrtp_profile.disclose_bit = b;
} EXEC_ACTION_END;

CONST_ACTION_2P(ZRTPGetSASAction, ',', true);
EXEC_ACTION_START(ZRTPGetSASAction) {
  string varname = par1;
  if (varname.size() && varname[0]=='$') varname = varname.substr(1);

  string sas2 = par2;
  if (sas2.size() && sas2[0]=='$') sas2 = sas2.substr(1);

  if (varname.empty()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("need variable name for zrtp.getSAS");
    EXEC_ACTION_STOP;
  }

  if (NULL == sess->zrtp_session_state.zrtp_session) {
    WARN("ZRTP not active on that session\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("ZRTP not active on that session");
    EXEC_ACTION_STOP;
  }

   zrtp_session_info_t zrtp_session_info;
   zrtp_session_get(sess->zrtp_session_state.zrtp_session, &zrtp_session_info);

   if (!zrtp_session_info.sas_is_ready) {
     sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
     sc_sess->SET_STRERROR("ZRTP SAS not ready on that session");
     EXEC_ACTION_STOP;
   }

   sc_sess->var[varname] = string(zrtp_session_info.sas1.buffer, zrtp_session_info.sas1.length);
   if (!sas2.empty())
     sc_sess->var[sas2] = string(zrtp_session_info.sas2.buffer, zrtp_session_info.sas2.length);

   DBG("got SAS1 and SAS2: <%.*s> <%.*s>\n", zrtp_session_info.sas1.length, zrtp_session_info.sas1.buffer,
       zrtp_session_info.sas2.length, zrtp_session_info.sas1.buffer);
} EXEC_ACTION_END;

EXEC_ACTION_START(ZRTPGetSessionInfoAction) {
  string varname = arg;
  if (varname.size() && varname[0]=='$') varname = varname.substr(1);

  if (varname.empty()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("need variable name for zrtp.getSessionInfo");
    EXEC_ACTION_STOP;
  }

  if (NULL == sess->zrtp_session_state.zrtp_session) {
    WARN("ZRTP not active on that session\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("ZRTP not active on that session");
    EXEC_ACTION_STOP;
  }

   zrtp_session_info_t zrtp_session_info;
   zrtp_session_get(sess->zrtp_session_state.zrtp_session, &zrtp_session_info);

   sc_sess->var[varname+".sas_is_ready"] = zrtp_session_info.sas_is_ready ? "true":"false";

   if (zrtp_session_info.sas_is_ready) {
     sc_sess->var[varname+".sas1"] = string(zrtp_session_info.sas1.buffer, zrtp_session_info.sas1.length);
     sc_sess->var[varname+".sas2"] = string(zrtp_session_info.sas2.buffer, zrtp_session_info.sas2.length);
   } else {
     sc_sess->var[varname+".sas1"] = sc_sess->var[varname+".sas2"] = string();
   }

   sc_sess->var[varname+".id"] = int2str(zrtp_session_info.id);
   string zid_hex;

   sc_sess->var[varname+".zid"] = "";
   for (size_t i=0;i<zrtp_session_info.zid.length;i++)
     sc_sess->var[varname+".zid"]+=char2hex(zrtp_session_info.zid.buffer[i], true);

   sc_sess->var[varname+".peer_zid"] = "";
   for (size_t i=0;i<zrtp_session_info.peer_zid.length;i++)
     sc_sess->var[varname+".peer_zid"]+=char2hex(zrtp_session_info.peer_zid.buffer[i], true);

   sc_sess->var[varname+".peer_clientid"] = string(zrtp_session_info.peer_clientid.buffer, zrtp_session_info.peer_clientid.length);
   sc_sess->var[varname+".peer_version"] = string(zrtp_session_info.peer_version.buffer, zrtp_session_info.peer_version.length);

   sc_sess->var[varname+".sas_is_verified"] = zrtp_session_info.sas_is_verified ? "true":"false";
   // todo: cached_flags, matches_flags, wrongs_flags

} EXEC_ACTION_END;

bool hex2zid(const string& zid1, char* buffer) {
  for (size_t i=0;i<zid1.length()/2;i++) {
    unsigned int h;
    if (reverse_hex2int(zid1.substr(i*2, 2), h)) {
      ERROR("in zid: '%s' is no hex number\n", zid1.substr(i*2, 2).c_str());
      return false;
    }
    buffer[i]=h % 0xff;
  }
  return true;
}

CONST_ACTION_2P(ZRTPSetVerifiedAction, ',', false);
EXEC_ACTION_START(ZRTPSetVerifiedAction) {
  string zid1 = resolveVars(par1, sess, sc_sess, event_params);
  string zid2 = resolveVars(par2, sess, sc_sess, event_params);
  if (zid1.empty() || zid2.empty()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("zid1 and zid2 must be there for setVerified");
    EXEC_ACTION_STOP;
  }

  DBG("setting as verified zids <%s> and <%s>\n", zid1.c_str(), zid2.c_str());

  zrtp_string16_t _zid1, _zid2;

  if (!hex2zid(zid1, _zid1.buffer)) {
      EXEC_ACTION_STOP;
  }

  if (!hex2zid(zid2, _zid2.buffer)) {
      EXEC_ACTION_STOP;
  }

  if (zrtp_status_ok != zrtp_verified_set(AmZRTP::zrtp_global, &_zid1, &_zid2, 1)) {
    DBG("zrtp_verified_set failed\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("zrtp_verified_set failed");
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(ZRTPSetUnverifiedAction, ',', false);
EXEC_ACTION_START(ZRTPSetUnverifiedAction) {
  string zid1 = resolveVars(par1, sess, sc_sess, event_params);
  string zid2 = resolveVars(par2, sess, sc_sess, event_params);
  if (zid1.empty() || zid2.empty()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("zid1 and zid2 must be there for setUnverified");
    EXEC_ACTION_STOP;
  }

  DBG("setting as unverified zids <%s> and <%s>\n", zid1.c_str(), zid2.c_str());

  zrtp_string16_t _zid1, _zid2;

  if (!hex2zid(zid1, _zid1.buffer)) {
      EXEC_ACTION_STOP;
  }

  if (!hex2zid(zid2, _zid2.buffer)) {
      EXEC_ACTION_STOP;
  }

  if (zrtp_status_ok != zrtp_verified_set(AmZRTP::zrtp_global, &_zid1, &_zid2, 0)) {
    DBG("zrtp_verified_set failed\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("zrtp_verified_set failed");
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(ZRTPSetSignalingHash) {
  string h = resolveVars(arg, sess, sc_sess, event_params);
  DBG("setting signaling hash to '%s'\n", h.c_str());

  if (NULL == sess->zrtp_session_state.zrtp_audio) {
    WARN("ZRTP not active on that stream\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("ZRTP not active on that stream");
    EXEC_ACTION_STOP;
  }
  
  if (zrtp_status_ok != zrtp_signaling_hash_set(sess->zrtp_session_state.zrtp_audio,
						h.c_str(), h.length())) {
    DBG("zrtp_signaling_hash_set failed\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("zrtp_signaling_hash_set failed");
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(ZRTPGetSignalingHash) {
  string varname = arg;
  if (varname.size() && varname[0]=='$') varname = varname.substr(1);

  if (NULL == sess->zrtp_session_state.zrtp_audio) {
    WARN("ZRTP not active on that stream\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("ZRTP not active on that stream");
    EXEC_ACTION_STOP;
  }

  char b[ZRTP_SIGN_ZRTP_HASH_LENGTH];
  memset(b, 0, sizeof(b));
  if (zrtp_status_ok != zrtp_signaling_hash_get(sess->zrtp_session_state.zrtp_audio,
						b, ZRTP_SIGN_ZRTP_HASH_LENGTH)) {
    DBG("zrtp_signaling_hash_get failed\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("zrtp_signaling_hash_get failed");
  }
  sc_sess->var[varname] = string(b);
  DBG("got signaling hash '%s'\n", b);
} EXEC_ACTION_END;

#endif
