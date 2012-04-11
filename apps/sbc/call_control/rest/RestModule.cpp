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
#include <algorithm>
#include <curl/curl.h>

using namespace std;

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

static RestParams::Format getFormat(const AmArg &values, RestParams::Format _default)
{
  if (!values.hasMember("format")) return _default;

  const AmArg &a = values["format"];
  if (!isArgCStr(a)) throw string("configuration error: wrong arg type\n");

  const char *str = a.asCStr();
  if (str) {
    if (strcmp(str, "text") == 0) return RestParams::TEXT;
    if (strcmp(str, "json") == 0) return RestParams::JSON;
    if (strcmp(str, "xml") == 0) return RestParams::XML;
  }

  throw string("invalid format parameter value\n");
  
  return _default;
}

static void setHeaderFilter(SBCCallProfile* call_profile,
    const string &type, const string &list) 
{
  if (type=="transparent")
    call_profile->headerfilter = Transparent;
  else if (type=="whitelist")
    call_profile->headerfilter = Whitelist;
  else if (type=="blacklist")
    call_profile->headerfilter = Blacklist;
  else {
    ERROR("invalid header_filter mode '%s'\n", type.c_str());
    throw string("invalid header filter");
  }

  vector<string> v = explode(list, ",");
  for (vector<string>::iterator i = v.begin(); i != v.end(); ++i) {
    transform(i->begin(), i->end(), i->begin(), ::tolower);
    call_profile->headerfilter_list.insert(*i);
  }
}

void RestModule::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  res.push(AmArg());
  AmArg& res_cmd = res[0];

  try {
    string url;

    if (!values.hasMember("url")) 
      throw string("configuration error: url must be configured for REST queries\n");
      
    if (!isArgCStr(values["url"]) || !strlen(values["url"].asCStr())) {
      throw string("configuration error: invalid value of url\n");
    }

    url = values["url"].asCStr();

    RestParams params;
    if (params.retrieve(url, getFormat(values, RestParams::TEXT))) {
      // parameters successfully read from server

      params.getIfSet("ruri", call_profile->ruri);
      params.getIfSet("from", call_profile->from);
      params.getIfSet("to", call_profile->to);
      params.getIfSet("contact", call_profile->contact);
      params.getIfSet("call-id", call_profile->callid);
      params.getIfSet("outbound_proxy", call_profile->outbound_proxy);
      params.getIfSet("force_outbound_proxy", call_profile->force_outbound_proxy);
      params.getIfSet("next_hop", call_profile->next_hop);
      params.getIfSet("next_hop_for_replies", call_profile->next_hop_for_replies);

      string hf_type, hf_list;
      params.getIfSet("header_filter", hf_type);
      params.getIfSet("header_list", hf_list);
      if ( (!hf_type.empty()) || (!hf_list.empty())) setHeaderFilter(call_profile, hf_type, hf_list);
      
      //messagefilter

      // sdpfilter, anonymize_sdp

      params.getIfSet("sst_enabled", call_profile->sst_enabled);
      //doesn't work:params.getIfSet("sst_aleg_enabled", call_profile->sst_aleg_enabled);

      // TODO: autho, auth_aleg, reply translations

      params.getIfSet("append_headers", call_profile->append_headers); // CRLF is handled in SBC

      //doesn't work: params.getIfSet("refuse_with", call_profile->refuse_with);

      // TODO: rtprelay, symmetric_rtp, ...
      params.getIfSet("rtprelay_interface", call_profile->rtprelay_interface);
      params.getIfSet("aleg_rtprelay_interface", call_profile->aleg_rtprelay_interface);

      params.getIfSet("outbound_interface", call_profile->outbound_interface);
    }
  }
  catch (string &err) {
    ERROR("%s", err.c_str());
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = "REST configuration error";
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
