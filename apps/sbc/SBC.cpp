/*
 * Copyright (C) 2010 Stefan Sayer
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
- fallback profile
 */
#include "SBC.h"

#include "ampi/SBCCallControlAPI.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmSipHeaders.h"

#include "HeaderFilter.h"
#include "ParamReplacer.h"
#include "SDPFilter.h"

using std::map;

AmConfigReader SBCFactory::cfg;
AmSessionEventHandlerFactory* SBCFactory::session_timer_fact = NULL;
RegexMapper SBCFactory::regex_mappings;

EXPORT_MODULE_FACTORY(SBCFactory);
DEFINE_MODULE_INSTANCE(SBCFactory, MOD_NAME);

SBCFactory::SBCFactory(const string& _app_name)
  : AmSessionFactory(_app_name), AmDynInvokeFactory(_app_name)
{
}

SBCFactory::~SBCFactory() {
}

int SBCFactory::onLoad()
{
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    ERROR("No configuration for sbc present (%s)\n",
	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str()
	 );
    return -1;
  }

  session_timer_fact = AmPlugIn::instance()->getFactory4Seh("session_timer");
  if(!session_timer_fact) {
    ERROR("could not load session_timer from session_timer plug-in\n");
    return -1;
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

  return 0;
}

#define REPLACE_VALS req, app_param, ruri_parser, from_parser, to_parser

/** get the first matching profile name from active profiles */
string SBCFactory::getActiveProfileMatch(string& profile_rule, const AmSipRequest& req,
					 const string& app_param, AmUriParser& ruri_parser,
					 AmUriParser& from_parser, AmUriParser& to_parser) {
  string res;
  for (vector<string>::iterator it=
	 active_profile.begin(); it != active_profile.end(); it++) {
    if (it->empty())
      continue;

    if (*it == "$(paramhdr)")
      res = get_header_keyvalue(app_param,"profile");
    else if (*it == "$(ruri.user)")
      res = req.user;
    else
      res = replaceParameters(*it, "active_profile", REPLACE_VALS);

    if (!res.empty()) {
      profile_rule = *it;    
      break;
    }
  }
  return res;
}

AmSession* SBCFactory::onInvite(const AmSipRequest& req, const string& app_name,
				const map<string,string>& app_params)
{
  AmUriParser ruri_parser, from_parser, to_parser;

  profiles_mut.lock();

  string app_param = getHeader(req.hdrs, PARAM_HDR, true);
  
  string profile_rule;
  string profile = getActiveProfileMatch(profile_rule, REPLACE_VALS);

  map<string, SBCCallProfile>::iterator it=
    call_profiles.find(profile);
  if (it==call_profiles.end()) {
    profiles_mut.unlock();
    ERROR("could not find call profile '%s' (matching active_profile rule: '%s')\n",
	  profile.c_str(), profile_rule.c_str());
    throw AmSession::Exception(500,SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  DBG("using call profile '%s' (from matching active_profile rule '%s')\n",
      profile.c_str(), profile_rule.c_str());
  const SBCCallProfile& call_profile = it->second;

  if (!call_profile.refuse_with.empty()) {
    string refuse_with = replaceParameters(call_profile.refuse_with,
					   "refuse_with", REPLACE_VALS);

    if (refuse_with.empty()) {
      ERROR("refuse_with empty after replacing (was '%s' in profile %s)\n",
	    call_profile.refuse_with.c_str(), call_profile.profile_file.c_str());
      profiles_mut.unlock();
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    size_t spos = refuse_with.find(' ');
    unsigned int refuse_with_code;
    if (spos == string::npos || spos == refuse_with.size() ||
	str2i(refuse_with.substr(0, spos), refuse_with_code)) {
      ERROR("invalid refuse_with '%s'->'%s' in  %s. Expected <code> <reason>\n",
	    call_profile.refuse_with.c_str(), refuse_with.c_str(),
	    call_profile.profile_file.c_str());
      profiles_mut.unlock();
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
    string refuse_with_reason = refuse_with.substr(spos+1);

    string hdrs = replaceParameters(call_profile.append_headers,
				    "append_headers", REPLACE_VALS);
    profiles_mut.unlock();

    if (hdrs.size()>2)
      assertEndCRLF(hdrs);

    DBG("refusing call with %u %s\n", refuse_with_code, refuse_with_reason.c_str());
    AmSipDialog::reply_error(req, refuse_with_code, refuse_with_reason, hdrs);

    return NULL;
  }

  // copy so call profile's object does not get modified
  AmConfigReader sst_a_cfg = call_profile.sst_a_cfg;

  bool sst_a_enabled;

  string enable_aleg_session_timer =
    replaceParameters(call_profile.sst_aleg_enabled, "enable_aleg_session_timer", REPLACE_VALS);

  if (enable_aleg_session_timer.empty()) {
    string sst_enabled =
      replaceParameters(call_profile.sst_enabled, "enable_session_timer", REPLACE_VALS);
    sst_a_enabled = sst_enabled == "yes";
  } else {
    sst_a_enabled = enable_aleg_session_timer == "yes";
  }

  if (sst_a_enabled) {
    DBG("Enabling SIP Session Timers (A leg)\n");

#define SST_CFG_REPLACE_PARAMS(cfgkey)					\
    if (sst_a_cfg.hasParameter(cfgkey)) {					\
      string newval = replaceParameters(sst_a_cfg.getParameter(cfgkey),	\
					cfgkey, REPLACE_VALS);		\
      if (newval.empty()) {						\
	sst_a_cfg.eraseParameter(cfgkey);					\
      } else{								\
	sst_a_cfg.setParameter(cfgkey,newval);				\
      }									\
    }

    SST_CFG_REPLACE_PARAMS("session_expires");
    SST_CFG_REPLACE_PARAMS("minimum_timer");
    SST_CFG_REPLACE_PARAMS("maximum_timer");
    SST_CFG_REPLACE_PARAMS("session_refresh_method");
    SST_CFG_REPLACE_PARAMS("accept_501_reply");
#undef SST_CFG_REPLACE_PARAMS

    try {
      if (!session_timer_fact->onInvite(req, sst_a_cfg)) {
	profiles_mut.unlock();
	throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      }
    } catch (const AmSession::Exception& e) {
      profiles_mut.unlock();
      throw;
    }
  }

  SBCDialog* b2b_dlg = new SBCDialog(call_profile);

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

  if (sst_a_enabled) {
    AmSessionEventHandler* h = session_timer_fact->getHandler(b2b_dlg);
    if(!h) {
      profiles_mut.unlock();
      delete b2b_dlg;
      ERROR("could not get a session timer event handler\n");
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    if (h->configure(sst_a_cfg)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      b2b_dlg->addHandler(h);
    }
  }
  profiles_mut.unlock();

  return b2b_dlg;
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
  } else if(method == "_list"){ 
    ret.push(AmArg("listProfiles"));
    ret.push(AmArg("reloadProfiles"));
    ret.push(AmArg("reloadProfile"));
    ret.push(AmArg("loadProfile"));
    ret.push(AmArg("getActiveProfile"));
    ret.push(AmArg("setActiveProfile"));
    ret.push(AmArg("getRegexMapNames"));
    ret.push(AmArg("setRegexMap"));
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

SBCDialog::SBCDialog(const SBCCallProfile& call_profile)
  : m_state(BB_Init),
    prepaid_acc(NULL),
    cdr_module(NULL),
    auth(NULL),
    call_profile(call_profile),
    outbound_interface(-1),
    cc_timer_id(SBC_TIMER_ID_CALL_TIMERS_START)
{
  set_sip_relay_only(false);
  dlg.reliable_1xx = REL100_IGNORED;

  memset(&prepaid_acc_start, 0, sizeof(struct timeval));
  memset(&prepaid_acc_end, 0, sizeof(struct timeval));
}


SBCDialog::~SBCDialog()
{
  if (auth)
    delete auth;
}

UACAuthCred* SBCDialog::getCredentials() {
  return &call_profile.auth_aleg_credentials;
}

void SBCDialog::onInvite(const AmSipRequest& req)
{
  AmUriParser ruri_parser, from_parser, to_parser;

  DBG("processing initial INVITE\n");

  string app_param = getHeader(req.hdrs, PARAM_HDR, true);

  // process call control
  if (call_profile.cc_interfaces.size()) {
    for (vector<CCInterface>::iterator cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {
      CCInterface& cc_if = *cc_it;
      cc_if.cc_module =
	replaceParameters(cc_if.cc_module, "cc_module", REPLACE_VALS);
    }

    if (!getCCInterfaces()) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    for (vector<CCInterface>::iterator cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {
      CCInterface& cc_if = *cc_it;

      DBG("processing replacements for call control interface '%s'\n", cc_if.cc_name.c_str());

      for (map<string, string>::iterator it = cc_if.cc_values.begin();
	   it != cc_if.cc_values.end(); it++) {
	it->second =
	  replaceParameters(it->second, it->first.c_str(), REPLACE_VALS);
      }
    }

    if (!CCStart(req)) {
      setStopped();
      return;
    }
  }

  if(dlg.reply(req, 100, "Connecting") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  ruri = call_profile.ruri.empty() ? 
    req.r_uri : replaceParameters(call_profile.ruri, "RURI", REPLACE_VALS);

  from = call_profile.from.empty() ? 
    req.from : replaceParameters(call_profile.from, "From", REPLACE_VALS);

  to = call_profile.to.empty() ? 
    req.to : replaceParameters(call_profile.to, "To", REPLACE_VALS);

  if (!call_profile.contact.empty()) {
    call_profile.contact =
      replaceParameters(call_profile.contact, "Contact", REPLACE_VALS);
  }

  callid = call_profile.callid.empty() ?
    "" : replaceParameters(call_profile.callid, "Call-ID", REPLACE_VALS);

  if (!call_profile.outbound_proxy.empty()) {
      call_profile.outbound_proxy =
      replaceParameters(call_profile.outbound_proxy, "outbound_proxy", REPLACE_VALS);
      DBG("set outbound proxy to '%s'\n", call_profile.outbound_proxy.c_str());
  }

  if (!call_profile.next_hop_ip.empty()) {
    call_profile.next_hop_ip =
      replaceParameters(call_profile.next_hop_ip, "next_hop_ip", REPLACE_VALS);
    DBG("set next hop ip to '%s'\n", call_profile.next_hop_ip.c_str());

    if (!call_profile.next_hop_port.empty()) {
      call_profile.next_hop_port =
	replaceParameters(call_profile.next_hop_port, "next_hop_port", REPLACE_VALS);
      unsigned int nh_port_i;
      if (str2i(call_profile.next_hop_port, nh_port_i)) {
	ERROR("next hop port '%s' not understood\n", call_profile.next_hop_port.c_str());
	throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      }
      call_profile.next_hop_port_i = nh_port_i;
      DBG("set next hop port to '%u'\n", call_profile.next_hop_port_i);

      if (!call_profile.next_hop_for_replies.empty()) {
	call_profile.next_hop_for_replies =
	  replaceParameters(call_profile.next_hop_for_replies, "next_hop_for_replies",
			    REPLACE_VALS);
      }
    }
  }

  if (call_profile.rtprelay_enabled) {
    DBG("Enabling RTP relay mode for SBC call\n");

    // force symmetric RTP?
    if (!call_profile.force_symmetric_rtp.empty()) {
      string force_symmetric_rtp =
	replaceParameters(call_profile.force_symmetric_rtp, "force_symmetric_rtp",
			  REPLACE_VALS);
      if (!force_symmetric_rtp.empty() && force_symmetric_rtp != "no"
	  && force_symmetric_rtp != "0") {
	DBG("forcing symmetric RTP (passive mode)\n");
	rtp_relay_force_symmetric_rtp = true;
      }
    }
    // enable symmetric RTP by P-MsgFlags?
    if (!rtp_relay_force_symmetric_rtp) {
      if (call_profile.msgflags_symmetric_rtp) {
	string str_msg_flags = getHeader(req.hdrs,"P-MsgFlags", true);
	unsigned int msg_flags = 0;
	if(reverse_hex2int(str_msg_flags,msg_flags)){
	  ERROR("while parsing 'P-MsgFlags' header\n");
	  msg_flags = 0;
	}
	if (msg_flags & FL_FORCE_ACTIVE) {
	  DBG("P-MsgFlags indicates forced symmetric RTP (passive mode)");
	  rtp_relay_force_symmetric_rtp = true;
	}
      }
    }

    enableRtpRelay(req);
  }

  call_profile.sst_enabled =
    replaceParameters(call_profile.sst_enabled, "enable_session_timer", REPLACE_VALS);

  if (call_profile.sst_enabled == "yes") {
    AmConfigReader& sst_cfg = call_profile.sst_b_cfg;
#define SST_CFG_REPLACE_PARAMS(cfgkey)					\
    if (sst_cfg.hasParameter(cfgkey)) {					\
      string newval = replaceParameters(sst_cfg.getParameter(cfgkey),	\
					cfgkey, REPLACE_VALS);		\
      if (newval.empty()) {						\
	sst_cfg.eraseParameter(cfgkey);					\
      } else{								\
	sst_cfg.setParameter(cfgkey,newval);				\
      }									\
    }

    SST_CFG_REPLACE_PARAMS("session_expires");
    SST_CFG_REPLACE_PARAMS("minimum_timer");
    SST_CFG_REPLACE_PARAMS("maximum_timer");
    SST_CFG_REPLACE_PARAMS("session_refresh_method");
    SST_CFG_REPLACE_PARAMS("accept_501_reply");
#undef SST_CFG_REPLACE_PARAMS
  }

  m_state = BB_Dialing;

  invite_req = req;
  est_invite_cseq = req.cseq;

  removeHeader(invite_req.hdrs,PARAM_HDR);
  removeHeader(invite_req.hdrs,"P-App-Name");

  if (call_profile.sdpfilter_enabled) {
    b2b_mode = B2BMode_SDPFilter;
  }

  if (call_profile.sst_enabled == "yes") {
    removeHeader(invite_req.hdrs,SIP_HDR_SESSION_EXPIRES);
    removeHeader(invite_req.hdrs,SIP_HDR_MIN_SE);
  }

  inplaceHeaderFilter(invite_req.hdrs,
		      call_profile.headerfilter_list, call_profile.headerfilter);


  if (!call_profile.append_headers.empty()) {
    string append_headers = replaceParameters(call_profile.append_headers,
					      "append_headers", REPLACE_VALS);
    if (append_headers.size()>2) {
      assertEndCRLF(append_headers);
      invite_req.hdrs+=append_headers;
    }
  }

  if (call_profile.auth_enabled) {
    call_profile.auth_credentials.user =
      replaceParameters(call_profile.auth_credentials.user, "auth_user", REPLACE_VALS);
    call_profile.auth_credentials.pwd =
      replaceParameters(call_profile.auth_credentials.pwd, "auth_pwd", REPLACE_VALS);
  }

  if (call_profile.auth_aleg_enabled) {
    call_profile.auth_aleg_credentials.user =
      replaceParameters(call_profile.auth_aleg_credentials.user, "auth_aleg_user", REPLACE_VALS);
    call_profile.auth_aleg_credentials.pwd =
      replaceParameters(call_profile.auth_aleg_credentials.pwd, "auth_aleg_pwd", REPLACE_VALS);
  }

  if (!call_profile.outbound_interface.empty()) {
    call_profile.outbound_interface = 
      replaceParameters(call_profile.outbound_interface, "outbound_interface",
			REPLACE_VALS);

    if (!call_profile.outbound_interface.empty()) {
      if (call_profile.outbound_interface == "default")
	outbound_interface = 0;
      else {
	map<string,unsigned short>::iterator name_it =
	  AmConfig::If_names.find(call_profile.outbound_interface);
	if (name_it != AmConfig::If_names.end()) {
	  outbound_interface = name_it->second;
	} else {
	  ERROR("selected outbound_interface '%s' does not exist as an interface. "
		"Please check the 'additional_interfaces' "
		"parameter in the main configuration file.",
		call_profile.outbound_interface.c_str());
	  throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
	}
      }
    }
  }

  // get timer
  if (call_profile.call_timer_enabled || call_profile.prepaid_enabled) {
    if (!timersSupported()) {
      ERROR("load session_timer module for call timers\n");
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
  }

  if (call_profile.call_timer_enabled) {
    call_profile.call_timer =
      replaceParameters(call_profile.call_timer, "call_timer", REPLACE_VALS);
    if (str2i(call_profile.call_timer, call_timer)) {
      ERROR("invalid call_timer value '%s'\n", call_profile.call_timer.c_str());
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    if (!call_timer) {
      // time=0
      throw AmSession::Exception(503, "Service Unavailable");
    }
  }

  // prepaid
  if (call_profile.prepaid_enabled || call_profile.cdr_enabled) {
    gettimeofday(&prepaid_starttime, NULL);
  }

  if (call_profile.prepaid_enabled) {
    call_profile.prepaid_accmodule =
      replaceParameters(call_profile.prepaid_accmodule, "prepaid_accmodule", REPLACE_VALS);

    if (!getPrepaidInterface()) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    call_profile.prepaid_uuid =
      replaceParameters(call_profile.prepaid_uuid, "prepaid_uuid", REPLACE_VALS);

    call_profile.prepaid_acc_dest =
      replaceParameters(call_profile.prepaid_acc_dest, "prepaid_acc_dest", REPLACE_VALS);

    AmArg di_args,ret;
    di_args.push(call_profile.prepaid_uuid);
    di_args.push(call_profile.prepaid_acc_dest);
    di_args.push((int)prepaid_starttime.tv_sec);
    di_args.push(getCallID());
    di_args.push(getLocalTag());
    prepaid_acc->invoke("getCredit", di_args, ret);
    prepaid_credit = ret.get(0).asInt();
    if(prepaid_credit < 0) {
      ERROR("Failed to fetch credit from accounting module\n");
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }
    if (prepaid_credit == 0) {
      throw AmSession::Exception(402,"Insufficient Credit");
    }
  }

  // CDR
  if (call_profile.cdr_enabled) {
    call_profile.cdr_module =
      replaceParameters(call_profile.cdr_module, "cdr_module", REPLACE_VALS);

    if (!getCDRInterface()) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    for (map<string, string>::iterator it = call_profile.cdr_values.begin();
	 it != call_profile.cdr_values.end(); it++) {
      it->second =
	replaceParameters(it->second, it->first.c_str(), REPLACE_VALS);
    }

    CDRStart();
  }

#undef REPLACE_VALS

  DBG("SBC: connecting to '%s'\n",ruri.c_str());
  DBG("     From:  '%s'\n",from.c_str());
  DBG("     To:  '%s'\n",to.c_str());
  connectCallee(to, ruri, true);
}

bool SBCDialog::getPrepaidInterface() {
  if (call_profile.prepaid_accmodule.empty()) {
    ERROR("using prepaid but empty prepaid_accmodule!\n");
    return false;
  }
  AmDynInvokeFactory* pp_fact =
    AmPlugIn::instance()->getFactory4Di(call_profile.prepaid_accmodule);
  if (NULL == pp_fact) {
    ERROR("prepaid_accmodule '%s' not loaded\n",
	  call_profile.prepaid_accmodule.c_str());
    return false;
  }
  prepaid_acc = pp_fact->getInstance();
  if(NULL == prepaid_acc) {
    ERROR("could not get a prepaid acc reference\n");
    return false;
  }
  return true;
}

bool SBCDialog::getCDRInterface() {
  if (call_profile.cdr_module.empty()) {
    ERROR("using CDR but empty cdr_module!\n");
    return false;
  }
  AmDynInvokeFactory* cdr_fact =
    AmPlugIn::instance()->getFactory4Di(call_profile.cdr_module);
  if (NULL == cdr_fact) {
    ERROR("cdr_module '%s' not loaded\n",
	  call_profile.cdr_module.c_str());
    return false;
  }
  cdr_module = cdr_fact->getInstance();
  if(NULL == cdr_module) {
    ERROR("could not get a cdr reference\n");
    return false;
  }
  return true;
}

bool SBCDialog::getCCInterfaces() {
  for (vector<CCInterface>::iterator cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    string& cc_module = cc_it->cc_module;
    if (cc_module.empty()) {
      ERROR("using call control but empty cc_module for '%s'!\n", cc_it->cc_name.c_str());
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

void SBCDialog::process(AmEvent* ev)
{

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    if (timer_id == SBC_TIMER_ID_CALL_TIMER &&
	getCalleeStatus() == Connected) {
      DBG("SBC: %us call timer hit - ending call\n", call_timer);
      stopCall();
      ev->processed = true;
      return;
    } else if (timer_id == SBC_TIMER_ID_PREPAID_TIMEOUT) {
      DBG("timer timeout, no more credit\n");
      stopCall();
      ev->processed = true;
      return;
    } else if (timer_id >= SBC_TIMER_ID_CALL_TIMERS_START &&
	       timer_id <= SBC_TIMER_ID_CALL_TIMERS_END) {
      DBG("timer %d timeout, stopping call\n", timer_id);
      stopCall();
      ev->processed = true;
    }
  }

  SBCCallTimerEvent* ct_event;
  if (ev->event_id == SBCCallTimerEvent_ID &&
      (ct_event = dynamic_cast<SBCCallTimerEvent*>(ev)) != NULL) {
    switch (ct_event->timer_action) {
    case SBCCallTimerEvent::Remove: removeTimer(ct_event->timer_id); return;
    case SBCCallTimerEvent::Set:    setTimer(ct_event->timer_id, ct_event->timeout); return;
    case SBCCallTimerEvent::Reset:
      removeTimer(ct_event->timer_id);
      setTimer(ct_event->timer_id, ct_event->timeout);
      return;
    default: ERROR("unknown timer_action in sbc call timer event\n");
    }
  }

  AmB2BCallerSession::process(ev);
}

int SBCDialog::relayEvent(AmEvent* ev) {
  if ((call_profile.headerfilter != Transparent) &&
      (ev->event_id == B2BSipRequest)) {
    // header filter
    B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
    assert(req_ev);
    inplaceHeaderFilter(req_ev->req.hdrs,
			call_profile.headerfilter_list, call_profile.headerfilter);
  } else {
    if (ev->event_id == B2BSipReply) {
      if ((call_profile.headerfilter != Transparent) ||
	  (call_profile.reply_translations.size())) {
	B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
	assert(reply_ev);
	// header filter
	if (call_profile.headerfilter != Transparent) {
	  inplaceHeaderFilter(reply_ev->reply.hdrs,
			      call_profile.headerfilter_list,
			      call_profile.headerfilter);
	}

	// reply translations
	map<unsigned int, pair<unsigned int, string> >::iterator it =
	  call_profile.reply_translations.find(reply_ev->reply.code);
	if (it != call_profile.reply_translations.end()) {
	  DBG("translating reply %u %s => %u %s\n",
	      reply_ev->reply.code, reply_ev->reply.reason.c_str(),
	      it->second.first, it->second.second.c_str());
	  reply_ev->reply.code = it->second.first;
	  reply_ev->reply.reason = it->second.second;
	}
      }
    }
  }

  return AmB2BCallerSession::relayEvent(ev);
}

int SBCDialog::filterBody(AmSdp& sdp, bool is_a2b) {
  if (call_profile.sdpfilter_enabled) {
    // normalize SDP
    normalizeSDP(sdp);
    // filter SDP
    if (call_profile.sdpfilter != Transparent) {
      filterSDP(sdp, call_profile.sdpfilter, call_profile.sdpfilter_list);
    }
  }
  return 0;
}

void SBCDialog::onSipRequest(const AmSipRequest& req) {
  // AmB2BSession does not call AmSession::onSipRequest for 
  // forwarded requests - so lets call event handlers here
  // todo: this is a hack, replace this by calling proper session 
  // event handler in AmB2BSession
  bool fwd = sip_relay_only &&
    //(req.method != "BYE") &&
    (req.method != "CANCEL");
  if (fwd) {
      CALL_EVENT_H(onSipRequest,req);
  }

  if (fwd && call_profile.messagefilter != Transparent) {
    bool is_filtered = (call_profile.messagefilter == Whitelist) ^ 
      (call_profile.messagefilter_list.find(req.method) != 
       call_profile.messagefilter_list.end());
    if (is_filtered) {
      DBG("replying 405 to filtered message '%s'\n", req.method.c_str());
      dlg.reply(req, 405, "Method Not Allowed", "", "", "", SIP_FLAGS_VERBATIM);
      return;
    }
  }

  AmB2BCallerSession::onSipRequest(req);
}

void SBCDialog::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();

  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.content_type.c_str());
  if (fwd) {
      CALL_EVENT_H(onSipReply,reply, old_dlg_status);
  }

  if (NULL == auth) {
    AmB2BCallerSession::onSipReply(reply, old_dlg_status);
    return;
  }

  // only for SIP authenticated
  unsigned int cseq_before = dlg.cseq;
  if (!auth->onSipReply(reply, old_dlg_status)) {
      AmB2BCallerSession::onSipReply(reply, old_dlg_status);
  } else {
    if (cseq_before != dlg.cseq) {
      DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
          "updating relayed_req map\n", reply.cseq, cseq_before);
      updateUACTransCSeq(reply.cseq, cseq_before);
    }
  }
}

void SBCDialog::onSendRequest(const string& method, const string& content_type,
			      const string& body, string& hdrs, int flags, unsigned int cseq) {
  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", cseq);
    auth->onSendRequest(method, content_type, body, hdrs, flags, cseq);
  }

  AmB2BCallerSession::onSendRequest(method, content_type, body, hdrs, flags, cseq);
}

bool SBCDialog::onOtherReply(const AmSipReply& reply)
{
  bool ret = false;

  if ((m_state == BB_Dialing) && (reply.cseq == invite_req.cseq)) {
    if (reply.code < 200) {
      DBG("Callee is trying... code %d\n", reply.code);
    }
    else if(reply.code < 300) {
      if(getCalleeStatus()  == Connected) {
	onCallConnected(reply);
      }
    } else {
      DBG("Callee final error with code %d\n",reply.code);
      onCallStopped();
      ret = AmB2BCallerSession::onOtherReply(reply);
    }
  }
  return ret;
}

void SBCDialog::onCallConnected(const AmSipReply& reply) {
  m_state = BB_Connected;

  if (!startCallTimer())
    return;

  if (call_profile.prepaid_enabled || call_profile.cdr_enabled) {
    gettimeofday(&prepaid_acc_start, NULL);
  }

  startPrepaidAccounting();

  CDRConnect();
  CCConnect(reply);
}

void SBCDialog::onCallStopped() {
  if (call_profile.prepaid_enabled || call_profile.cdr_enabled) {
    gettimeofday(&prepaid_acc_end, NULL);
  }

  if (m_state == BB_Connected) {
    stopPrepaidAccounting();
    stopCallTimer();
  }

  CDREnd();
  CCEnd();
}

void SBCDialog::onOtherBye(const AmSipRequest& req)
{
  onCallStopped();

  AmB2BCallerSession::onOtherBye(req);
}


void SBCDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye()\n");

  onCallStopped();

  AmB2BCallerSession::onBye(req);
}


void SBCDialog::onCancel(const AmSipRequest& cancel)
{
  dlg.bye();
  stopCall();
}

void SBCDialog::stopCall() {
  terminateOtherLeg();
  terminateLeg();
  onCallStopped();
}

/** @return whether successful */
bool SBCDialog::startCallTimer() {
  if ((call_profile.call_timer_enabled || call_profile.prepaid_enabled) &&
      (!AmSession::timersSupported())) {
    ERROR("internal implementation error: timers not supported\n");
    stopCall();
    return false;
  }

  if (call_profile.call_timer_enabled) {
    DBG("SBC: starting call timer of %u seconds\n", call_timer);
    setTimer(SBC_TIMER_ID_CALL_TIMER, call_timer);
  }

  for (vector<pair<int, unsigned int> >::iterator it=
	 call_timers.begin(); it != call_timers.end(); it++) {
    DBG("SBC: starting call timer %i of %u seconds\n", it->first, it->second);
    setTimer(it->first, it->second);
  }

  return true;
}

void SBCDialog::stopCallTimer() {
  if (call_profile.call_timer_enabled) {
    DBG("SBC: removing call timer\n");
    removeTimer(SBC_TIMER_ID_CALL_TIMER);
  }
}

void SBCDialog::startPrepaidAccounting() {
  if (!call_profile.prepaid_enabled)
    return;

  if (NULL == prepaid_acc) {
    ERROR("Internal error, trying to use prepaid, but no prepaid_acc\n");
    terminateOtherLeg();
    terminateLeg();
    return;
  }

  DBG("SBC: starting prepaid timer of %d seconds\n", prepaid_credit);
  {
    setTimer(SBC_TIMER_ID_PREPAID_TIMEOUT, prepaid_credit);
  }

  {
    AmArg di_args,ret;
    di_args.push(call_profile.prepaid_uuid);     // prepaid_uuid
    di_args.push(call_profile.prepaid_acc_dest); // accounting destination
    di_args.push((int)prepaid_starttime.tv_sec); // call start time (INVITE)
    di_args.push((int)prepaid_acc_start.tv_sec); // call connect time
    di_args.push(getCallID());                   // Call-ID
    di_args.push(getLocalTag());                 // ltag
    di_args.push(other_id);                      // other leg ltag

    prepaid_acc->invoke("connectCall", di_args, ret);
  }
}

void SBCDialog::stopPrepaidAccounting() {
  if (!call_profile.prepaid_enabled)
    return;

  if(prepaid_acc_start.tv_sec != 0 || prepaid_acc_start.tv_usec != 0) {

    if (NULL == prepaid_acc) {
      ERROR("Internal error, trying to subtractCredit, but no prepaid_acc\n");
      return;
    }

    struct timeval diff;
    timersub(&prepaid_acc_end, &prepaid_acc_start, &diff);
    if(diff.tv_usec > 500000)
      diff.tv_sec++;
    DBG("Call lasted %ld seconds\n", diff.tv_sec);

    AmArg di_args,ret;
    di_args.push(call_profile.prepaid_uuid);     // prepaid_uuid
    di_args.push((int)diff.tv_sec);              // call duration
    di_args.push(call_profile.prepaid_acc_dest); // accounting destination
    di_args.push((int)prepaid_starttime.tv_sec); // call start time (INVITE)
    di_args.push((int)prepaid_acc_start.tv_sec); // call connect time
    di_args.push((int)prepaid_acc_end.tv_sec);   // call end time
    di_args.push(getCallID());                   // Call-ID
    di_args.push(getLocalTag());                 // ltag
    di_args.push(other_id);                      // 2nd leg ltag

    prepaid_acc->invoke("subtractCredit", di_args, ret);
  }
}

bool SBCDialog::CCStart(const AmSipRequest& req) {
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (vector<CCInterface>::iterator cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(getLocalTag());
    di_args.push((ArgObject*)&call_profile);
    di_args.push((int)prepaid_starttime.tv_sec);
    di_args.push((int)prepaid_starttime.tv_usec);
    di_args.push(AmArg());
    AmArg& vals = di_args.get(4);
    vals.assertStruct();
    for (map<string, string>::iterator it = cc_if.cc_values.begin();
	 it != cc_if.cc_values.end(); it++) {
      vals[it->first] = it->second;
    }
    di_args.push(cc_timer_id); // current timer ID

    (*cc_mod)->invoke("start", di_args, ret);

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
	  dlg.setStatus(AmSipDialog::Disconnected);
	  return false;
	}

	case SBC_CC_REFUSE_ACTION: {
	  if (ret[i].size() < 3 ||
	      !isArgInt(ret[i][SBC_CC_REFUSE_CODE]) ||
	      !isArgCStr(ret[i][SBC_CC_REFUSE_REASON])) {
	    ERROR("in call control module '%s' - REFUSE action parameters missing/wrong: '%s'\n",
		  cc_if.cc_name.c_str(), AmArg::print(ret[i]).c_str());
	    continue;
	  }
	  string headers;
	  if (ret[i].size() > SBC_CC_REFUSE_HEADERS) {
	    for (size_t h=0;h<ret[i][SBC_CC_REFUSE_HEADERS].size();h++)
	      headers += string(ret[i][SBC_CC_REFUSE_HEADERS][h].asCStr()) + CRLF;
	  }

	  DBG("replying with %d %s on call control action REFUSE from '%s'\n",
	      ret[i][SBC_CC_REFUSE_CODE].asInt(), ret[i][SBC_CC_REFUSE_REASON].asCStr(),
	      cc_if.cc_name.c_str());

	  dlg.reply(req,
		    ret[i][SBC_CC_REFUSE_CODE].asInt(), ret[i][SBC_CC_REFUSE_REASON].asCStr(),
		    headers);
	  return false;
	}

	case SBC_CC_SET_CALL_TIMER_ACTION: {
	  if (cc_timer_id > SBC_TIMER_ID_CALL_TIMERS_END) {
	    ERROR("too many call timers - ignoring timer\n");
	    continue;
	  }

	  if (ret[i].size() < 2 ||
	      !isArgInt(ret[i][SBC_CC_TIMER_TIMEOUT])) {
	    ERROR("in call control module '%s' - SET_CALL_TIMER action parameters missing: '%s'\n",
		  cc_if.cc_name.c_str(), AmArg::print(ret[i]).c_str());
	    continue;
	  }

	  DBG("saving call timer %i: timeout %i\n",
	      cc_timer_id, ret[i][SBC_CC_TIMER_TIMEOUT].asInt());
	  call_timers.push_back(std::make_pair(cc_timer_id, ret[i][SBC_CC_TIMER_TIMEOUT].asInt()));
	  cc_timer_id++;
	} break;

	default: {
	  ERROR("unknown call control action: '%s'\n", AmArg::print(ret[i]).c_str());
	  continue;
	}

	}

      }
    }

    cc_mod++;
  }
  return true;
}

void SBCDialog::CCConnect(const AmSipReply& reply) {
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (vector<CCInterface>::iterator cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    //    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(getLocalTag());                 // call ltag
    di_args.push(other_id);                      // other leg ltag
    di_args.push((int)prepaid_acc_start.tv_sec);
    di_args.push((int)prepaid_acc_start.tv_usec);
    (*cc_mod)->invoke("connect", di_args, ret);

    cc_mod++;
  }
}

void SBCDialog::CCEnd() {
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (vector<CCInterface>::iterator cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    //    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(getLocalTag());                 // call ltag
    di_args.push((int)prepaid_acc_end.tv_sec);
    di_args.push((int)prepaid_acc_end.tv_usec);
    (*cc_mod)->invoke("end", di_args, ret);

    cc_mod++;
  }
}

#define CHECK_CDR_MODULE						\
  if (NULL == cdr_module) {						\
    ERROR("Internal error, trying to use CDR, but no cdr_module\n");	\
    terminateOtherLeg();						\
    terminateLeg();							\
    return;								\
  }

void SBCDialog::CDRStart() {

  if (!call_profile.cdr_enabled) return;
  CHECK_CDR_MODULE;

  AmArg di_args,ret;
  di_args.push(getLocalTag());
  di_args.push(getCallID());
  di_args.push(dlg.remote_tag);
  di_args.push((int)prepaid_starttime.tv_sec);
  di_args.push(AmArg());
  AmArg& vals = di_args.get(4);
  vals.assertStruct();
  for (map<string, string>::iterator it = call_profile.cdr_values.begin();
       it != call_profile.cdr_values.end(); it++) {
    vals[it->first] = it->second;
  }

  cdr_module->invoke("start", di_args, ret);
}

void SBCDialog::CDRConnect() {
  if (!call_profile.cdr_enabled) return;
  CHECK_CDR_MODULE;

  AmArg di_args,ret;
  di_args.push(getLocalTag());                 // ltag
  di_args.push(getCallID());                   // Call-ID
  di_args.push(other_id);                      // other leg ltag
  di_args.push((int)prepaid_acc_start.tv_sec); // call connect time
  cdr_module->invoke("connect", di_args, ret);
}

void SBCDialog::CDREnd() {
  if (!call_profile.cdr_enabled) return;
  CHECK_CDR_MODULE;

  AmArg di_args,ret;
  di_args.push(getLocalTag());                 // ltag
  di_args.push(getCallID());                   // Call-ID
  di_args.push((int)prepaid_acc_end.tv_sec);   // call end time
  cdr_module->invoke("end", di_args, ret);
}

void SBCDialog::createCalleeSession()
{
  SBCCalleeSession* callee_session = new SBCCalleeSession(this, call_profile);
  
  if (call_profile.auth_enabled) {
    // adding auth handler
    AmSessionEventHandlerFactory* uac_auth_f = 
      AmPlugIn::instance()->getFactory4Seh("uac_auth");
    if (NULL == uac_auth_f)  {
      INFO("uac_auth module not loaded. uac auth NOT enabled.\n");
    } else {
      AmSessionEventHandler* h = uac_auth_f->getHandler(callee_session);
      
      // we cannot use the generic AmSessionEventHandler hooks, 
      // because the hooks don't work in AmB2BSession
      callee_session->setAuthHandler(h);
      DBG("uac auth enabled for callee session.\n");
    }
  }

  if (call_profile.sst_enabled == "yes") {
    AmSessionEventHandler* h = SBCFactory::session_timer_fact->getHandler(callee_session);
    if(!h) {
      ERROR("could not get a session timer event handler\n");
      delete callee_session;
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    if(h->configure(call_profile.sst_b_cfg)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      callee_session->addHandler(h);
    }
  }

  AmSipDialog& callee_dlg = callee_session->dlg;

  if (!call_profile.outbound_proxy.empty()) {
    callee_dlg.outbound_proxy = call_profile.outbound_proxy;
    callee_dlg.force_outbound_proxy = call_profile.force_outbound_proxy;
  }
  
  if (!call_profile.next_hop_ip.empty()) {
    callee_dlg.next_hop_ip = call_profile.next_hop_ip;
    callee_dlg.next_hop_port = call_profile.next_hop_port.empty() ?
      5060 : call_profile.next_hop_port_i;

    if (!call_profile.next_hop_for_replies.empty()) {
      callee_dlg.next_hop_for_replies =
	(call_profile.next_hop_for_replies == "yes" ||
	 call_profile.next_hop_for_replies == "1");
    }
  }

  if(outbound_interface >= 0)
    callee_dlg.outbound_interface = outbound_interface;

  other_id = AmSession::getNewId();
  
  callee_dlg.local_tag    = other_id;
  callee_dlg.callid       = callid.empty() ?
    AmSession::getNewId() : callid;
  
  // this will be overwritten by ConnectLeg event 
  callee_dlg.remote_party = to;
  callee_dlg.remote_uri   = ruri;

  callee_dlg.local_party  = from; 
  callee_dlg.local_uri    = from; 
  
  DBG("Created B2BUA callee leg, From: %s\n",
      from.c_str());

  if (AmConfig::LogSessions) {
    INFO("Starting B2B callee session %s\n",
	 callee_session->getLocalTag().c_str()/*, invite_req.cmd.c_str()*/);
  }

  MONITORING_LOG4(other_id.c_str(),
		  "dir",  "out",
		  "from", callee_dlg.local_party.c_str(),
		  "to",   callee_dlg.remote_party.c_str(),
		  "ruri", callee_dlg.remote_uri.c_str());

  try {
    initializeRTPRelay(callee_session);
  }
  catch (...) {
    delete callee_session;
    throw;
  }

  callee_session->start();
  
  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}

SBCCalleeSession::SBCCalleeSession(const AmB2BCallerSession* caller,
				   const SBCCallProfile& call_profile) 
  : auth(NULL),
    call_profile(call_profile),
    AmB2BCalleeSession(caller)
{
  dlg.reliable_1xx = REL100_IGNORED;

  if (call_profile.sdpfilter_enabled) {
    b2b_mode = B2BMode_SDPFilter;
  }

  if (!call_profile.contact.empty()) {
    dlg.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) + call_profile.contact + CRLF;
  }
}

SBCCalleeSession::~SBCCalleeSession() {
  if (auth) 
    delete auth;
}

inline UACAuthCred* SBCCalleeSession::getCredentials() {
  return &call_profile.auth_credentials;
}

int SBCCalleeSession::relayEvent(AmEvent* ev) {
  if ((call_profile.headerfilter != Transparent) &&
      (ev->event_id == B2BSipRequest)) {
    // header filter
    B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
    assert(req_ev);
    inplaceHeaderFilter(req_ev->req.hdrs,
			call_profile.headerfilter_list, call_profile.headerfilter);
  } else {
    if (ev->event_id == B2BSipReply) {
      if ((call_profile.headerfilter != Transparent) ||
	  (call_profile.reply_translations.size())) {
	B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
	assert(reply_ev);

	// header filter
	if (call_profile.headerfilter != Transparent) {
	  inplaceHeaderFilter(reply_ev->reply.hdrs,
			      call_profile.headerfilter_list,
			      call_profile.headerfilter);
	}
	// reply translations
	map<unsigned int, pair<unsigned int, string> >::iterator it =
	  call_profile.reply_translations.find(reply_ev->reply.code);
	if (it != call_profile.reply_translations.end()) {
	  DBG("translating reply %u %s => %u %s\n",
	      reply_ev->reply.code, reply_ev->reply.reason.c_str(),
	      it->second.first, it->second.second.c_str());
	  reply_ev->reply.code = it->second.first;
	  reply_ev->reply.reason = it->second.second;
	}
      }
    }
  }

  return AmB2BCalleeSession::relayEvent(ev);
}

void SBCCalleeSession::onSipRequest(const AmSipRequest& req) {
  // AmB2BSession does not call AmSession::onSipRequest for 
  // forwarded requests - so lets call event handlers here
  // todo: this is a hack, replace this by calling proper session 
  // event handler in AmB2BSession
  bool fwd = sip_relay_only &&
    //(req.method != "BYE") &&
    (req.method != "CANCEL");
  if (fwd) {
      CALL_EVENT_H(onSipRequest,req);
  }

  if (fwd && call_profile.messagefilter != Transparent) {
    bool is_filtered = (call_profile.messagefilter == Whitelist) ^ 
      (call_profile.messagefilter_list.find(req.method) != 
       call_profile.messagefilter_list.end());
    if (is_filtered) {
      DBG("replying 405 to filtered message '%s'\n", req.method.c_str());
      dlg.reply(req, 405, "Method Not Allowed", "", "", "", SIP_FLAGS_VERBATIM);
      return;
    }
  }

  AmB2BCalleeSession::onSipRequest(req);
}

void SBCCalleeSession::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status)
{
  // call event handlers where it is not done 
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();
  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  DBG("onSipReply: content-type = %s\n",reply.content_type.c_str());
  if(fwd) {
    CALL_EVENT_H(onSipReply,reply, old_dlg_status);
  }


  if (NULL == auth) {
    AmB2BCalleeSession::onSipReply(reply, old_dlg_status);
    return;
  }
  
  unsigned int cseq_before = dlg.cseq;
  if (!auth->onSipReply(reply, old_dlg_status)) {
      AmB2BCalleeSession::onSipReply(reply, old_dlg_status);
  } else {
    if (cseq_before != dlg.cseq) {
      DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
          "updating relayed_req map\n", reply.cseq, cseq_before);
      updateUACTransCSeq(reply.cseq, cseq_before);
    }
  }
}

void SBCCalleeSession::onSendRequest(const string& method, const string& content_type,
				     const string& body, string& hdrs, int flags, unsigned int cseq)
{
  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", cseq);
    auth->onSendRequest(method, content_type, body, hdrs, flags, cseq);
  }
  
  AmB2BCalleeSession::onSendRequest(method, content_type, body, hdrs, flags, cseq);
}

int SBCCalleeSession::filterBody(AmSdp& sdp, bool is_a2b) {
  if (call_profile.sdpfilter_enabled) {
    // normalize SDP
    normalizeSDP(sdp);
    // filter SDP
    if (call_profile.sdpfilter != Transparent) {
      filterSDP(sdp, call_profile.sdpfilter, call_profile.sdpfilter_list);
    }
  }
  return 0;
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
