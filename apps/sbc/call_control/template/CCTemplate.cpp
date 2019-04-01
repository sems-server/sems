/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "AmPlugIn.h"
#include "log.h"
#include "AmArg.h"

#include "CCTemplate.h"

#include "SBCCallControlAPI.h"

#include "SBCCallLeg.h"
#include <string.h>

class CCTemplateFactory : public AmDynInvokeFactory
{
public:
    CCTemplateFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return CCTemplate::instance();
    }

    int onLoad(){
      if (CCTemplate::instance()->onLoad())
	return -1;

      DBG("template call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCTemplateFactory, MOD_NAME);

CCTemplate* CCTemplate::_instance=0;

CCTemplate* CCTemplate::instance()
{
    if(!_instance)
	_instance = new CCTemplate();
    return _instance;
}

CCTemplate::CCTemplate()
{
}

CCTemplate::~CCTemplate() { }

int CCTemplate::onLoad() {
  AmConfigReader cfg;

  // if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
  //   INFO(MOD_NAME "configuration  file (%s) not found, "
  // 	 "assuming default configuration is fine\n",
  // 	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
  //   return 0;
  // }

  // syslog_prefix = cfg.hasParameter("cdr_prefix") ?
  //   cfg.getParameter("cdr_prefix") : syslog_prefix;

  return 0;
}

void CCTemplate::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("CCTemplate: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if(method == "start"){
    // INFO("--------------------------------------------------------------\n");
    // INFO("Got call control start ltag '%s' start_ts %i.%i\n",
    // 	   args.get(0).asCStr(), args[2][0].asInt(), args[2][1].asInt());
    // INFO("---- dumping CC values ----\n");
    // for (AmArg::ValueStruct::const_iterator it =
    // 	     args.get(CC_API_PARAMS_CFGVALUES).begin();
    //               it != args.get(CC_API_PARAMS_CFGVALUES).end(); it++) {
    // 	INFO("    CDR value '%s' = '%s'\n", it->first.c_str(), it->second.asCStr());
    // }
    // INFO("--------------------------------------------------------------\n");

    // cc_name, ltag, call profile, timestamps, [[key: val], ...], timer_id
    //args.assertArrayFmt("ssoaui");
    //args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    start(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	  args[CC_API_PARAMS_LTAG].asCStr(),
	  call_profile,
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_SEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_USEC].asInt(),
	  args[CC_API_PARAMS_CFGVALUES],
	  args[CC_API_PARAMS_TIMERID].asInt(),  ret);

  } else if(method == "connect"){
    // INFO("--------------------------------------------------------------\n");
    // INFO("Got CDR connect ltag '%s' other_ltag '%s', connect_ts %i.%i\n",
    // 	   args[CC_API_PARAMS_LTAG].asCStr(),
    //           args[CC_API_PARAMS_OTHERID].asCStr(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());
    // INFO("--------------------------------------------------------------\n");
    // cc_name, ltag, call_profile, other_ltag, connect_ts_sec, connect_ts_usec
    // args.assertArrayFmt("ssoas");
    // args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    connect(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	    args[CC_API_PARAMS_LTAG].asCStr(),
	    call_profile,
	    args[CC_API_PARAMS_OTHERID].asCStr(),
	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());

  } else if(method == "end"){
    // INFO("--------------------------------------------------------------\n");
    // INFO("Got CDR end ltag %s end_ts %i.%i\n",
    // 	   args[CC_API_PARAMS_LTAG].asCStr(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt());
    // INFO("--------------------------------------------------------------\n");

    // cc_name, ltag, call_profile, end_ts_sec, end_ts_usec
    // args.assertArrayFmt("ssoa");
    // args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    end(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	args[CC_API_PARAMS_LTAG].asCStr(),
	call_profile,
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
	);
  }
  // extended call control
  // else if (method == "getExtendedInterfaceHandler") {
  //   ret.push((AmObject*)this);
  // }

  else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

void CCTemplate::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  // call start code here
  res.push(AmArg());
  // AmArg& res_cmd = res[0];

  // Drop:
  // res_cmd[SBC_CC_ACTION] = SBC_CC_DROP_ACTION;

  // res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
  // res_cmd[SBC_CC_REFUSE_CODE] = 404;
  // res_cmd[SBC_CC_REFUSE_REASON] = "No, not here";

  // Set Timer:
  // DBG("my timer ID will be %i\n", timer_id);
  // res_cmd[SBC_CC_ACTION] = SBC_CC_SET_CALL_TIMER_ACTION;
  // res_cmd[SBC_CC_TIMER_TIMEOUT] = 5;
}

void CCTemplate::connect(const string& cc_name, const string& ltag,
			 SBCCallProfile* call_profile,
			 const string& other_tag,
			 int connect_ts_sec, int connect_ts_usec) {
  // call connect code here

}

void CCTemplate::end(const string& cc_name, const string& ltag,
		     SBCCallProfile* call_profile,
		     int end_ts_sec, int end_ts_usec) {
  // call end code here

}

// ------- extended call control interface -------------------


/*
bool CCTemplate::init(SBCCallLeg *call, const map<string, string> &values)
{
  DBG("ExtCC: init - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");

  SBCCallProfile &profile = call->getCallProfile();

  return true;
}

CCChainProcessing CCTemplate::onInitialInvite(SBCCallLeg *call, InitialInviteHandlerParams &params)
{
  DBG("ExtCC: onInitialInvite - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}

void CCTemplate::onStateChange(SBCCallLeg *call, const CallLeg::StatusChangeCause &cause) {
  DBG("ExtCC: onStateChange - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
};

CCChainProcessing CCTemplate::onBLegRefused(SBCCallLeg *call, const AmSipReply& reply)
{
  DBG("ExtCC: onBLegRefused - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}
void CCTemplate::onDestroyLeg(SBCCallLeg *call) {
  DBG("ExtCC: onDestroyLeg - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
}
*/

/** called from A/B leg when in-dialog request comes in */
/*
CCChainProcessing CCTemplate::onInDialogRequest(SBCCallLeg *call, const AmSipRequest &req) {
  DBG("ExtCC: onInDialogRequest - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}

CCChainProcessing CCTemplate::onInDialogReply(SBCCallLeg *call, const AmSipReply &reply) {
  DBG("ExtCC: onInDialogReply - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}
*/

/** called before any other processing for the event is done */
/*
CCChainProcessing CCTemplate::onEvent(SBCCallLeg *call, AmEvent *e) {
  DBG("ExtCC: onEvent - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}

CCChainProcessing CCTemplate::onDtmf(SBCCallLeg *call, int event, int duration) {
  DBG("ExtCC: onDtmf(%i;%i) - call instance: '%p' isAleg==%s\n",
      event, duration, call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}
*/

// hold related functionality
/*
CCChainProcessing CCTemplate::putOnHold(SBCCallLeg *call) {
  DBG("ExtCC: putOnHold - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}

CCChainProcessing CCTemplate::resumeHeld(SBCCallLeg *call, bool send_reinvite) {
  DBG("ExtCC: resumeHeld - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}

CCChainProcessing CCTemplate::createHoldRequest(SBCCallLeg *call, AmSdp &sdp) {
  DBG("ExtCC: createHoldRequest - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}

CCChainProcessing CCTemplate::handleHoldReply(SBCCallLeg *call, bool succeeded) {
  DBG("ExtCC: handleHoldReply - call instance: '%p' isAleg==%s\n", call, call->isALeg()?"true":"false");
  return ContinueProcessing;
}
*/

/** Possibility to influence messages relayed to the B2B peer leg.
    return value:
    - lower than 0 means error (returned upstream, the one
    returning error is responsible for destrying the event instance)
    - greater than 0 means "stop processing and return 0 upstream"
    - equal to 0 means "continue processing" */
/*int CCTemplate::relayEvent(SBCCallLeg *call, AmEvent *e) {

  return 0;
}
*/

// using extended CC modules with simple relay - non-call relay

/*
bool CCTemplate::init(SBCCallProfile& profile, SimpleRelayDialog *relay, void *&user_data) {
  DBG("init simple relay\n");
  return true;
}

void CCTemplate::initUAC(const AmSipRequest &req, void *user_data) {
  DBG("initUAC simple relay\n");
}

void CCTemplate::initUAS(const AmSipRequest &req, void *user_data) {
  DBG("initUAS simple relay\n");
}

void CCTemplate::finalize(void *user_data) {
  DBG("finalize simple relay\n");
}

void CCTemplate::onSipRequest(const AmSipRequest& req, void *user_data) {
  DBG("onSipRequest simple relay\n");
}

void CCTemplate::onSipReply(const AmSipRequest& req,
			     const AmSipReply& reply,
			     AmBasicSipDialog::Status old_dlg_status,
			     void *user_data) {
  DBG("onSipReply simple relay\n");
}

void CCTemplate::onB2BRequest(const AmSipRequest& req, void *user_data) {
  DBG("onB2BRequest simple relay\n");
}

void CCTemplate::onB2BReply(const AmSipReply& reply, void *user_data) {
  DBG("onB2BReply simple relay\n");
}
*/
