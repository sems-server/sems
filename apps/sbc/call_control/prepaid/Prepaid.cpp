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
#include "AmSipHeaders.h"

#include "Prepaid.h"

#include "SBCCallControlAPI.h"

#include <string.h>

#define SBCVAR_PREPAID_UUID "uuid"

class PrepaidFactory : public AmDynInvokeFactory
{
public:
    PrepaidFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return Prepaid::instance();
    }

    int onLoad(){
      if (Prepaid::instance()->onLoad())
	return -1;

      DBG("prepaid call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(PrepaidFactory, MOD_NAME);

Prepaid* Prepaid::_instance=0;

Prepaid* Prepaid::instance()
{
    if(!_instance)
	_instance = new Prepaid();
    return _instance;
}

Prepaid::Prepaid()
{
}

Prepaid::~Prepaid() { }

int Prepaid::onLoad() {
  // AmConfigReader cfg;

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

void Prepaid::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  // DBG("Prepaid: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if(method == "start"){
    args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");
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
    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    connect(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	    args[CC_API_PARAMS_LTAG].asCStr(),
	    call_profile,
	    args[CC_API_PARAMS_OTHERID].asCStr(),
	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());

  } else if(method == "end"){
    args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");
    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    end(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	args[CC_API_PARAMS_LTAG].asCStr(),
	call_profile,
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_SEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_USEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
	);
  } else if(method == CC_INTERFACE_MAND_VALUES_METHOD){

    ret.push("uuid");

  } else if (method == "getCredit"){
    assertArgCStr(args.get(0));
    bool found;
    ret.push(getCredit(args.get(0).asCStr(), found));
    ret.push(found);
  } else if (method == "subtractCredit"){
    assertArgCStr(args.get(0));
    assertArgInt(args.get(1));
    bool found;
    ret.push(subtractCredit(args.get(0).asCStr(),
			    args.get(1).asInt(),
			    found));
    ret.push(found);
  } else if (method == "addCredit"){
    assertArgCStr(args.get(0));
    assertArgInt(args.get(1));
    ret.push(addCredit(args.get(0).asCStr(),
		       args.get(1).asInt()));	
  } else if (method == "setCredit"){
    assertArgCStr(args.get(0));
    assertArgInt(args.get(1));
    ret.push(setCredit(args.get(0).asCStr(),
		       args.get(1).asInt()));	
  } else if (method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");

    ret.push("getCredit");
    ret.push("subtractCredit");
    ret.push("setCredit");
    ret.push("addCredit");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

void Prepaid::start(const string& cc_name, const string& ltag,
		    SBCCallProfile* call_profile,
		    int start_ts_sec, int start_ts_usec,
		    const AmArg& values, int timer_id, AmArg& res) {

  if (!call_profile) return;

  res.push(AmArg());
  AmArg& res_cmd = res[0];

  if (!values.hasMember("uuid") || !isArgCStr(values["uuid"]) ||
      !strlen(values["uuid"].asCStr())) {
    ERROR("configuration error: uuid missing for prepaid call control!\n");
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
    return;
  }

  string uuid = values["uuid"].asCStr();

  call_profile->cc_vars[cc_name+"::"+SBCVAR_PREPAID_UUID] = uuid;

  bool found;
  int credit = getCredit(uuid, found);
  if (!found) {
    ERROR("Failed to fetch credit for uuid '%s'\n", uuid.c_str());
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
    return;
  }

  if (credit<=0) {
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 402;
    res_cmd[SBC_CC_REFUSE_REASON] = "Insufficient Credit";
    return;
  }

  // Set Timer:
  DBG("setting prepaid call timer ID %i of %i seconds\n", timer_id, credit);
  res_cmd[SBC_CC_ACTION] = SBC_CC_SET_CALL_TIMER_ACTION;
  res_cmd[SBC_CC_TIMER_TIMEOUT] = credit;
}

void Prepaid::connect(const string& cc_name, 
		      const string& ltag, SBCCallProfile* call_profile,
		      const string& other_tag,
		      int connect_ts_sec, int connect_ts_usec) {
  // DBG("call '%s' gets connected\n", ltag.c_str());
}

void Prepaid::end(const string& cc_name, 
		  const string& ltag, SBCCallProfile* call_profile,
		  int start_ts_sec, int start_ts_usec,
		  int connect_ts_sec, int connect_ts_usec,
		  int end_ts_sec, int end_ts_usec) {

  if (!call_profile) return;

  // get uuid
  SBCVarMapIteratorT vars_it = call_profile->
    cc_vars.find(cc_name+"::"+SBCVAR_PREPAID_UUID);

  if (vars_it == call_profile->cc_vars.end() || !isArgCStr(vars_it->second)) {
    ERROR("internal: could not find UUID for call '%s' - "
	  "not accounting (start_ts %i.%i, connect_ts %i.%i, end_ts %i.%i)\n",
	  ltag.c_str(), start_ts_sec, start_ts_usec,
	  connect_ts_sec, connect_ts_usec,
	  end_ts_sec, end_ts_usec);
    return;
  }

  string uuid = vars_it->second.asCStr();
  call_profile->cc_vars.erase(cc_name+"::"+SBCVAR_PREPAID_UUID);

  if (!connect_ts_sec || !end_ts_sec) {
    DBG("call not connected - uuid '%s' ltag '%s'\n", uuid.c_str(), ltag.c_str());
    return;
  }

  struct timeval start;
  start.tv_sec = connect_ts_sec;
  start.tv_usec = connect_ts_usec;
  struct timeval diff;
  diff.tv_sec = end_ts_sec;
  diff.tv_usec = end_ts_usec;
  if (!connect_ts_sec || timercmp(&start, &diff, >)) {
    diff.tv_sec = diff.tv_usec = 0;
  } else {
    timersub(&diff,&start,&diff);
  }

  // rounding
  if (diff.tv_usec >= 500000)
    diff.tv_sec++;

  DBG("call ltag '%s' for uuid '%s' lasted %lds\n", ltag.c_str(), uuid.c_str(), diff.tv_sec);

  bool found;
  subtractCredit(uuid, diff.tv_sec, found);
  if (!found) {
    ERROR("credit for uuid '%s' not found\n", uuid.c_str());
  }
}

/* accounting functions... */
int Prepaid::getCredit(string pin, bool& found) {
  credits_mut.lock();
  std::map<string, unsigned int>::iterator it =  credits.find(pin);
  if (it == credits.end()) {
    found = false;
    credits_mut.unlock();
    DBG("PIN '%s' does not exist.\n", pin.c_str());
    return 0;
  }
  unsigned int res = it->second;
  credits_mut.unlock();
  found = true;
  return res;
}

int Prepaid::setCredit(string pin, int amount) {
  credits_mut.lock();
  credits[pin] = amount;
  credits_mut.unlock();
  return amount;
}

int Prepaid::addCredit(string pin, int amount) {
  credits_mut.lock();
  int res = (credits[pin] += amount);
  credits_mut.unlock();
  return res;
}

int Prepaid::subtractCredit(string pin, int amount, bool& found) {
  credits_mut.lock();
  std::map<string, unsigned int>::iterator it =  credits.find(pin);
  if (it == credits.end()) {
    credits[pin] = -amount;
    credits_mut.unlock();
    found = false;
    return -amount;
  }
  credits[pin] = it->second - amount;
  int res =  credits[pin];
  credits_mut.unlock();
  found = true;
  return res;
}

