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

#include "log.h"
#include "CCDSM.h"

static const string data_var_name(MOD_NAME "::data");

EXPORT_PLUGIN_CLASS_FACTORY(CCDSMFactory, MOD_NAME);

CCDSMModule* CCDSMModule::_instance = 0;

CCDSMModule* CCDSMModule::instance()
{
    if (!_instance) _instance = new CCDSMModule();
    return _instance;
}

void CCDSMModule::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  TRACE(MOD_NAME " %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if ((method == "start") || (method == "connect") || (method == "end")) {
    return; // FIXME: advertise that the interface shouldn't be used
  }
  else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else if (method == "getExtendedInterfaceHandler") {
    ret.push((AmObject*)this);
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

SBCDSMInstance* CCDSMModule::getDSMInstance(SBCCallProfile &profile)
{
  SBCVarMapIteratorT i = profile.cc_vars.find(data_var_name);
  if (i != profile.cc_vars.end())
    return dynamic_cast<SBCDSMInstance*>(i->second.asObject());

  return NULL;
}

void CCDSMModule::deleteDSMInstance(SBCCallProfile &profile)
{
  SBCVarMapIteratorT i = profile.cc_vars.find(data_var_name);
  if (i != profile.cc_vars.end()) {
    SBCDSMInstance* h = dynamic_cast<SBCDSMInstance*>(i->second.asObject());
    if (h) delete h;
    profile.cc_vars.erase(i);
  }
}

void CCDSMModule::resetDSMInstance(SBCCallProfile &profile)
{
  SBCVarMapIteratorT i = profile.cc_vars.find(data_var_name);
  if (i != profile.cc_vars.end()) {
    profile.cc_vars.erase(i);
  }
}

bool CCDSMModule::init(SBCCallLeg *call, const map<string, string> &values)
{
  DBG("ExtCC: init - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");

  SBCCallProfile &profile = call->getCallProfile();
  resetDSMInstance(profile); // just forget the handler if already set

  // if (!call->isALeg()) return;

  // create DSM handler if necessary (with evaluated configuration)
  try {
    SBCDSMInstance* h = new SBCDSMInstance(call, values);
    profile.cc_vars[data_var_name] = (AmObject*)h;
  } catch (const string& e) {
    ERROR("initializing DSM Call control module: '%s'\n", e.c_str());
    return false;
  } catch (...) {
    ERROR("initializing DSM Call control module\n");
    return false;
  }
  return true;
}

#define GET_DSM_INSTANCE				      \
  SBCDSMInstance* h = getDSMInstance(call->getCallProfile()); \
  if (NULL == h)					      \
    return StopProcessing;

#define GET_DSM_INSTANCE_VOID						\
  SBCDSMInstance* h = getDSMInstance(call->getCallProfile());		\
  if (NULL == h) {							\
    ERROR("DSM instance not found for call leg\n");			\
    return;/* todo: throw exception? */					\
  }

CCChainProcessing CCDSMModule::onInitialInvite(SBCCallLeg *call, InitialInviteHandlerParams &params)
{
  DBG("ExtCC: onInitialInvite - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->onInitialInvite(call, params);
}

void CCDSMModule::onStateChange(SBCCallLeg *call, const CallLeg::StatusChangeCause &cause) {
  DBG("ExtCC: onStateChange - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE_VOID;
  h->onStateChange(call, cause);
};

CCChainProcessing CCDSMModule::onBLegRefused(SBCCallLeg *call, const AmSipReply& reply)
{
  DBG("ExtCC: onBLegRefused - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->onBLegRefused(call, reply);
}
void CCDSMModule::onDestroyLeg(SBCCallLeg *call) {
  DBG("ExtCC: onDestroyLeg - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  DBG("TODO: call DSM event?\n");
  deleteDSMInstance(call->getCallProfile());
}

/** called from A/B leg when in-dialog request comes in */
CCChainProcessing CCDSMModule::onInDialogRequest(SBCCallLeg *call, const AmSipRequest &req) {
  DBG("ExtCC: onInDialogRequest - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->onInDialogRequest(call, req);
}

CCChainProcessing CCDSMModule::onInDialogReply(SBCCallLeg *call, const AmSipReply &reply) {
  DBG("ExtCC: onInDialogReply - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->onInDialogReply(call, reply);
}

/** called before any other processing for the event is done */
CCChainProcessing CCDSMModule::onEvent(SBCCallLeg *call, AmEvent *e) {
  DBG("ExtCC: onEvent - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->onEvent(call, e);
}

CCChainProcessing CCDSMModule::onDtmf(SBCCallLeg *call, int event, int duration) { 
  DBG("ExtCC: onDtmf(%i;%i) - call instance: '%p' isAleg==%s\n",
      event, duration, call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->onDtmf(call, event, duration);
}

// hold related functionality
CCChainProcessing CCDSMModule::putOnHold(SBCCallLeg *call) {
  DBG("ExtCC: putOnHold - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->putOnHold(call);
}

CCChainProcessing CCDSMModule::resumeHeld(SBCCallLeg *call, bool send_reinvite) {
  DBG("ExtCC: resumeHeld - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->resumeHeld(call, send_reinvite);
}

CCChainProcessing CCDSMModule::createHoldRequest(SBCCallLeg *call, AmSdp &sdp) {
  DBG("ExtCC: createHoldRequest - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->createHoldRequest(call, sdp);
}

CCChainProcessing CCDSMModule::handleHoldReply(SBCCallLeg *call, bool succeeded) {
  DBG("ExtCC: handleHoldReply - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  GET_DSM_INSTANCE;
  return h->handleHoldReply(call, succeeded);
}

/** Possibility to influence messages relayed to the B2B peer leg.
    return value:
    - lower than 0 means error (returned upstream, the one
    returning error is responsible for destrying the event instance)
    - greater than 0 means "stop processing and return 0 upstream"
    - equal to 0 means "continue processing" */
int CCDSMModule::relayEvent(SBCCallLeg *call, AmEvent *e) {
  // todo
  return 0;
}

// using extended CC modules with simple relay

struct RelayUserData {
  SimpleRelayDialog *relay;
  SBCCallProfile& profile;
  RelayUserData(SimpleRelayDialog *relay, SBCCallProfile& profile)
    : relay(relay), profile(profile) { }
};


bool CCDSMModule::init(SBCCallProfile& profile, SimpleRelayDialog *relay, void *&user_data) {
  SBCDSMInstance* h = getDSMInstance(profile);
  if (NULL == h) {
    user_data = NULL;
    return false;
  }

  if (!h->init(profile, relay)) {
    return false;
  }

  user_data = new RelayUserData(relay, profile);
  return true;
}

#define GET_USER_DATA				\
  if (NULL == user_data)			\
    return;					\
  						\
  RelayUserData* ud= (RelayUserData*)user_data;		\
  SBCDSMInstance* h = getDSMInstance(ud->profile);	\
  if (NULL == h) {					\
    ERROR("SBC DSM instance disappeared, huh?\n");	\
    return;						\
  }

void CCDSMModule::initUAC(const AmSipRequest &req, void *user_data) {
  GET_USER_DATA;
  h->initUAC(ud->profile, ud->relay, req);
}

void CCDSMModule::initUAS(const AmSipRequest &req, void *user_data) {
  GET_USER_DATA;
  h->initUAS(ud->profile, ud->relay, req);
}

void CCDSMModule::finalize(void *user_data) {
  GET_USER_DATA;
  h->finalize(ud->profile, ud->relay);
  delete ud;
}

void CCDSMModule::onSipRequest(const AmSipRequest& req, void *user_data) {
  GET_USER_DATA;
  h->onSipRequest(ud->profile, ud->relay, req);
}

void CCDSMModule::onSipReply(const AmSipRequest& req,
			     const AmSipReply& reply,
			     AmBasicSipDialog::Status old_dlg_status,
			     void *user_data) {
  GET_USER_DATA;
  h->onSipReply(ud->profile, ud->relay, req, reply, old_dlg_status);
}

void CCDSMModule::onB2BRequest(const AmSipRequest& req, void *user_data) {
  GET_USER_DATA;
  h->onB2BRequest(ud->profile, ud->relay, req);
}

void CCDSMModule::onB2BReply(const AmSipReply& reply, void *user_data) {
  GET_USER_DATA;
  h->onB2BReply(ud->profile, ud->relay, reply);
}
