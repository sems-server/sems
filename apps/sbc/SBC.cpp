/*
 * Copyright (C) 2010-2011 Stefan Sayer
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

/* 
SBC - feature-wishlist
- accounting (MySQL DB, cassandra DB)
- RTP transcoding mode (bridging)
- overload handling (parallel call to target thresholds)
- call distribution
- select profile on monitoring in-mem DB record
 */
#include "SBC.h"

#include "SBCCallControlAPI.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmSipHeaders.h"
#include "SBCSimpleRelay.h"
#include "RegisterDialog.h"
#include "SubscriptionDialog.h"
#include "sip/pcap_logger.h"
#include "sip/sip_parser.h"
#include "sip/sip_trans.h"

#include "HeaderFilter.h"
#include "ParamReplacer.h"
#include "SDPFilter.h"
#include "SBCCallLeg.h"

#include "AmEventQueueProcessor.h"

#include "SubscriptionDialog.h"
#include "RegisterDialog.h"
#include "RegisterCache.h"

#include <algorithm>

using std::map;

#define MOD_NAME "sbc"


EXPORT_MODULE_FACTORY(SBCFactory);
DEFINE_MODULE_INSTANCE(SBCFactory, MOD_NAME);

// helper functions

bool getCCInterfaces(CCInterfaceListT& cc_interfaces, vector<AmDynInvoke*>& cc_modules)
{
  for (CCInterfaceListIteratorT cc_it = cc_interfaces.begin();
       cc_it != cc_interfaces.end(); cc_it++) {
    string& cc_module = cc_it->cc_module;
    if (cc_module.empty()) {
      ERROR("using call control but empty cc_module for '%s'!\n", 
	    cc_it->cc_name.c_str());
      return false;
    }

    AmDynInvokeFactory* cc_fact = AmPlugIn::instance()->getFactory4Di(cc_module);
    if (NULL == cc_fact) {
      ERROR("cc_module '%s' not loaded\n", cc_module.c_str());
      return false;
    }

    AmDynInvoke* cc_di = cc_fact->getInstance();
    if(NULL == cc_di) {
      ERROR("could not get a DI reference\n");
      return false;
    }
    cc_modules.push_back(cc_di);
  }
  return true;
}

void assertEndCRLF(string& s) {
  if (s[s.size()-2] != '\r' ||
      s[s.size()-1] != '\n') {
    while ((s[s.size()-1] == '\r') ||
	   (s[s.size()-1] == '\n'))
      s.erase(s.size()-1);
    s += "\r\n";
  }
}

///////////////////////////////////////////////////////////////////////////////////////////

SBCCallLeg* CallLegCreator::create(const SBCCallProfile& call_profile)
{
  return new SBCCallLeg(call_profile, new AmSipDialog());
}

SBCCallLeg* CallLegCreator::create(SBCCallLeg* caller)
{
  return new SBCCallLeg(caller);
}

SimpleRelayCreator::Relay 
SimpleRelayCreator::createRegisterRelay(SBCCallProfile& call_profile,
					vector<AmDynInvoke*> &cc_modules)
{
  return SimpleRelayCreator::Relay(new RegisterDialog(call_profile, cc_modules),
				   new RegisterDialog(call_profile, cc_modules));
}

SimpleRelayCreator::Relay
SimpleRelayCreator::createSubscriptionRelay(SBCCallProfile& call_profile,
					    vector<AmDynInvoke*> &cc_modules)
{
  return SimpleRelayCreator::Relay(new SubscriptionDialog(call_profile, cc_modules),
				   new SubscriptionDialog(call_profile, cc_modules));
}

SimpleRelayCreator::Relay
SimpleRelayCreator::createGenericRelay(SBCCallProfile& call_profile,
				       vector<AmDynInvoke*> &cc_modules)
{
  return SimpleRelayCreator::Relay(new SimpleRelayDialog(call_profile, cc_modules),
				   new SimpleRelayDialog(call_profile, cc_modules));
}

SBCFactory::SBCFactory(const string& _app_name)
  : AmSessionFactory(_app_name), 
    AmDynInvokeFactory(_app_name),
    callLegCreator(new CallLegCreator()),
    simpleRelayCreator(new SimpleRelayCreator())
{
}

SBCFactory::~SBCFactory() {
  RegisterCache::dispose();
}

int SBCFactory::onLoad()
{
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    ERROR("No configuration for sbc present (%s)\n",
	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str()
	 );
    return -1;
  }

  string load_cc_plugins = cfg.getParameter("load_cc_plugins");
  if (!load_cc_plugins.empty()) {
    INFO("loading call control plugins '%s' from '%s'\n",
	 load_cc_plugins.c_str(), AmConfig::PlugInPath.c_str());
    if (AmPlugIn::instance()->load(AmConfig::PlugInPath, load_cc_plugins) < 0) {
      ERROR("loading call control plugins '%s' from '%s'\n",
	    load_cc_plugins.c_str(), AmConfig::PlugInPath.c_str());
      return -1;
    }
  }

  session_timer_fact = AmPlugIn::instance()->getFactory4Seh("session_timer");
  if(!session_timer_fact) {
    WARN("session_timer plug-in not loaded - "
	 "SIP Session Timers will not be supported\n");
  }

  vector<string> profiles_names = explode(cfg.getParameter("profiles"), ",");
  for (vector<string>::iterator it =
	 profiles_names.begin(); it != profiles_names.end(); it++) {
    string profile_file_name = AmConfig::ModConfigPath + *it + ".sbcprofile.conf";
    if (!call_profiles[*it].readFromConfiguration(*it, profile_file_name)) {
      ERROR("configuring SBC call profile from '%s'\n", profile_file_name.c_str());
      return -1;
    }
  }

  active_profile = explode(cfg.getParameter("active_profile"), ",");
  if (active_profile.empty()) {
    ERROR("active_profile not set.\n");
    return -1;
  }

  string active_profile_s;
  for (vector<string>::iterator it =
	 active_profile.begin(); it != active_profile.end(); it++) {
    if (it->empty())
      continue;
    if (((*it)[0] != '$') && call_profiles.find(*it) == call_profiles.end()) {
      ERROR("call profile active_profile '%s' not loaded!\n", it->c_str());
      return -1;
    }
    active_profile_s+=*it;
    if (it != active_profile.end()-1)
      active_profile_s+=", ";
  }

  INFO("SBC: active profile: '%s'\n", active_profile_s.c_str());

  vector<string> regex_maps = explode(cfg.getParameter("regex_maps"), ",");
  for (vector<string>::iterator it =
	 regex_maps.begin(); it != regex_maps.end(); it++) {
    string regex_map_file_name = AmConfig::ModConfigPath + *it + ".conf";
    RegexMappingVector v;
    if (!read_regex_mapping(regex_map_file_name, "=>",
			    ("SBC regex mapping " + *it+":").c_str(), v)) {
      ERROR("reading regex mapping from '%s'\n", regex_map_file_name.c_str());
      return -1;
    }
    regex_mappings.setRegexMap(*it, v);
    INFO("loaded regex mapping '%s'\n", it->c_str());
  }

  if (!AmPlugIn::registerApplication(MOD_NAME, this)) {
    ERROR("registering "MOD_NAME" application\n");
    return -1;
  }

  if (!AmPlugIn::registerDIInterface(MOD_NAME, this)) {
    ERROR("registering "MOD_NAME" DI interface\n");
    return -1;
  }

  // TODO: add config param for the number of threads
  subnot_processor.addThreads(1);
  RegisterCache::instance()->start();

  return 0;
}

/** get the first matching profile name from active profiles */
SBCCallProfile* SBCFactory::getActiveProfileMatch(const AmSipRequest& req,
						  ParamReplacerCtx& ctx) 
{
  string profile, profile_rule;
  vector<string>::const_iterator it = active_profile.begin();
  for (; it != active_profile.end(); it++) {

    if (it->empty())
      continue;

    if (*it == "$(paramhdr)")
      profile = get_header_keyvalue(ctx.app_param,"profile");
    else if (*it == "$(ruri.user)")
      profile = req.user;
    else
      profile = ctx.replaceParameters(*it, "active_profile", req);

    if (!profile.empty()) {
      profile_rule = *it;
      break;
    }
  }

  DBG("active profile = %s\n", profile.c_str());

  map<string, SBCCallProfile>::iterator prof_it = call_profiles.find(profile);
  if (prof_it==call_profiles.end()) {
    ERROR("could not find call profile '%s'"
	  " (matching active_profile rule: '%s')\n",
	  profile.c_str(), profile_rule.c_str());

    return NULL;
  }

  DBG("using call profile '%s' (from matching active_profile rule '%s')\n",
      profile.c_str(), profile_rule.c_str());

  return &prof_it->second;
}


AmSession* SBCFactory::onInvite(const AmSipRequest& req, const string& app_name,
				const map<string,string>& app_params)
{
  ParamReplacerCtx ctx;
  ctx.app_param = getHeader(req.hdrs, PARAM_HDR, true);

  profiles_mut.lock();
  const SBCCallProfile* p_call_profile = getActiveProfileMatch(req, ctx);
  if(!p_call_profile) {
    profiles_mut.unlock();
    throw AmSession::Exception(500,SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  const SBCCallProfile& call_profile = *p_call_profile;
  if(!call_profile.refuse_with.empty()) {
    if(call_profile.refuse(ctx, req) < 0) {
      profiles_mut.unlock();
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
    profiles_mut.unlock();
    return NULL;
  }

  SBCCallLeg* b2b_dlg = callLegCreator->create(call_profile);

  if (call_profile.auth_aleg_enabled) {
    // adding auth handler
    AmSessionEventHandlerFactory* uac_auth_f =
      AmPlugIn::instance()->getFactory4Seh("uac_auth");
    if (NULL == uac_auth_f)  {
      INFO("uac_auth module not loaded. uac auth for caller session NOT enabled.\n");
    } else {
      AmSessionEventHandler* h = uac_auth_f->getHandler(b2b_dlg);

      // we cannot use the generic AmSessionEventHandler hooks,
      // because the hooks don't work in AmB2BSession
      b2b_dlg->setAuthHandler(h);
      DBG("uac auth enabled for caller session.\n");
    }
  }
  profiles_mut.unlock();

  return b2b_dlg;
}

/** out-of-dialog request handling terminated */
void oodHandlingTerminated(const AmSipRequest &req, vector<AmDynInvoke*>& cc_modules, SBCCallProfile& call_profile)
{
  for (vector<AmDynInvoke*>::iterator m = cc_modules.begin(); m != cc_modules.end(); ++m) {
    AmArg args,ret;
    args.push((AmObject*)&call_profile);
    args.push((AmObject*)&req);
    try {
      (*m)->invoke("ood_handling_terminated", args, ret);
    } catch (...) { /* ignore */ }
  }
}

void SBCFactory::onOoDRequest(const AmSipRequest& req)
{
  DBG("processing message %s %s\n", req.method.c_str(), req.r_uri.c_str());  
  profiles_mut.lock();

  ParamReplacerCtx ctx;
  ctx.app_param = getHeader(req.hdrs, PARAM_HDR, true);

  string profile_rule;
  const SBCCallProfile* p_call_profile = getActiveProfileMatch(req, ctx);
  if(!p_call_profile) {
    profiles_mut.unlock();
    throw AmSession::Exception(500,SIP_REPLY_SERVER_INTERNAL_ERROR);
  }
  
  SBCCallProfile call_profile(*p_call_profile);
  profiles_mut.unlock();

  ctx.call_profile = &call_profile;
  call_profile.eval_cc_list(ctx,req);

  vector<AmDynInvoke*> cc_modules;
  if(!getCCInterfaces(call_profile.cc_interfaces,cc_modules)) {
    ERROR("could not get CC interfaces\n");
    return;
  }

  msg_logger *logger = NULL;

  // fix up variables
  call_profile.replace_cc_values(ctx,req,NULL);
  if(!SBCFactory::CCRoute(req,cc_modules,call_profile, logger)) {
    oodHandlingTerminated(req, cc_modules, call_profile);
    //ERROR("routing failed\n");
    if (logger) delete logger;
    return;
  }

  if (!call_profile.refuse_with.empty()) {
    oodHandlingTerminated(req, cc_modules, call_profile);
    if (logger) delete logger;
    if(call_profile.refuse(ctx, req) < 0) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
    return;
  }
  
  call_profile.fix_append_hdrs(ctx, req);

  SimpleRelayCreator::Relay relay(NULL,NULL);
  if(req.method == SIP_METH_REGISTER) {
    relay = simpleRelayCreator->createRegisterRelay(call_profile, cc_modules);
  }
  else if((req.method == SIP_METH_SUBSCRIBE) ||
	  (req.method == SIP_METH_REFER)){

    relay = simpleRelayCreator->createSubscriptionRelay(call_profile, cc_modules);
  }
  else {
    relay = simpleRelayCreator->createGenericRelay(call_profile, cc_modules);
  }
  if (logger) inc_ref(logger);
  if (call_profile.log_sip) {
    relay.first->setMsgLogger(logger);
    relay.second->setMsgLogger(logger);
  }

  if(SBCSimpleRelay::start(relay,req,call_profile)) {
    AmSipDialog::reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR, 
			     "", call_profile.log_sip ? logger: NULL);
    delete relay.first;
    delete relay.second;
  }
  if (logger) dec_ref(logger);
}

void SBCFactory::invoke(const string& method, const AmArg& args, 
				AmArg& ret)
{
  if (method == "listProfiles"){
    listProfiles(args, ret);
  } else if (method == "reloadProfiles"){
    reloadProfiles(args,ret);
  } else if (method == "loadProfile"){
    args.assertArrayFmt("u");
    loadProfile(args,ret);
  } else if (method == "reloadProfile"){
    args.assertArrayFmt("u");
    reloadProfile(args,ret);
  } else if (method == "getActiveProfile"){
    getActiveProfile(args,ret);
  } else if (method == "setActiveProfile"){
    args.assertArrayFmt("u");
    setActiveProfile(args,ret);
  } else if (method == "getRegexMapNames"){
    getRegexMapNames(args,ret);
  } else if (method == "setRegexMap"){
    args.assertArrayFmt("u");
    setRegexMap(args,ret);
  } else if (method == "loadCallcontrolModules"){
    args.assertArrayFmt("s");
    loadCallcontrolModules(args,ret);
  } else if (method == "postControlCmd"){
    args.assertArrayFmt("ss"); // at least call-ltag, cmd
    postControlCmd(args,ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("listProfiles"));
    ret.push(AmArg("reloadProfiles"));
    ret.push(AmArg("reloadProfile"));
    ret.push(AmArg("loadProfile"));
    ret.push(AmArg("getActiveProfile"));
    ret.push(AmArg("setActiveProfile"));
    ret.push(AmArg("getRegexMapNames"));
    ret.push(AmArg("setRegexMap"));
    ret.push(AmArg("loadCallcontrolModules"));
    ret.push(AmArg("postControlCmd"));
    ret.push(AmArg("printCallStats"));
  } else if(method == "printCallStats"){ 
    B2BMediaStatistics::instance()->getReport(args, ret);
  }  else
    throw AmDynInvoke::NotImplemented(method);
}

void SBCFactory::listProfiles(const AmArg& args, AmArg& ret) {
  profiles_mut.lock();
  for (std::map<string, SBCCallProfile>::iterator it=
	 call_profiles.begin(); it != call_profiles.end(); it++) {
    AmArg p;
    p["name"] = it->first;
    p["md5"] = it->second.md5hash;
    p["path"] = it->second.profile_file;
    ret.push((p));
  }
  profiles_mut.unlock();
}

void SBCFactory::reloadProfiles(const AmArg& args, AmArg& ret) {
  std::map<string, SBCCallProfile> new_call_profiles;
  
  bool failed = false;
  string res = "OK";
  AmArg profile_list;
  profiles_mut.lock();
  for (std::map<string, SBCCallProfile>::iterator it=
	 call_profiles.begin(); it != call_profiles.end(); it++) {
    new_call_profiles[it->first] = SBCCallProfile();
    if (!new_call_profiles[it->first].readFromConfiguration(it->first,
							    it->second.profile_file)) {
      ERROR("reading call profile file '%s'\n", it->second.profile_file.c_str());
      res = "Error reading call profile for "+it->first+" from "+it->second.profile_file+
	+"; no profiles reloaded";
      failed = true;
      break;
    }
    AmArg p;
    p["name"] = it->first;
    p["md5"] = it->second.md5hash;
    p["path"] = it->second.profile_file;
    profile_list.push(p);
  }
  if (!failed) {
    call_profiles = new_call_profiles;
    ret.push(200);
  } else {
    ret.push(500);
  }
  ret.push(res);
  ret.push(profile_list);
  profiles_mut.unlock();
}

void SBCFactory::reloadProfile(const AmArg& args, AmArg& ret) {
  bool failed = false;
  string res = "OK";
  AmArg p;
  if (!args[0].hasMember("name")) {
    ret.push(400);
    ret.push("Parameters error: expected ['name': profile_name] ");
    return;
  }

  profiles_mut.lock();
  std::map<string, SBCCallProfile>::iterator it=
    call_profiles.find(args[0]["name"].asCStr());
  if (it == call_profiles.end()) {
    res = "profile '"+string(args[0]["name"].asCStr())+"' not found";
    failed = true;
  } else {
    SBCCallProfile new_cp;
    if (!new_cp.readFromConfiguration(it->first, it->second.profile_file)) {
      ERROR("reading call profile file '%s'\n", it->second.profile_file.c_str());
      res = "Error reading call profile for "+it->first+" from "+it->second.profile_file;
      failed = true;
    } else {
      it->second = new_cp;
      p["name"] = it->first;
      p["md5"] = it->second.md5hash;
      p["path"] = it->second.profile_file;
    }
  }
  profiles_mut.unlock();

  if (!failed) {
    ret.push(200);
    ret.push(res);
    ret.push(p);
  } else {
    ret.push(500);
    ret.push(res);
  }
}

void SBCFactory::loadProfile(const AmArg& args, AmArg& ret) {
  if (!args[0].hasMember("name") || !args[0].hasMember("path")) {
    ret.push(400);
    ret.push("Parameters error: expected ['name': profile_name] "
	     "and ['path': profile_path]");
    return;
  }
  SBCCallProfile cp;
  if (!cp.readFromConfiguration(args[0]["name"].asCStr(), args[0]["path"].asCStr())) {
    ret.push(500);
    ret.push("Error reading sbc call profile for "+string(args[0]["name"].asCStr())+
	     " from file "+string(args[0]["path"].asCStr()));
    return;
  }

  profiles_mut.lock();
  call_profiles[args[0]["name"].asCStr()] = cp;
  profiles_mut.unlock();
  ret.push(200);
  ret.push("OK");
  AmArg p;
  p["name"] = args[0]["name"];
  p["md5"] = cp.md5hash;
  p["path"] = args[0]["path"];
  ret.push(p);
}

void SBCFactory::getActiveProfile(const AmArg& args, AmArg& ret) {
  profiles_mut.lock();
  AmArg p;
  for (vector<string>::iterator it=active_profile.begin();
       it != active_profile.end(); it++) {
    p["active_profile"].push(*it);
  }
  profiles_mut.unlock();
  ret.push(200);
  ret.push("OK");
  ret.push(p);
}

void SBCFactory::setActiveProfile(const AmArg& args, AmArg& ret) {
  if (!args[0].hasMember("active_profile")) {
    ret.push(400);
    ret.push("Parameters error: expected ['active_profile': <active_profile list>] ");
    return;
  }
  profiles_mut.lock();
  active_profile = explode(args[0]["active_profile"].asCStr(), ",");
  profiles_mut.unlock();
  ret.push(200);
  ret.push("OK");
  AmArg p;
  p["active_profile"] = args[0]["active_profile"];
  ret.push(p);  
}

void SBCFactory::getRegexMapNames(const AmArg& args, AmArg& ret) {
  AmArg p;
  vector<string> reg_names = regex_mappings.getNames();
  for (vector<string>::iterator it=reg_names.begin();
       it != reg_names.end(); it++) {
    p["regex_maps"].push(*it);
  }
  ret.push(200);
  ret.push("OK");
  ret.push(p);
}

void SBCFactory::setRegexMap(const AmArg& args, AmArg& ret) {
  if (!args[0].hasMember("name") || !args[0].hasMember("file") ||
      !isArgCStr(args[0]["name"]) || !isArgCStr(args[0]["file"])) {
    ret.push(400);
    ret.push("Parameters error: expected ['name': <name>, 'file': <file name>]");
    return;
  }

  string m_name = args[0]["name"].asCStr();
  string m_file = args[0]["file"].asCStr();
  RegexMappingVector v;
  if (!read_regex_mapping(m_file, "=>", "SBC regex mapping", v)) {
    ERROR("reading regex mapping from '%s'\n", m_file.c_str());
    ret.push(401);
    ret.push("Error reading regex mapping from file");
    return;
  }
  regex_mappings.setRegexMap(m_name, v);
  ret.push(200);
  ret.push("OK");
}

void SBCFactory::loadCallcontrolModules(const AmArg& args, AmArg& ret) {
  string load_cc_plugins = args[0].asCStr();
  if (!load_cc_plugins.empty()) {
    INFO("loading call control plugins '%s' from '%s'\n",
	 load_cc_plugins.c_str(), AmConfig::PlugInPath.c_str());
    if (AmPlugIn::instance()->load(AmConfig::PlugInPath, load_cc_plugins) < 0) {
      ERROR("loading call control plugins '%s' from '%s'\n",
	    load_cc_plugins.c_str(), AmConfig::PlugInPath.c_str());
      
      ret.push(500);
      ret.push("Failed - please see server logs\n");
      return;
    }
  }
  ret.push(200);
  ret.push("OK");
}

void SBCFactory::postControlCmd(const AmArg& args, AmArg& ret) {
  SBCControlEvent* evt;
  if (args.size()<3) {
    evt = new SBCControlEvent(args[1].asCStr());
  } else {
    evt = new SBCControlEvent(args[1].asCStr(), args[2]);
  }
  if (!AmSessionContainer::instance()->postEvent(args[0].asCStr(), evt)) {
    ret.push(404);
    ret.push("Not found");
  } else {
    ret.push(202);
    ret.push("Accepted");
  }
}

bool SBCFactory::CCRoute(const AmSipRequest& req,
			 vector<AmDynInvoke*>& cc_modules,
			 SBCCallProfile& call_profile,
                         msg_logger* &logger)
{
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);
    di_args.push(""); //getLocalTag()
    di_args.push((AmObject*)&call_profile);
    di_args.push((AmObject*)&req);
    di_args.push(AmArg());
    di_args.back().push((int) 0);
    di_args.back().push((int) 0);

    di_args.push(AmArg());
    AmArg& vals = di_args.back();
    vals.assertStruct();
    for (map<string, string>::iterator it = cc_if.cc_values.begin();
	 it != cc_if.cc_values.end(); it++) {
      vals[it->first] = it->second;
    }

    di_args.push(0); // current timer ID

    try {
      (*cc_mod)->invoke("route", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface route "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      AmBasicSipDialog::reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      return false;
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface route "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      AmBasicSipDialog::reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      return false;
    }

    if (!logger && !call_profile.msg_logger_path.empty()) {

      ParamReplacerCtx ctx(&call_profile);
      call_profile.msg_logger_path = 
	ctx.replaceParameters(call_profile.msg_logger_path,
			      "msg_logger_path",req);

      if(!call_profile.msg_logger_path.empty()) {
	pcap_logger *log = new pcap_logger();
	if(log->open(call_profile.msg_logger_path.c_str())) {
	  ERROR("could not open message logger\n");
          delete log;
	  call_profile.msg_logger_path.clear();
	}
        else logger = log;
      }

      if(logger && call_profile.log_sip) {
        req.tt.lock_bucket();
        const sip_trans* t = req.tt.get_trans();
        if (t) {
          sip_msg* msg = t->msg;
          logger->log(msg->buf,msg->len,&msg->remote_ip,
              &msg->local_ip,msg->u.request->method_str);
        }
        req.tt.unlock_bucket();
      }
    }

    // evaluate ret
    if (isArgArray(ret)) {
      for (size_t i=0;i<ret.size();i++) {
	if (!isArgArray(ret[i]) || !ret[i].size())
	  continue;
	if (!isArgInt(ret[i][SBC_CC_ACTION])) {
	  ERROR("in call control module '%s' - action type not int\n",
		cc_if.cc_name.c_str());
	  continue;
	}
	switch (ret[i][SBC_CC_ACTION].asInt()) {
	case SBC_CC_DROP_ACTION: {
	  DBG("dropping call on call control action DROP from '%s'\n",
	      cc_if.cc_name.c_str());
	  return false;
	}

	case SBC_CC_REFUSE_ACTION: {
	  if (ret[i].size() < 3 ||
	      !isArgInt(ret[i][SBC_CC_REFUSE_CODE]) ||
	      !isArgCStr(ret[i][SBC_CC_REFUSE_REASON])) {
	    ERROR("in call control module '%s' - REFUSE"
		  " action parameters missing/wrong: '%s'\n",
		  cc_if.cc_name.c_str(), AmArg::print(ret[i]).c_str());
	    continue;
	  }
	  string headers;
	  if (ret[i].size() > SBC_CC_REFUSE_HEADERS) {
	    for (size_t h=0;h<ret[i][SBC_CC_REFUSE_HEADERS].size();h++)
	      headers += string(ret[i][SBC_CC_REFUSE_HEADERS][h].asCStr()) + CRLF;
	  }

	  DBG("replying with %d %s on call control action "
	      "REFUSE from '%s' headers='%s'\n",
	      ret[i][SBC_CC_REFUSE_CODE].asInt(), 
	      ret[i][SBC_CC_REFUSE_REASON].asCStr(),
	      cc_if.cc_name.c_str(), headers.c_str());

	  AmBasicSipDialog::reply_error(req,
	        ret[i][SBC_CC_REFUSE_CODE].asInt(), 
		ret[i][SBC_CC_REFUSE_REASON].asCStr(),
		headers, call_profile.log_sip ? logger: NULL);

	  return false;
	}

	case SBC_CC_SET_CALL_TIMER_ACTION:
          // just ignore
          continue;

	default: {
	  ERROR("unknown call control action: '%s'\n", 
		AmArg::print(ret[i]).c_str());
	  continue;
	}

	}

      }
    }

    cc_mod++;
  }

  return true;
}

