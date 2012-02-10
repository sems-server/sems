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
#include "SBC.h"

#include "CCCtl.h"

#include "ampi/SBCCallControlAPI.h"

#include <string.h>
#include <algorithm>

class CCCtlFactory : public AmDynInvokeFactory
{
public:
    CCCtlFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return CCCtl::instance();
    }

    int onLoad(){
      if (CCCtl::instance()->onLoad())
	return -1;

      DBG("ctl call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCCtlFactory, "ctl");

CCCtl* CCCtl::_instance=0;

CCCtl* CCCtl::instance()
{
    if(!_instance)
	_instance = new CCCtl();
    return _instance;
}

CCCtl::CCCtl()
{
}

CCCtl::~CCCtl() { }

int CCCtl::onLoad() {
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

void CCCtl::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("CCCtl: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if(method == "start"){
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
    // dummy
  } else if(method == "end"){
    // dummy
  } else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

void CCCtl::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  if (!call_profile) {
    ERROR("internal - call profile not found\n");
    return;
  }

#define SET_TO_CALL_PROFILE(cfgparam, member)		\
  if (values.hasMember(cfgparam)) {			\
    DBG("setting '%s' to '%s'\n", cfgparam, values[cfgparam].asCStr());	\
    call_profile->member = values[cfgparam].asCStr();	\
  }

#define ADD_TO_CALL_PROFILE(cfgparam, member)		\
  if (values.hasMember(cfgparam)) {			\
    DBG("adding '%s' to '%s'\n", values[cfgparam].asCStr(), cfgparam);	\
    call_profile->member += values[cfgparam].asCStr();	\
  }

#define ENABLE_IN_CALL_PROFILE(cfgparam, member)	\
  if (values.hasMember(cfgparam)) {			\
    call_profile->member =				\
      string(values[cfgparam].asCStr()) == "yes";	\
    DBG("%sabling '%s'\n", call_profile->member?"en":"dis", \
	values[cfgparam].asCStr());		       \
  }

  SET_TO_CALL_PROFILE("RURI", ruri);
  SET_TO_CALL_PROFILE("From", from);
  SET_TO_CALL_PROFILE("To", to);
  SET_TO_CALL_PROFILE("Contact", contact);
  SET_TO_CALL_PROFILE("Call-ID", callid);
  SET_TO_CALL_PROFILE("outbound_proxy", outbound_proxy);
  ENABLE_IN_CALL_PROFILE("force_outbound_proxy", force_outbound_proxy);

  SET_TO_CALL_PROFILE("next_hop_ip", next_hop_ip);
  SET_TO_CALL_PROFILE("next_hop_port", next_hop_port);

  SET_TO_CALL_PROFILE("sst_enabled", sst_enabled);
  SET_TO_CALL_PROFILE("sst_aleg_enabled", sst_aleg_enabled);

  assertEndCRLF(call_profile->append_headers);
  ADD_TO_CALL_PROFILE("append_headers", append_headers);
  assertEndCRLF(call_profile->append_headers);

  ENABLE_IN_CALL_PROFILE("rtprelay_enabled", rtprelay_enabled);
  SET_TO_CALL_PROFILE("rtprelay_interface", rtprelay_interface);
  SET_TO_CALL_PROFILE("aleg_rtprelay_interface", aleg_rtprelay_interface);

  SET_TO_CALL_PROFILE("outbound_interface", outbound_interface);

  if (values.hasMember("headerfilter")) {
    string hf = values["headerfilter"].asCStr();
    FilterType t = String2FilterType(hf.c_str());
    if (Undefined != t) {
      if (call_profile->headerfilter != Undefined) {
	ERROR("call control instance '%s' changing headerfilter from %s to %s!\n",
	      cc_name.c_str(),
	      FilterType2String(call_profile->headerfilter),
	      FilterType2String(t));
	// stop call here???
      }
      call_profile->headerfilter = t;
      call_profile->headerfilter_list.clear();
      string hl;
      if (values.hasMember("header_list"))
	hl = values["header_list"].asCStr();

      vector<string> elems = explode(hl, "|");
      for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++) {
	transform(it->begin(), it->end(), it->begin(), ::tolower);
	call_profile->headerfilter_list.insert(*it);
      }

      DBG("call control '%s': set header filter '%s', list '%s'\n",
	  cc_name.c_str(), FilterType2String(t), hl.c_str());
    }
  }

  if (values.hasMember("messagefilter")) {
    string hf = values["messagefilter"].asCStr();
    FilterType t = String2FilterType(hf.c_str());
    if (Undefined != t) {
      if (call_profile->messagefilter != Undefined) {
	ERROR("call control instance '%s' changing messagefilter from %s to %s!\n",
	      cc_name.c_str(),
	      FilterType2String(call_profile->messagefilter),
	      FilterType2String(t));
	// stop call here???
      }
      call_profile->messagefilter = t;
      call_profile->messagefilter_list.clear();
      string hl;
      if (values.hasMember("message_list"))
	hl = values["message_list"].asCStr();

      vector<string> elems = explode(hl, "|");
      for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++) {
	call_profile->messagefilter_list.insert(*it);
      }

      DBG("call control '%s': set message filter '%s', list '%s'\n",
	  cc_name.c_str(), FilterType2String(t), hl.c_str());
    }
  }

}
