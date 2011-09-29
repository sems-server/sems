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

#include "ampi/SBCCallControlAPI.h"

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

      // ltag, call profile, timestamps, [[key: val], ...], timer_id
      args.assertArrayFmt("soaui");
      args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");
      SBCCallProfile* call_profile =
	dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

      start(args[CC_API_PARAMS_LTAG].asCStr(),
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
      // ltag, call_profile, other_ltag, connect_ts_sec, connect_ts_usec
      args.assertArrayFmt("soas");
      args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");
      SBCCallProfile* call_profile =
	dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

      connect(args[CC_API_PARAMS_LTAG].asCStr(),
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

      // ltag, call_profile, end_ts_sec, end_ts_usec
      args.assertArrayFmt("soa"); 
      args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");
      SBCCallProfile* call_profile =
	dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

      end(args[CC_API_PARAMS_LTAG].asCStr(),
	      call_profile,
	      args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
	      args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
	      );
    } else if(method == "_list"){
      ret.push("start");
      ret.push("connect");
      ret.push("end");
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

void CCTemplate::start(const string& ltag, SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  // start code here
  res.push(AmArg());
  AmArg& res_cmd = res[0];

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

void CCTemplate::connect(const string& ltag, SBCCallProfile* call_profile,
			 const string& other_tag,
			 int connect_ts_sec, int connect_ts_usec) {
  // connect code here

}

void CCTemplate::end(const string& ltag, SBCCallProfile* call_profile,
		     int end_ts_sec, int end_ts_usec) {
  // end code here

}
