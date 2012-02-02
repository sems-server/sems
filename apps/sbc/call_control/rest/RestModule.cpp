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

#include "RestModule.h"
#include "RestParams.h"

#include "ampi/SBCCallControlAPI.h"

#include <string.h>
#include <curl/curl.h>

class RestModuleFactory : public AmDynInvokeFactory
{
public:
    RestModuleFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return RestModule::instance();
    }

    int onLoad(){
      if (RestModule::instance()->onLoad())
	return -1;

      DBG("template call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(RestModuleFactory, MOD_NAME);

RestModule* RestModule::_instance=0;

RestModule* RestModule::instance()
{
    if(!_instance)
	_instance = new RestModule();
    return _instance;
}

RestModule::RestModule()
{
}

RestModule::~RestModule() { }

int RestModule::onLoad() 
{
  // FIXME: integrate with other modules using libcurl, better handling of
  // multithreaded access could be helpful, etc...

  static bool globally_initialized = false;

  if (!globally_initialized) {
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    if (res != 0) {
      ERROR("can not initialize libcurl: %d\n", res);
      return -1;
    }
  }

  return 0;
}

void RestModule::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("RestModule: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

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
  } else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

void RestModule::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  res.push(AmArg());
  AmArg& res_cmd = res[0];

  try {
    string url;
    bool ignore_errors = true;

    if (!values.hasMember("url")) 
      throw string("configuration error: url must be configured for REST queries\n");
      
    if (!isArgCStr(values["url"]) || !strlen(values["url"].asCStr())) {
      throw string("configuration error: invalid value of url\n");
    }

    /*FIXME
      if (values.hasMember("ignore_errors")) {

      if (!isArgBool(values["ignore_errors"])) {
	throw string("configuration error: invalid value of parameter ignore_errors\n");
      }

      ignore_errors = values["ignore_errors"].asBool();
    }*/

    url = values["url"].asCStr();
    DBG("REST: url = %s\n", url.c_str());

    RestParams params(url, ignore_errors);
    params.getIfSet("ruri", call_profile->ruri);
    params.getIfSet("from", call_profile->from);
    params.getIfSet("to", call_profile->to);
    params.getIfSet("contact", call_profile->contact);
    params.getIfSet("call-id", call_profile->callid);
    params.getIfSet("outbound_proxy", call_profile->outbound_proxy);
    params.getIfSet("force_outbound_proxy", call_profile->force_outbound_proxy);
    params.getIfSet("next_hop_ip", call_profile->next_hop_ip);
    params.getIfSet("next_hop_port", call_profile->next_hop_port);
    params.getIfSet("next_hop_for_replies", call_profile->next_hop_for_replies);

    // TODO: headerfilter, messagefilter

    // TODO: other params

  }
  catch (string &err) {
    ERROR(err.c_str());
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = "REST configuration error";
    return;
  }

}

void RestModule::connect(const string& cc_name, const string& ltag,
			 SBCCallProfile* call_profile,
			 const string& other_tag,
			 int connect_ts_sec, int connect_ts_usec) {
  // connect code here

}

void RestModule::end(const string& cc_name, const string& ltag,
		     SBCCallProfile* call_profile,
		     int end_ts_sec, int end_ts_usec) {
  // end code here

}
