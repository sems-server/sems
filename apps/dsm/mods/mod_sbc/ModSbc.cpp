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
  DEF_CMD("sbc.sendDisconnectEvent", MODSBCActionSendDisconnectEvent);

  DEF_CMD("sbc.getCallStatus", MODSBCActionGetCallStatus);
  DEF_CMD("sbc.relayReliableEvent", MODSBCActionB2BRelayReliable);

  DEF_CMD("sbc.addCallee", MODSBCActionAddCallee);

  DEF_CMD("sbc.enableRelayDTMFReceiving", MODSBCEnableRelayDTMFReceiving);
  DEF_CMD("sbc.addToMediaProcessor", MODSBCAddToMediaProcessor);
  DEF_CMD("sbc.removeFromMediaProcessor", MODSBCRemoveFromMediaProcessor);
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

  SET_TO_CALL_PROFILE_OPTION("rtprelay_dtmf_filtering", rtprelay_dtmf_filtering);
  SET_TO_CALL_PROFILE_OPTION("rtprelay_dtmf_detection", rtprelay_dtmf_detection);

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

EXEC_ACTION_START(MODSBCActionSendDisconnectEvent) {
  GET_CALL_LEG(SendDisconnectEvent);
  string hold_remote = resolveVars(arg, sess, sc_sess, event_params);
  if (!AmSessionContainer::instance()->postEvent(call_leg->getLocalTag(),
						 new DisconnectLegEvent(hold_remote == DSM_TRUE))) {
    ERROR("couldn't self-post event\n");
  }
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

void setReliableEventParameters(const DSMSession* sc_sess, const string& var, VarMapT& params) {
  vector<string> vars = explode(var, ";");
  for (vector<string>::iterator it = vars.begin(); it != vars.end(); it++) {
    string varname = *it;

    if (varname.length() && varname[varname.length()-1]=='.') {
      DBG("adding postEvent param %s (struct)\n", varname.c_str());

      map<string, string>::const_iterator lb = sc_sess->var.lower_bound(varname);
      while (lb != sc_sess->var.end()) {
	if ((lb->first.length() < varname.length()) ||
	    strncmp(lb->first.c_str(), varname.c_str(), varname.length()))
	  break;
	params[lb->first] = lb->second;
	lb++;
      }
    } else {
      VarMapT::const_iterator v_it = sc_sess->var.find(varname);
      if (v_it != sc_sess->var.end()) {
	DBG("adding reliableEvent param %s=%s\n",
	    it->c_str(), v_it->second.c_str());
	params[varname] = v_it->second;
      }
    }
  }
}

CONST_ACTION_2P(MODSBCActionB2BRelayReliable, ',', false);
EXEC_ACTION_START(MODSBCActionB2BRelayReliable) {
  GET_CALL_LEG(B2BRelayReliable);
  string ev_params = par1;
  vector<string> success_params = explode(par2, ","); // q&d...
  B2BEvent* processed = new B2BEvent(E_B2B_APP, B2BEvent::B2BApplication);
  if (success_params.size()) {
    setReliableEventParameters(sc_sess, trim(success_params[0], " "), processed->params);
  }
  B2BEvent* unprocessed = new B2BEvent(E_B2B_APP, B2BEvent::B2BApplication);
  if (success_params.size() > 1) {
    DBG("p='%s'\n", success_params[1].c_str());
    setReliableEventParameters(sc_sess, trim(success_params[1], " "), unprocessed->params);
  }

  ReliableB2BEvent* rel_b2b_ev = new ReliableB2BEvent(E_B2B_APP, B2BEvent::B2BApplication, processed, unprocessed);
  setReliableEventParameters(sc_sess, ev_params, rel_b2b_ev->params);

  rel_b2b_ev->setSender(call_leg->getLocalTag());
  call_leg->relayEvent(rel_b2b_ev);
} EXEC_ACTION_END;

CONST_ACTION_2P(MODSBCActionAddCallee, ',', false);
EXEC_ACTION_START(MODSBCActionAddCallee) {
  GET_CALL_LEG(AddCallee);

  SBCCallLeg* sbc_call_leg = dynamic_cast<SBCCallLeg*>(call_leg);
  if (NULL == sbc_call_leg) {
    DBG("script writer error: DSM action sbc.addCallee "
	" used without sbc call leg\n");
    throw DSMException("sbc", "type", "param", "cause",
		       "script writer error: DSM action sbc.addCallee "
		       " used without call leg");
    EXEC_ACTION_STOP;
  }

  string mode = resolveVars(par1, sess, sc_sess, event_params);

  if (mode == DSM_SBC_PARAM_ADDCALLEE_MODE_STR) {
    string varname = par2;
    string hdrs;
    SBCCallLeg* peer = new SBCCallLeg(sbc_call_leg);
    SBCCallProfile &p = peer->getCallProfile();
    AmB2BSession::RTPRelayMode rtp_mode = call_leg->getRtpRelayMode();

    VarMapT::iterator it = sc_sess->var.find(varname+"." DSM_SBC_PARAM_ADDCALLEE_LOCAL_PARTY);
    if (it != sc_sess->var.end())
      peer->setLocalParty(it->second, it->second);

    it = sc_sess->var.find(varname+"." DSM_SBC_PARAM_ADDCALLEE_REMOTE_PARTY);
    if (it != sc_sess->var.end())
      peer->setRemoteParty(it->second, it->second);

    it = sc_sess->var.find(varname+"." DSM_SBC_PARAM_ADDCALLEE_HDRS);
    if (it != sc_sess->var.end())
      hdrs = it->second;

    it = sc_sess->var.find(varname+"." DSM_SBC_PARAM_ADDCALLEE_OUTBOUND_PROXY);
    if (it != sc_sess->var.end())
      p.outbound_proxy = it->second;

    it = sc_sess->var.find(varname+"." DSM_SBC_PARAM_ADDCALLEE_RTP_MODE);
    if (it != sc_sess->var.end()) {
      if (it->second == "RTP_Direct") {
	rtp_mode = AmB2BSession::RTP_Direct;
      } else if (it->second == "RTP_Relay") {
	rtp_mode = AmB2BSession::RTP_Relay;
      } else if (it->second == "RTP_Transcoding") {
	rtp_mode = AmB2BSession::RTP_Transcoding;
      } else {
	WARN("Unknown rtp_mode '%s', using this leg's mode\n", it->second.c_str());
      }
    }

    sbc_call_leg->addCallee(peer, hdrs, rtp_mode);
  }

} EXEC_ACTION_END;

EXEC_ACTION_START(MODSBCEnableRelayDTMFReceiving) {
  GET_CALL_LEG(AddCallee);

  bool enable = (resolveVars(arg, sess, sc_sess, event_params)==DSM_TRUE);

  SBCCallLeg* sbc_call_leg = dynamic_cast<SBCCallLeg*>(call_leg);
  if (NULL == sbc_call_leg) {
    DBG("script writer error: DSM action sbc.addCallee "
	" used without sbc call leg\n");
    throw DSMException("sbc", "type", "param", "cause",
		       "script writer error: DSM action sbc.addCallee "
		       " used without call leg");
    EXEC_ACTION_STOP;
  }

  AmB2BMedia* b2b_media = sbc_call_leg->getMediaSession();
  DBG("session: %p, media: %p\n", sbc_call_leg, b2b_media);
  if (NULL != b2b_media) {
    b2b_media->setRelayDTMFReceiving(enable);
  } else {
    DBG("No B2BMedia in current SBC call leg, sorry\n");
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(MODSBCAddToMediaProcessor) {
  GET_CALL_LEG(AddToMediaProcessor);
  AmMediaProcessor::instance()->addSession(call_leg, call_leg->getCallgroup());

} EXEC_ACTION_END;

EXEC_ACTION_START(MODSBCRemoveFromMediaProcessor) {
  GET_CALL_LEG(RemoveFromMediaProcessor);
  AmMediaProcessor::instance()->removeSession(call_leg);
} EXEC_ACTION_END;
  
