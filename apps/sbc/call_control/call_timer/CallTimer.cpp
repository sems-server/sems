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

#include "CallTimer.h"

#include "SBCCallControlAPI.h"
#include "AmSipHeaders.h"

#include <string.h>

class CallTimerFactory : public AmDynInvokeFactory
{
public:
    CallTimerFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return CallTimer::instance();
    }

    int onLoad(){
      if (CallTimer::instance()->onLoad())
	return -1;

      DBG("call timer call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(CallTimerFactory, MOD_NAME);

CallTimer* CallTimer::_instance=0;

CallTimer* CallTimer::instance()
{
    if(!_instance)
	_instance = new CallTimer();
    return _instance;
}

CallTimer::CallTimer()
  : default_timer(-1)
{
}

CallTimer::~CallTimer() { }

int CallTimer::onLoad() {
  AmConfigReader cfg;

  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    INFO(MOD_NAME "configuration  file (%s) not found, "
  	 "assuming default configuration is fine\n",
  	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
    return 0;
  }

  if (cfg.hasParameter("default_timer")) {
    if (!str2int(cfg.getParameter("default_timer"), default_timer)) {
      ERROR("default_timer '%s' not understood\n", cfg.getParameter("default_timer").c_str());
      return -1;
    }
  }

  DBG("default call timer set to '%i'\n", default_timer);

  return 0;
}

void CallTimer::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  // DBG("CallTimer: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if (method == "start"){

    start(args[CC_API_PARAMS_CFGVALUES],
	  args[CC_API_PARAMS_TIMERID].asInt(),  ret);

  } else if (method == "connect"){
    // unused
  } else if (method == "end"){
    // unused
  } else if (method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

void CallTimer::start(const AmArg& values, int timer_id, AmArg& res) {

  int timer = default_timer;

  if (values.hasMember("timer")) {
    if (isArgCStr(values["timer"])) {
      if (strlen(values["timer"].asCStr()))
	str2int(values["timer"].asCStr(), timer);
    } else if (isArgInt(values["timer"])) {
      timer = values["timer"].asInt();
    }
  }
  DBG("got timer value '%i'\n", timer);

  if (timer==0) {
    res.push(AmArg());
    AmArg& res_cmd = res.back();
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 503;
    res_cmd[SBC_CC_REFUSE_REASON] = "Service Unavailable";
    return;
  }

  if (timer<0) {
    ERROR("configuration error: timer missing for call timer call control!\n");
    res.push(AmArg());
    AmArg& res_cmd = res.back();
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
    return;
  }
  res.push(AmArg());
  AmArg& res_cmd = res.back();

  // Set Timer:
  DBG("setting timer ID %i, timeout %i\n", timer_id, timer);
  res_cmd[SBC_CC_ACTION] = SBC_CC_SET_CALL_TIMER_ACTION;
  res_cmd[SBC_CC_TIMER_TIMEOUT] = timer;
}
