/*
 * Copyright (C) 2011 Stefan Sayer
 * Copyright (C) 2012 FRAFOS GmbH
 * Copyright (C) 2014 Stefan Sayer
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
#include "RegisterCache.h"
#include "AmUriParser.h"

#include "Registrar.h"

#include "SBCCallControlAPI.h"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <fstream>

using namespace std;

class CCRegistrarFactory : public AmDynInvokeFactory
{
public:
    CCRegistrarFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return CCRegistrar::instance();
    }

    int onLoad(){
      if (CCRegistrar::instance()->onLoad())
	return -1;

      DBG("template call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCRegistrarFactory, MOD_NAME);

CCRegistrar* CCRegistrar::_instance=0;

CCRegistrar* CCRegistrar::instance()
{
    if(!_instance)
	_instance = new CCRegistrar();
    return _instance;
}

CCRegistrar::CCRegistrar()
{
}

CCRegistrar::~CCRegistrar() { }

int CCRegistrar::onLoad() {
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

void CCRegistrar::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("CCRegistrar: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if(method == "start"){

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());
    
    const AmSipRequest* msg = dynamic_cast<const AmSipRequest*>(args[CC_API_PARAMS_SIP_MSG].asObject());

    start(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	  args[CC_API_PARAMS_LTAG].asCStr(),
	  call_profile,
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_SEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_USEC].asInt(),
	  args[CC_API_PARAMS_CFGVALUES],
	  args[CC_API_PARAMS_TIMERID].asInt(),  ret, msg);

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

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    end(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	args[CC_API_PARAMS_LTAG].asCStr(),
	call_profile,
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
	);
  } else if (method == "route"){
    SBCCallProfile* call_profile =

      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    const AmSipRequest* msg = dynamic_cast<const AmSipRequest*>(args[CC_API_PARAMS_SIP_MSG].asObject());

    route(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(), call_profile, msg,
	  args[CC_API_PARAMS_CFGVALUES], ret);

  } else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
    ret.push("route");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

bool retarget(const string& r_uri, const AmArg& values, SBCCallProfile* call_profile) {
  bool nat_handling = true; // default
  bool sticky_iface = true; // default
  if (values.hasMember("nat_handling") && string(values["nat_handling"].asCStr())=="no") {
    nat_handling = false;
  }
  if (values.hasMember("sticky_iface") && string(values["sticky_iface"].asCStr())=="no") {
    sticky_iface = false;
  }
  DBG("Registrar handling message to R-URI '%s'\n", r_uri.c_str());
  DBG("nat_handling: %sabled, sticky_iface: %sabled\n", nat_handling?"en":"dis", sticky_iface?"en":"dis");

  // should we check if the R-URI has already been changed?
  string aor = RegisterCache::canonicalize_aor(r_uri);
  RegisterCache* reg_cache = RegisterCache::instance();
    
  map<string,string> alias_map;
  if(!aor.empty() && reg_cache->getAorAliasMap(aor,alias_map) &&
     !alias_map.empty()) {

    AliasEntry alias_entry;
    const string& alias = alias_map.begin()->first; // no forking
    if(!alias.empty() && reg_cache->findAliasEntry(alias,alias_entry)) {

      call_profile->ruri = alias_entry.contact_uri;
      DBG("R-URI from location DB: '%s'\n", alias_entry.contact_uri.c_str());

      if (nat_handling) {
	call_profile->next_hop = alias_entry.source_ip;
	if(alias_entry.source_port != 5060)
	  call_profile->next_hop += ":" + int2str(alias_entry.source_port);
	call_profile->next_hop += "/" + alias_entry.trsp;
      }
    
      if (sticky_iface) {
	string out_if = AmConfig::SIP_Ifs[alias_entry.local_if].name;
	DBG("out_if = '%s'",out_if.c_str());
	
	call_profile->outbound_interface = out_if;
	DBG("setting from registration cache: outbound_interface='%s'\n",
	    call_profile->outbound_interface.c_str());
      }

      return true;
    }
  }

  DBG("No registration found for r-ruri '%s'/ aor '%s' in location database.\n",
      r_uri.c_str(), aor.c_str());

  return false;
}

#define REFUSE_WITH_404					\
  {							\
  res.push(AmArg());					\
  AmArg& res_cmd = res.back();				\
  res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;	\
  res_cmd[SBC_CC_REFUSE_CODE] = (int)404;		\
  res_cmd[SBC_CC_REFUSE_REASON] = "Not Found";		\
  }


void CCRegistrar::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res, const AmSipRequest* req) {
  if (NULL == req)
    REFUSE_WITH_404;

  if ((req->method == "INVITE") && (retarget(req->r_uri, values, call_profile))){  
      return;
  }

  REFUSE_WITH_404;
}

void CCRegistrar::connect(const string& cc_name, const string& ltag,
			 SBCCallProfile* call_profile,
			 const string& other_tag,
			 int connect_ts_sec, int connect_ts_usec) {
  DBG("call '%s' connecting\n", ltag.c_str());
}

void CCRegistrar::end(const string& cc_name, const string& ltag,
		     SBCCallProfile* call_profile,
		     int end_ts_sec, int end_ts_usec) {
  DBG("call '%s' ending\n", ltag.c_str());
}

void CCRegistrar::route(const string& cc_name,
			SBCCallProfile* call_profile, const AmSipRequest* ood_req,
			const AmArg& values, AmArg& res)
{
  DBG("CCRegistrar: route '%s %s'\n", ood_req->method.c_str(), ood_req->r_uri.c_str());
	
	
  if (ood_req->method == "REGISTER"){
    RegisterCacheCtx reg_cache_ctx;

    // reply 200 if possible, else continue
    bool replied = RegisterCache::instance()->saveSingleContact(reg_cache_ctx, *ood_req);

    if(replied) {
      DBG("replied!");
      res.push(AmArg());
      AmArg& res_cmd = res.back();
      res_cmd[SBC_CC_ACTION] = SBC_CC_DROP_ACTION;
    }
  } else {
    if (retarget(ood_req->r_uri, values, call_profile)){  
      return;
    }
    REFUSE_WITH_404;
    return;
  }
}

