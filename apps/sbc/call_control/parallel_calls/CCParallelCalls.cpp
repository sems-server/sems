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

#include "CCParallelCalls.h"
#include "ampi/SBCCallControlAPI.h"

#include <string.h>

class CCParallelCallsFactory : public AmDynInvokeFactory
{
public:
    CCParallelCallsFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return CCParallelCalls::instance();
    }

    int onLoad(){
      if (CCParallelCalls::instance()->onLoad())
	return -1;

      DBG("template call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCParallelCallsFactory,MOD_NAME);

CCParallelCalls* CCParallelCalls::_instance=0;

CCParallelCalls* CCParallelCalls::instance()
{
    if(!_instance)
	_instance = new CCParallelCalls();
    return _instance;
}

CCParallelCalls::CCParallelCalls()
{
}

CCParallelCalls::~CCParallelCalls() { }

int CCParallelCalls::onLoad() {
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

void CCParallelCalls::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  // DBG("CCParallelCalls: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

    if(method == "start"){

      // ltag, call profile, start_ts_sec, start_ts_usec, [[key: val], ...], timer_id
      args.assertArrayFmt("soiiui"); 

      // INFO("--------------------------------------------------------------\n");
      // INFO("Got call control start ltag '%s' start_ts %i.%i\n",
      // 	   args.get(0).asCStr(), args.get(2).asInt(), args.get(3).asInt());
      // INFO("---- dumping CC values ----\n");
      // for (AmArg::ValueStruct::const_iterator it =
      // 	     args.get(4).begin(); it != args.get(4).end(); it++) {
      // 	INFO("    CDR value '%s' = '%s'\n", it->first.c_str(), it->second.asCStr());
      // }
      // INFO("--------------------------------------------------------------\n");

      start(args[0].asCStr(), args[2].asInt(), args[3].asInt(), args[4],
	    args[5].asInt(),  ret);

    } else if(method == "connect"){
      // ltag, other_ltag, connect_ts_sec, connect_ts_usec
      args.assertArrayFmt("ssii"); 

      // INFO("--------------------------------------------------------------\n");
      // INFO("Got CDR connect ltag '%s' other_ltag '%s', connect_ts %i.%i\n",
      // 	   args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asInt(),
      // 	   args.get(3).asInt());
      // INFO("--------------------------------------------------------------\n");
      connect(args.get(0).asCStr(), args.get(1).asCStr(),
	      args.get(2).asInt(), args.get(3).asInt());
    } else if(method == "end"){
      // INFO("--------------------------------------------------------------\n");
      // INFO("Got CDR end ltag %s end_ts %i.%i\n",
      // 	   args.get(0).asCStr(), args.get(1).Int(), args.get(2).asInt());
      // INFO("--------------------------------------------------------------\n");
      end(args.get(0).asCStr(), args.get(1).asInt(), args.get(2).asInt());
    } else if(method == "_list"){
      ret.push("start");
      ret.push("connect");
      ret.push("end");
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

void CCParallelCalls::start(const string& ltag, int start_ts_sec, int start_ts_usec,
			    const AmArg& values, int timer_id, AmArg& res) {

  if (!values.hasMember("uuid") || !isArgCStr(values["uuid"]) ||
      !strlen(values["uuid"].asCStr())) {
    ERROR("configuration error: uuid missing for parallel calls call control!\n");
    res.push(AmArg());
    AmArg& res_cmd = res[0];
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
    return;
  }

  string uuid = values["uuid"].asCStr();

  unsigned int max_calls = 1; // default
  if (values.hasMember("max_calls") && isArgCStr(values["max_calls"])) {
    if (str2i(values["max_calls"].asCStr(), max_calls)) {
      ERROR("max_calls '%s' could not be interpreted!\n", values["max_calls"].asCStr());
      res.push(AmArg());
      AmArg& res_cmd = res[0];
      res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
      res_cmd[SBC_CC_REFUSE_CODE] = 500;
      res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
      return;
    }
  }

  DBG("enforcing limit of %i calls for uuid '%s'\n", max_calls, uuid.c_str());

  bool do_limit = !max_calls;
  unsigned int current_calls = 0;
  if (max_calls) {
    call_control_calls_mut.lock();
    map<string, unsigned int>::iterator it=call_control_calls.find(uuid);
    if (it==call_control_calls.end()) {
      call_control_calls[uuid] = current_calls = 1;
    } else {
      if (it->second < max_calls) {
	it->second++;
      } else {
	do_limit = true;
      }
      current_calls = it->second;
    }
  }

  if (!do_limit) {
    call_control_uuids[ltag] = uuid;
  }
  call_control_calls_mut.unlock();

  DBG("uuid %s has %u active calls (limit = %s)\n",
      uuid.c_str(), current_calls, do_limit?"true":"false");

  if (do_limit) {
    res.push(AmArg());
    AmArg& res_cmd = res[0];
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 402;
    res_cmd[SBC_CC_REFUSE_REASON] = "Too many Simultaneous Calls";
  }

}

void CCParallelCalls::connect(const string& ltag, const string& other_tag,
			      int connect_ts_sec, int connect_ts_usec) {
  // connect code here

}

void CCParallelCalls::end(const string& ltag,
			  int end_ts_sec, int end_ts_usec) {
  // end code here
  unsigned int new_call_count  = 0;
  string uuid = "<unknown>";

  call_control_calls_mut.lock();

  map<string, string>::iterator it=call_control_uuids.find(ltag);
  if (it!=call_control_uuids.end()) {
    uuid = it->second;
    if (call_control_calls[it->second] > 1) {
      new_call_count = --call_control_calls[it->second];
    }  else {
      call_control_calls.erase(it->second);
    }
    call_control_uuids.erase(it);
  }

  call_control_calls_mut.unlock();

  DBG("uuid '%s' now has %u active calls\n", uuid.c_str(), new_call_count);
}
