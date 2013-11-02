/*
 * Copyright (C) 2013 Stefan Sayer
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
#include "ModSbc.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMCoreModule.h"

#include "SBCCallProfile.h"
#include "SBCDSMParams.h"
#include "SBCCallLeg.h"

SC_EXPORT(MOD_CLS_NAME);

int MOD_CLS_NAME::preload() {
  DBG("initializing mod_sbc...\n");
  return 0;
}

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {
  DEF_CMD("sbc.profileSet", MODSBCActionProfileSet);

  DEF_CMD("sbc.stopCall", MODSBCActionStopCall);
  DEF_CMD("sbc.disconnect", MODSBCActionDisconnect);
  DEF_CMD("sbc.putOnHold", MODSBCActionPutOnHold);
  DEF_CMD("sbc.resumeHeld", MODSBCActionResumeHeld);

  DEF_CMD("sbc.getCallStatus", MODSBCActionGetCallStatus);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {
  if (cmd == "legStateChange")
    return new TestDSMCondition(params, DSMCondition::LegStateChange);

  if (cmd == "bLegRefused")
    return new TestDSMCondition(params, DSMCondition::BLegRefused);

  // HOLD related
  if (cmd == "PutOnHold")
    return new TestDSMCondition(params, DSMCondition::PutOnHold);

  if (cmd == "ResumeHeld")
    return new TestDSMCondition(params, DSMCondition::ResumeHeld);

  if (cmd == "CreateHoldRequest")
    return new TestDSMCondition(params, DSMCondition::CreateHoldRequest);

  if (cmd == "HandleHoldReply")
    return new TestDSMCondition(params, DSMCondition::HandleHoldReply);

  // simple relay related
  if (cmd == "RelayInit")
    return new TestDSMCondition(params, DSMCondition::RelayInit);

  if (cmd == "RelayInitUAC")
    return new TestDSMCondition(params, DSMCondition::RelayInitUAC);

  if (cmd == "RelayInitUAS")
    return new TestDSMCondition(params, DSMCondition::RelayInitUAS);

  if (cmd == "RelayFinalize")
    return new TestDSMCondition(params, DSMCondition::RelayFinalize);

  if (cmd == "RelayOnSipRequest")
    return new TestDSMCondition(params, DSMCondition::RelayOnSipRequest);

  if (cmd == "RelayOnSipReply")
    return new TestDSMCondition(params, DSMCondition::RelayOnSipReply);

  if (cmd == "RelayOnB2BRequest")
    return new TestDSMCondition(params, DSMCondition::RelayOnB2BRequest);

  if (cmd == "RelayOnB2BReply")
    return new TestDSMCondition(params, DSMCondition::RelayOnB2BReply);

  if (cmd == "RelayOnB2BReply")
    return new TestDSMCondition(params, DSMCondition::RelayOnB2BReply);

  if (cmd == "sbc.isALeg")
    return new SBCIsALegCondition(params, false);

  if (cmd == "sbc.isOnHold")
    return new SBCIsOnHoldCondition(params, false);

} MOD_CONDITIONEXPORT_END


MATCH_CONDITION_START(SBCIsALegCondition) {
  SBCCallLeg* call_leg = dynamic_cast<SBCCallLeg*>(sess);
  if (NULL == call_leg) {
    DBG("script writer error: DSM condition sbc.isALeg"
	" used without call leg\n");
    return false;
  }

  bool b = call_leg->isALeg();
  bool res = inv ^ b;
  DBG("SBC: isALeg() == %s (res = %s)\n",
      b ? "true":"false", res ? "true":"false");
  return res;
} MATCH_CONDITION_END;

MATCH_CONDITION_START(SBCIsOnHoldCondition) {
  SBCCallLeg* call_leg = dynamic_cast<SBCCallLeg*>(sess);
  if (NULL == call_leg) {
    DBG("script writer error: DSM condition sbc.isOnHold"
	" used without call leg\n");
    return false;
  }

  bool b = call_leg->isOnHold();
  bool res = inv ^ b;
  DBG("SBC: isOnHold() == %s (res = %s)\n",
      b ? "true":"false", res ? "true":"false");
  return res;
} MATCH_CONDITION_END;


#define ACTION_GET_PROFILE			\
  SBCCallProfile* profile = NULL;					\
  AVarMapT::iterator it = sc_sess->avar.find(DSM_SBC_AVAR_PROFILE);	\
  if (it != sc_sess->avar.end()) {					\
    profile = dynamic_cast<SBCCallProfile*>(it->second.asObject());	\
  }									\
  if (NULL == profile) {						\
  SBCCallLeg* call = dynamic_cast<SBCCallLeg*>(sess);			\
  if (NULL !=  call)							\
    profile = &call->getCallProfile();					\
  }									\
									\
  if (NULL == profile) {						\
    ERROR("internal: Call profile object not found\n");			\
    EXEC_ACTION_STOP;							\
  }

CONST_ACTION_2P(MODSBCActionProfileSet, ',', false);
EXEC_ACTION_START(MODSBCActionProfileSet) {
  string profile_param = resolveVars(par1, sess, sc_sess, event_params);
  string value = resolveVars(par2, sess, sc_sess, event_params);

  ACTION_GET_PROFILE;

#define SET_TO_CALL_PROFILE(cfgparam, member)		\
  if (profile_param == cfgparam) {			\
    profile->member=value;				\
    DBG(cfgparam " set to '%s'\n", value.c_str());	\
    EXEC_ACTION_STOP;					\
  }

#define SET_TO_CALL_PROFILE_OPTION(cfgparam, member)			\
  if (profile_param == cfgparam) {					\
    profile->member=(value == "true");					\
    DBG(cfgparam " set to '%s'\n", profile->member?"true":"false");	\
    EXEC_ACTION_STOP;							\
  }

  switch (profile_param.length()) {
  case 2: 
  SET_TO_CALL_PROFILE("To", to);
  break;

  case 4:
  SET_TO_CALL_PROFILE("RURI", ruri);
  SET_TO_CALL_PROFILE("From", from);
  break;


  case 7:
  SET_TO_CALL_PROFILE("Call-ID", callid);
  break;

  case 8:
  SET_TO_CALL_PROFILE("next_hop", next_hop);
  break;

  case 9:
  SET_TO_CALL_PROFILE("RURI_host", ruri_host);
  break;


  case 11:
  SET_TO_CALL_PROFILE("refuse_with", refuse_with);
  break;

  default:

  SET_TO_CALL_PROFILE("outbound_proxy", outbound_proxy);
  SET_TO_CALL_PROFILE_OPTION("force_outbound_proxy", force_outbound_proxy);

  SET_TO_CALL_PROFILE("aleg_outbound_proxy", aleg_outbound_proxy);
  SET_TO_CALL_PROFILE_OPTION("aleg_force_outbound_proxy", aleg_force_outbound_proxy);

  SET_TO_CALL_PROFILE_OPTION("next_hop_1st_req", next_hop_1st_req);
  SET_TO_CALL_PROFILE_OPTION("patch_ruri_next_hop", patch_ruri_next_hop);

  SET_TO_CALL_PROFILE("aleg_next_hop", aleg_next_hop);

  // TODO: message_filter
  // TODO: header_filter
  // TODO: sdp_filter

  // TODO: auth
  // TODO: aleg_auth

  SET_TO_CALL_PROFILE("append_headers", append_headers);
  SET_TO_CALL_PROFILE("append_headers_req", append_headers_req);
  SET_TO_CALL_PROFILE("aleg_append_headers_req", aleg_append_headers_req);

  SET_TO_CALL_PROFILE_OPTION("rtprelay_enabled", rtprelay_enabled);

  SET_TO_CALL_PROFILE_OPTION("force_symmetric_rtp", force_symmetric_rtp_value);
  SET_TO_CALL_PROFILE_OPTION("aleg_force_symmetric_rtp", aleg_force_symmetric_rtp_value);
  SET_TO_CALL_PROFILE_OPTION("msgflags_symmetric_rtp", msgflags_symmetric_rtp);
  SET_TO_CALL_PROFILE_OPTION("rtprelay_transparent_seqno", rtprelay_transparent_seqno);
  SET_TO_CALL_PROFILE_OPTION("rtprelay_transparent_ssrc", rtprelay_transparent_ssrc);

  if (profile_param == "rtprelay_interface") {
    profile->rtprelay_interface=value;
    if (!profile->evaluateRTPRelayInterface()) {
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("rtprelay_interface '"+value+"' not present");
    } else {
      DBG("rtprelay_interface set to '%s'\n", value.c_str());
    }
    EXEC_ACTION_STOP;
  }

  if (profile_param == "aleg_rtprelay_interface") {
    profile->aleg_rtprelay_interface=value;
    if (!profile->evaluateRTPRelayAlegInterface()) {
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("aleg_rtprelay_interface '"+value+"' not present");
    } else {
      DBG("aleg_rtprelay_interface set to '%s'\n", value.c_str());
    }
    EXEC_ACTION_STOP;
  }

  }

  // TODO: Transcoder Settings
  // TODO: CODEC Prefs

  // TODO: contact hiding
  // TODO: reg_caching

  DBG("script writer: Call profile property '%s' not known\n", profile_param.c_str());
  sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
  sc_sess->SET_STRERROR("Call profile property '"+profile_param+"' not known");
} EXEC_ACTION_END;


#define GET_CALL_LEG(action)						\
  CallLeg* call_leg = dynamic_cast<CallLeg*>(sess);			\
  if (NULL == call_leg) {						\
    DBG("script writer error: DSM action " #action			\
	" used without call leg\n");					\
    throw DSMException("sbc", "type", "param", "cause",			\
		       "script writer error: DSM action " #action	\
		       " used without call leg");			\
  }

EXEC_ACTION_START(MODSBCActionStopCall) {
  GET_CALL_LEG(StopCall);
  string cause = resolveVars(arg, sess, sc_sess, event_params);
  call_leg->stopCall(cause.c_str());
} EXEC_ACTION_END;

EXEC_ACTION_START(MODSBCActionDisconnect) {
  GET_CALL_LEG(Disconnect);
  string hold_remote = resolveVars(arg, sess, sc_sess, event_params);
  call_leg->disconnect(hold_remote == DSM_TRUE);
} EXEC_ACTION_END;


EXEC_ACTION_START(MODSBCActionPutOnHold) {
  GET_CALL_LEG(PutOnHold);
  call_leg->putOnHold();
} EXEC_ACTION_END;

EXEC_ACTION_START(MODSBCActionResumeHeld) {
  GET_CALL_LEG(ResumeHeld);
  string send_reinvite = resolveVars(arg, sess, sc_sess, event_params);
  call_leg->resumeHeld(send_reinvite == DSM_TRUE);
} EXEC_ACTION_END;

EXEC_ACTION_START(MODSBCActionGetCallStatus) {
  GET_CALL_LEG(GetCallStatus);
  string varname = arg;
  if (varname.size() && varname[0] == '$')
    varname.erase(0, 1);
  sc_sess->var[varname] = call_leg->getCallStatusStr();
  DBG("set $%s='%s'\n", varname.c_str(), sc_sess->var[varname].c_str());
} EXEC_ACTION_END;
