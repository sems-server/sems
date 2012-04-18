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
#include <algorithm>

using std::map;

#define MOD_NAME "sbc"

AmConfigReader SBCFactory::cfg;
AmSessionEventHandlerFactory* SBCFactory::session_timer_fact = NULL;
RegexMapper SBCFactory::regex_mappings;

EXPORT_MODULE_FACTORY(SBCFactory);
DEFINE_MODULE_INSTANCE(SBCFactory, MOD_NAME);

// helper functions

static bool containsPayload(const std::vector<SdpPayload>& payloads, const SdpPayload &payload)
{
  string pname = payload.encoding_name;
  transform(pname.begin(), pname.end(), pname.begin(), ::tolower);

  for (vector<SdpPayload>::const_iterator p = payloads.begin(); p != payloads.end(); ++p) {
    string s = p->encoding_name;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s != pname) continue;
    if (p->clock_rate != payload.clock_rate) continue;
    if ((p->encoding_param >= 0) && (payload.encoding_param >= 0) && 
        (p->encoding_param != payload.encoding_param)) continue;
    return true;
  }
  return false;
}

static void appendTranscoderCodecs(AmSdp &sdp, MediaType mtype, std::vector<SdpPayload> &transcoder_codecs)
{
  // append codecs for transcoding, remember the added ones to be able to filter
  // them out from relayed reply!

  // important: normalized SDP should get here

  // FIXME: only first matching media stream?

  vector<SdpPayload>::const_iterator p;
  for (vector<SdpMedia>::iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) {

    // handle audio transcoder codecs
    if (m->type == mtype) {
      // transcoder codecs can be added only if there are common payloads with
      // the remote (only those allowed for transcoder)
      // if there are no such common payloads adding transcoder codecs can't help
      // because we won't be able to transcode later on!
      // (we have to check for each media stream independently)

      // find first unused dynamic payload number & detect transcodable codecs
      // in original SDP
      int id = 96;
      bool transcodable = false;
      for (p = m->payloads.begin(); p != m->payloads.end(); ++p) {
        if (p->payload_type >= id) id = p->payload_type + 1;
        if (containsPayload(transcoder_codecs, *p)) transcodable = true;
      }

      if (transcodable) {
        // there are some transcodable codecs present in the SDP, we can safely
        // add the other transcoder codecs to the SDP
        for (p = transcoder_codecs.begin(); p != transcoder_codecs.end(); ++p) {
          // add all payloads which are not already there
          if (!containsPayload(m->payloads, *p)) {
            m->payloads.push_back(*p);
            if (p->payload_type < 0) m->payloads.back().payload_type = id++;
          }
        }
        if (id > 128) ERROR("assigned too high payload type number (%d), see RFC 3551\n", id);
      }
    }
  }
}

// do the filtering, returns true if SDP was changed
static bool doFiltering(AmSdp &sdp, SBCCallProfile &call_profile, bool a_leg)
{
  bool changed = false;

  bool prefer_existing_codecs = call_profile.codec_prefs.preferExistingCodecs(a_leg);

  if (prefer_existing_codecs) {
    // We have to order payloads before adding transcoder codecs to leave
    // transcoding as the last chance (existing codecs are preferred thus
    // relaying will be used if possible).
    if (call_profile.codec_prefs.shouldOrderPayloads(a_leg)) {
      normalizeSDP(sdp, call_profile.anonymize_sdp);
      call_profile.codec_prefs.orderSDP(sdp, a_leg);
      changed = true;
    }
  }

  // Add transcoder codecs before filtering because otherwise SDP filter could
  // inactivate some media lines which shouldn't be inactivated.

  if (call_profile.transcoder.isActive()) {
    if (!changed) // otherwise already normalized
      normalizeSDP(sdp, call_profile.anonymize_sdp);
    appendTranscoderCodecs(sdp, MT_AUDIO, call_profile.transcoder.audio_codecs);
    changed = true;
  }
  
  if (!prefer_existing_codecs) {
    // existing codecs are not preferred - reorder AFTER adding transcoder
    // codecs so it might happen that transcoding will be forced though relaying
    // would be possible
    if (call_profile.codec_prefs.shouldOrderPayloads(a_leg)) {
      if (!changed) normalizeSDP(sdp, call_profile.anonymize_sdp);
      call_profile.codec_prefs.orderSDP(sdp, a_leg);
      changed = true;
    }
  }
  
  // It doesn't make sense to filter out codecs allowed for transcoding and thus
  // if the filter filters them out it can be considered as configuration
  // problem, right? 
  // => So we wouldn't try to avoid filtering out transcoder codecs what would
  // just complicate things.

  if (call_profile.sdpfilter_enabled) {
    if (!changed) // otherwise already normalized
      normalizeSDP(sdp, call_profile.anonymize_sdp);
    if (isActiveFilter(call_profile.sdpfilter)) {
      filterSDP(sdp, call_profile.sdpfilter, call_profile.sdpfilter_list);
    }
    changed = true;
  }
  if (call_profile.sdpalinesfilter_enabled &&
      isActiveFilter(call_profile.sdpalinesfilter)) {
    // filter SDP "a=lines"
    filterSDPalines(sdp, call_profile.sdpalinesfilter, call_profile.sdpalinesfilter_list);
    changed = true;
  }

  return changed;
}

static int filterBody(AmMimeBody *body, AmSdp& sdp, SBCCallProfile &call_profile, bool a_leg) 
{
  int res = sdp.parse((const char *)body->getPayload());
  if (0 != res) {
    DBG("SDP parsing failed!\n");
    return res;
  }

  if (doFiltering(sdp, call_profile, a_leg)) {
    string n_body;
    sdp.print(n_body);
    body->setPayload((const unsigned char*)n_body.c_str(),
			 n_body.length());
  }
  return 0;
}

static void filterBody(AmSipRequest &req, AmSdp &sdp, SBCCallProfile &call_profile, bool a_leg)
{
  AmMimeBody* body = req.body.hasContentType(SIP_APPLICATION_SDP);

  DBG("filtering SDP for request '%s'\n",
      req.method.c_str());
  if (body && 
      (req.method == SIP_METH_INVITE || 
       req.method == SIP_METH_UPDATE ||
       req.method == SIP_METH_ACK)) {

    // todo: handle filtering errors
    filterBody(body, sdp, call_profile, a_leg);
  }
}

static void filterBody(AmSipReply &reply, AmSdp &sdp, SBCCallProfile &call_profile, bool a_leg)
{
  AmMimeBody* body = reply.body.hasContentType(SIP_APPLICATION_SDP);

  DBG("filtering body of relayed reply %d\n", reply.code);
  if (body &&
      (reply.cseq_method == SIP_METH_INVITE || reply.cseq_method == SIP_METH_UPDATE)) {
    filterBody(body, sdp, call_profile, a_leg);
  }
}


///////////////////////////////////////////////////////////////////////////////////////////

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
      if (NULL == session_timer_fact) {
	ERROR("session_timer module not loaded - unable to create call with SST\n");
	throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      }

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
    if (NULL == session_timer_fact) {
      ERROR("session_timer module not loaded - unable to create call with SST\n");
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

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

SBCDialog::SBCDialog(const SBCCallProfile& call_profile)
  : m_state(BB_Init),
    auth(NULL),
    call_profile(call_profile),
    outbound_interface(-1),
    rtprelay_interface(-1),
    cc_timer_id(SBC_TIMER_ID_CALL_TIMERS_START)
{
  set_sip_relay_only(false);
  dlg.setRel100State(Am100rel::REL100_IGNORED);

  memset(&call_connect_ts, 0, sizeof(struct timeval));
  memset(&call_end_ts, 0, sizeof(struct timeval));
}


SBCDialog::~SBCDialog()
{
  if (auth)
    delete auth;
}

UACAuthCred* SBCDialog::getCredentials() {
  return &call_profile.auth_aleg_credentials;
}

void SBCDialog::fixupCCInterface(const string& val, CCInterface& cc_if) {
  DBG("instantiating CC interface from '%s'\n", val.c_str());
  size_t spos, last = val.length() - 1;
  if (last < 0) {
      spos = string::npos;
      cc_if.cc_module = "";
  } else {
      spos = val.find(";", 0);
      cc_if.cc_module = val.substr(0, spos);
  }
  DBG("    module='%s'\n", cc_if.cc_module.c_str());
  while (spos < last) {
      size_t epos = val.find("=", spos + 1);
      if (epos == string::npos) {
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1), ""));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1).c_str(), "");
	  return;
      }
      if (epos == last) {
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), ""));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), "");
	  return;
      }
      // if value starts with " char, it continues until another " is found
      if (val[epos + 1] == '"') {
	  if (epos + 1 == last) {
	      cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), ""));
	      DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), "");
	      return;
	  }
	  size_t qpos = val.find('"', epos + 2);
	  if (qpos == string::npos) {
	      cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos -1), val.substr(epos + 2)));
	      DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 2).c_str());
	      return;
	  }
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), val.substr(epos + 2, qpos - epos - 2)));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 2, qpos - epos - 2).c_str());
	  if (qpos < last) {
	      spos = val.find(";", qpos + 1);
	  } else {
	      return;
	  }
      } else {
	  size_t new_spos = val.find(";", epos + 1);
	  if (new_spos == string::npos) {
	      cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), val.substr(epos + 1)));
	      DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 1).c_str());
	      return;
	  }
	  cc_if.cc_values.insert(make_pair(val.substr(spos + 1, epos - spos - 1), val.substr(epos + 1, new_spos - epos - 1)));
	  DBG("    '%s'='%s'\n", val.substr(spos + 1, epos - spos - 1).c_str(), val.substr(epos + 1, new_spos - epos - 1).c_str());
	  spos = new_spos;
      }
  }
  return;
}

void SBCDialog::onInvite(const AmSipRequest& req)
{
  AmUriParser ruri_parser, from_parser, to_parser;

  DBG("processing initial INVITE %s\n", req.r_uri.c_str());

  string app_param = getHeader(req.hdrs, PARAM_HDR, true);

  // get start time for call control
  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_start_ts, NULL);
  }

  // process call control
  if (call_profile.cc_interfaces.size()) {
    unsigned int cc_dynif_count = 0;

    // fix up replacements in cc list
    CCInterfaceListIteratorT cc_rit = call_profile.cc_interfaces.begin();
    while (cc_rit != call_profile.cc_interfaces.end()) {
      CCInterfaceListIteratorT curr_if = cc_rit;
      cc_rit++;
      //      CCInterfaceListIteratorT next_cc = cc_rit+1;
      if (curr_if->cc_name.find('$') != string::npos) {
	vector<string> dyn_ccinterfaces =
	  explode(replaceParameters(curr_if->cc_name, "cc_interfaces", REPLACE_VALS), ",");
	if (!dyn_ccinterfaces.size()) {
	  DBG("call_control '%s' did not produce any call control instances\n",
	      curr_if->cc_name.c_str());
	  call_profile.cc_interfaces.erase(curr_if);
	} else {
	  // fill first CC interface (replacement item)
	  vector<string>::iterator it=dyn_ccinterfaces.begin();
	  curr_if->cc_name = "cc_dyn_"+int2str(cc_dynif_count++);
	  fixupCCInterface(trim(*it, " \t"), *curr_if);
	  it++;

	  // insert other CC interfaces (in order!)
	  while (it != dyn_ccinterfaces.end()) {
	    CCInterfaceListIteratorT new_cc =
	      call_profile.cc_interfaces.insert(cc_rit, CCInterface());
	    fixupCCInterface(trim(*it, " \t"), *new_cc);
	    new_cc->cc_name = "cc_dyn_"+int2str(cc_dynif_count++);
	    it++;
	  }
	}
      }
    }

    // fix up module names
    for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {
      cc_it->cc_module =
	replaceParameters(cc_it->cc_module, "cc_module", REPLACE_VALS);
    }

    if (!getCCInterfaces()) {
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

    // fix up variables
    for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
	 cc_it != call_profile.cc_interfaces.end(); cc_it++) {
      CCInterface& cc_if = *cc_it;

      DBG("processing replacements for call control interface '%s'\n",
	  cc_if.cc_name.c_str());

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

  if (!call_profile.evaluate(REPLACE_VALS)) {
    ERROR("call profile evaluation failed\n");
    throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
  }

  ruri = call_profile.ruri.empty() ? req.r_uri : call_profile.ruri;
  if(!call_profile.ruri_host.empty()){
    ruri_parser.uri = ruri;
    if(!ruri_parser.parse_uri()) {
      WARN("Error parsing R-URI '%s'\n", ruri.c_str());
    }
    else {
      ruri_parser.uri_port.clear();
      ruri_parser.uri_host = call_profile.ruri_host;
      ruri = ruri_parser.uri_str();
    }
  }
  from = call_profile.from.empty() ? req.from : call_profile.from;
  to = call_profile.to.empty() ? req.to : call_profile.to;
  callid = call_profile.callid;

  if (call_profile.rtprelay_enabled || call_profile.transcoder.isActive()) {
    DBG("Enabling RTP relay mode for SBC call\n");

    if (call_profile.force_symmetric_rtp_value) {
      DBG("forcing symmetric RTP (passive mode)\n");
      rtp_relay_force_symmetric_rtp = true;
    }

    if (call_profile.aleg_rtprelay_interface_value >= 0) {
      setRtpRelayInterface(call_profile.aleg_rtprelay_interface_value);
      DBG("using RTP interface %i for A leg\n", rtp_interface);
    }

    if (call_profile.rtprelay_interface_value >= 0) {
      rtprelay_interface = call_profile.rtprelay_interface_value;
      DBG("configured RTP relay interface %i for B leg\n", rtprelay_interface);
    }

    setRtpRelayTransparentSeqno(call_profile.rtprelay_transparent_seqno);
    setRtpRelayTransparentSSRC(call_profile.rtprelay_transparent_ssrc);

    setRtpRelayMode(RTP_Relay);
  }

  m_state = BB_Dialing;

  invite_req = req;
  est_invite_cseq = req.cseq;

  removeHeader(invite_req.hdrs,PARAM_HDR);
  removeHeader(invite_req.hdrs,"P-App-Name");

  if (call_profile.sst_enabled_value) {
    removeHeader(invite_req.hdrs,SIP_HDR_SESSION_EXPIRES);
    removeHeader(invite_req.hdrs,SIP_HDR_MIN_SE);
  }

  inplaceHeaderFilter(invite_req.hdrs,
		      call_profile.headerfilter_list, call_profile.headerfilter);

  if (call_profile.append_headers.size() > 2) {
    string append_headers = call_profile.append_headers;
    assertEndCRLF(append_headers);
    invite_req.hdrs+=append_headers;
  }

  if (call_profile.outbound_interface_value >= 0)
    outbound_interface = call_profile.outbound_interface_value;

  dlg.setOAEnabled(false); // ???

#undef REPLACE_VALS

  DBG("SBC: connecting to '%s'\n",ruri.c_str());
  DBG("     From:  '%s'\n",from.c_str());
  DBG("     To:  '%s'\n",to.c_str());
  connectCallee(to, ruri, true);
}

bool SBCDialog::getCCInterfaces() {
  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
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
    if (timer_id >= SBC_TIMER_ID_CALL_TIMERS_START &&
	timer_id <= SBC_TIMER_ID_CALL_TIMERS_END) {
      DBG("timer %d timeout, stopping call\n", timer_id);
      stopCall();
      ev->processed = true;
    }
  }

  SBCCallTimerEvent* ct_event;
  if (ev->event_id == SBCCallTimerEvent_ID &&
      (ct_event = dynamic_cast<SBCCallTimerEvent*>(ev)) != NULL) {
    switch (m_state) {
    case BB_Connected: {
      switch (ct_event->timer_action) {
      case SBCCallTimerEvent::Remove:
	DBG("removing timer %d on call timer request\n", ct_event->timer_id);
	removeTimer(ct_event->timer_id); return;
      case SBCCallTimerEvent::Set:
	DBG("setting timer %d to %f on call timer request\n",
	    ct_event->timer_id, ct_event->timeout);
	setTimer(ct_event->timer_id, ct_event->timeout); return;
      case SBCCallTimerEvent::Reset:
	DBG("resetting timer %d to %f on call timer request\n",
	    ct_event->timer_id, ct_event->timeout);
	removeTimer(ct_event->timer_id);
	setTimer(ct_event->timer_id, ct_event->timeout);
	return;
      default: ERROR("unknown timer_action in sbc call timer event\n"); return;
      }
    }

    case BB_Init:
    case BB_Dialing: {
      switch (ct_event->timer_action) {
      case SBCCallTimerEvent::Remove: clearCallTimer(ct_event->timer_id); return;
      case SBCCallTimerEvent::Set:
      case SBCCallTimerEvent::Reset:
	saveCallTimer(ct_event->timer_id, ct_event->timeout); return;
      default: ERROR("unknown timer_action in sbc call timer event\n"); return;
      }
    } break;

    default: break;
    }
  }

  SBCControlEvent* ctl_event;
  if (ev->event_id == SBCControlEvent_ID &&
      (ctl_event = dynamic_cast<SBCControlEvent*>(ev)) != NULL) {
    onControlCmd(ctl_event->cmd, ctl_event->params);
    return;
  }

  AmB2BCallerSession::process(ev);
}

void SBCDialog::onControlCmd(string& cmd, AmArg& params) {
  if (cmd == "teardown") {
    DBG("teardown requested from control cmd\n");
    stopCall();
    return;
  }
  DBG("ignoring unknown control cmd : '%s'\n", cmd.c_str());
}

int SBCDialog::relayEvent(AmEvent* ev) {
  if (isActiveFilter(call_profile.headerfilter) && (ev->event_id == B2BSipRequest)) {
    // header filter
    B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
    assert(req_ev);
    inplaceHeaderFilter(req_ev->req.hdrs,
			call_profile.headerfilter_list, call_profile.headerfilter);
  } else {
    if (ev->event_id == B2BSipReply) {
      if (isActiveFilter(call_profile.headerfilter) ||
	  call_profile.reply_translations.size()) {
	B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
	assert(reply_ev);
	// header filter
	if (isActiveFilter(call_profile.headerfilter)) {
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

void SBCDialog::filterBody(AmSipRequest &req, AmSdp &sdp)
{
  ::filterBody(req, sdp, call_profile, a_leg);
}

void SBCDialog::filterBody(AmSipReply &reply, AmSdp &sdp)
{
  ::filterBody(reply, sdp, call_profile, a_leg);
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

  if (fwd && isActiveFilter(call_profile.messagefilter)) {
    bool is_filtered = (call_profile.messagefilter == Whitelist) ^ 
      (call_profile.messagefilter_list.find(req.method) != 
       call_profile.messagefilter_list.end());
    if (is_filtered) {
      DBG("replying 405 to filtered message '%s'\n", req.method.c_str());
      dlg.reply(req, 405, "Method Not Allowed", NULL, "", SIP_FLAGS_VERBATIM);
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
  DBG("onSipReply: content-type = %s\n",reply.body.getCTStr().c_str());
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

void SBCDialog::onSendRequest(AmSipRequest& req, int flags) {
  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", req.cseq);
    auth->onSendRequest(req, flags);
  }

  AmB2BCallerSession::onSendRequest(req, flags);
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

  if (!startCallTimers())
    return;

  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_connect_ts, NULL);
  }

  CCConnect(reply);
}

void SBCDialog::onCallStopped() {
  if (call_profile.cc_interfaces.size()) {
    gettimeofday(&call_end_ts, NULL);
  }

  if (m_state == BB_Connected) {
    stopCallTimers();
  }

  m_state = BB_Teardown;

  CCEnd();
}

void SBCDialog::onOtherBye(const AmSipRequest& req)
{
  onCallStopped();

  AmB2BCallerSession::onOtherBye(req);
}

void SBCDialog::onSessionTimeout() {
  onCallStopped();

  AmB2BCallerSession::onSessionTimeout();
}

void SBCDialog::onNoAck(unsigned int cseq) {
  onCallStopped();

  AmB2BCallerSession::onNoAck(cseq);
}

void SBCDialog::onRemoteDisappeared(const AmSipReply& reply)  {
  DBG("Remote unreachable - ending SBC call\n");
  onCallStopped();

  AmB2BCallerSession::onRemoteDisappeared(reply);
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

void SBCDialog::onSystemEvent(AmSystemEvent* ev) {
  if (ev->sys_event == AmSystemEvent::ServerShutdown) {
    onCallStopped();
  }

  AmB2BCallerSession::onSystemEvent(ev);
}

void SBCDialog::stopCall() {
  terminateOtherLeg();
  terminateLeg();
  onCallStopped();
}

void SBCDialog::saveCallTimer(int timer, double timeout) {
  call_timers[timer] = timeout;
}

void SBCDialog::clearCallTimer(int timer) {
  call_timers.erase(timer);
}

void SBCDialog::clearCallTimers() {
  call_timers.clear();
}

/** @return whether successful */
bool SBCDialog::startCallTimers() {
  for (map<int, double>::iterator it=
	 call_timers.begin(); it != call_timers.end(); it++) {
    DBG("SBC: starting call timer %i of %f seconds\n", it->first, it->second);
    setTimer(it->first, it->second);
  }

  return true;
}

void SBCDialog::stopCallTimers() {
  for (map<int, double>::iterator it=
	 call_timers.begin(); it != call_timers.end(); it++) {
    DBG("SBC: removing call timer %i\n", it->first);
    removeTimer(it->first);
  }
}

bool SBCDialog::CCStart(const AmSipRequest& req) {
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);
    di_args.push(getLocalTag());
    di_args.push((AmObject*)&call_profile);
    di_args.push(AmArg());
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    for (int i=0;i<4;i++)
      di_args.back().push((int)0);

    di_args.push(AmArg());
    AmArg& vals = di_args.back();
    vals.assertStruct();
    for (map<string, string>::iterator it = cc_if.cc_values.begin();
	 it != cc_if.cc_values.end(); it++) {
      vals[it->first] = it->second;
    }

    di_args.push(cc_timer_id); // current timer ID

    try {
      (*cc_mod)->invoke("start", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface start "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      dlg.reply(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);

      // call 'end' of call control modules up to here
      call_end_ts.tv_sec = call_start_ts.tv_sec;
      call_end_ts.tv_usec = call_start_ts.tv_usec;
      CCEnd(cc_it);

      return false;
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface start "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      dlg.reply(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);

      // call 'end' of call control modules up to here
      call_end_ts.tv_sec = call_start_ts.tv_sec;
      call_end_ts.tv_usec = call_start_ts.tv_usec;
      CCEnd(cc_it);

      return false;
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
	  dlg.setStatus(AmSipDialog::Disconnected);

	  // call 'end' of call control modules up to here
	  call_end_ts.tv_sec = call_start_ts.tv_sec;
	  call_end_ts.tv_usec = call_start_ts.tv_usec;
	  CCEnd(cc_it);

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

	  DBG("replying with %d %s on call control action REFUSE from '%s' headers='%s'\n",
	      ret[i][SBC_CC_REFUSE_CODE].asInt(), ret[i][SBC_CC_REFUSE_REASON].asCStr(),
	      cc_if.cc_name.c_str(), headers.c_str());

	  dlg.reply(req,
		    ret[i][SBC_CC_REFUSE_CODE].asInt(), ret[i][SBC_CC_REFUSE_REASON].asCStr(),
		    NULL, headers);

	  // call 'end' of call control modules up to here
	  call_end_ts.tv_sec = call_start_ts.tv_sec;
	  call_end_ts.tv_usec = call_start_ts.tv_usec;
	  CCEnd(cc_it);
	  return false;
	}

	case SBC_CC_SET_CALL_TIMER_ACTION: {
	  if (cc_timer_id > SBC_TIMER_ID_CALL_TIMERS_END) {
	    ERROR("too many call timers - ignoring timer\n");
	    continue;
	  }

	  if (ret[i].size() < 2 ||
	      (!(isArgInt(ret[i][SBC_CC_TIMER_TIMEOUT]) ||
		 isArgDouble(ret[i][SBC_CC_TIMER_TIMEOUT])))) {
	    ERROR("in call control module '%s' - SET_CALL_TIMER action parameters missing: '%s'\n",
		  cc_if.cc_name.c_str(), AmArg::print(ret[i]).c_str());
	    continue;
	  }

	  double timeout;
	  if (isArgInt(ret[i][SBC_CC_TIMER_TIMEOUT]))
	    timeout = ret[i][SBC_CC_TIMER_TIMEOUT].asInt();
	  else
	    timeout = ret[i][SBC_CC_TIMER_TIMEOUT].asDouble();

	  DBG("saving call timer %i: timeout %f\n", cc_timer_id, timeout);
	  saveCallTimer(cc_timer_id, timeout);
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

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != call_profile.cc_interfaces.end(); cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);                // cc name
    di_args.push(getLocalTag());                 // call ltag
    di_args.push((AmObject*)&call_profile);     // call profile
    di_args.push(AmArg());                       // timestamps
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    di_args.back().push((int)call_connect_ts.tv_sec);
    di_args.back().push((int)call_connect_ts.tv_usec);
    for (int i=0;i<2;i++)
      di_args.back().push((int)0);
    di_args.push(other_id);                      // other leg ltag


    try {
      (*cc_mod)->invoke("connect", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface connect "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      stopCall();
      return;
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface connect "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
      stopCall();
      return;
    }

    cc_mod++;
  }
}

void SBCDialog::CCEnd() {
  CCEnd(call_profile.cc_interfaces.end());
}

void SBCDialog::CCEnd(const CCInterfaceListIteratorT& end_interface) {
  vector<AmDynInvoke*>::iterator cc_mod=cc_modules.begin();

  for (CCInterfaceListIteratorT cc_it=call_profile.cc_interfaces.begin();
       cc_it != end_interface; cc_it++) {
    CCInterface& cc_if = *cc_it;

    AmArg di_args,ret;
    di_args.push(cc_if.cc_name);
    di_args.push(getLocalTag());                 // call ltag
    di_args.push((AmObject*)&call_profile);
    di_args.push(AmArg());                       // timestamps
    di_args.back().push((int)call_start_ts.tv_sec);
    di_args.back().push((int)call_start_ts.tv_usec);
    di_args.back().push((int)call_connect_ts.tv_sec);
    di_args.back().push((int)call_connect_ts.tv_usec);
    di_args.back().push((int)call_end_ts.tv_sec);
    di_args.back().push((int)call_end_ts.tv_usec);

    try {
      (*cc_mod)->invoke("end", di_args, ret);
    } catch (const AmArg::OutOfBoundsException& e) {
      ERROR("OutOfBoundsException executing call control interface end "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
    } catch (const AmArg::TypeMismatchException& e) {
      ERROR("TypeMismatchException executing call control interface end "
	    "module '%s' named '%s', parameters '%s'\n",
	    cc_if.cc_module.c_str(), cc_if.cc_name.c_str(),
	    AmArg::print(di_args).c_str());
    }

    cc_mod++;
  }
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

  if (call_profile.sst_enabled_value) {
    if (NULL == SBCFactory::session_timer_fact) {
      ERROR("session_timer module not loaded - unable to create call with SST\n");
      throw AmSession::Exception(500, SIP_REPLY_SERVER_INTERNAL_ERROR);
    }

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
  
  if (!call_profile.next_hop.empty()) {
    callee_dlg.next_hop = call_profile.next_hop;
  }

  if(outbound_interface >= 0)
    callee_dlg.outbound_interface = outbound_interface;

  callee_dlg.setOAEnabled(false); // ???

  if(rtprelay_interface >= 0)
    callee_session->setRtpRelayInterface(rtprelay_interface);

  callee_session->setRtpRelayTransparentSeqno(call_profile.rtprelay_transparent_seqno);
  callee_session->setRtpRelayTransparentSSRC(call_profile.rtprelay_transparent_ssrc);

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
  dlg.setRel100State(Am100rel::REL100_IGNORED);

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
  if (isActiveFilter(call_profile.headerfilter) && ev->event_id == B2BSipRequest) {
    // header filter
    B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
    assert(req_ev);
    inplaceHeaderFilter(req_ev->req.hdrs,
			call_profile.headerfilter_list, call_profile.headerfilter);
  } else {
    if (ev->event_id == B2BSipReply) {
      if (isActiveFilter(call_profile.headerfilter) ||
	  (call_profile.reply_translations.size())) {
	B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
	assert(reply_ev);

	// header filter
	if (isActiveFilter(call_profile.headerfilter)) {
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

void SBCCalleeSession::process(AmEvent* ev) {
  SBCControlEvent* ctl_event;
  if (ev->event_id == SBCControlEvent_ID &&
      (ctl_event = dynamic_cast<SBCControlEvent*>(ev)) != NULL) {
    onControlCmd(ctl_event->cmd, ctl_event->params);
    return;
  }

  AmB2BCalleeSession::process(ev);
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

  if (fwd && isActiveFilter(call_profile.messagefilter)) {
    bool is_filtered = (call_profile.messagefilter == Whitelist) ^ 
      (call_profile.messagefilter_list.find(req.method) != 
       call_profile.messagefilter_list.end());
    if (is_filtered) {
      DBG("replying 405 to filtered message '%s'\n", req.method.c_str());
      dlg.reply(req, 405, "Method Not Allowed", NULL, "", SIP_FLAGS_VERBATIM);
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
  DBG("onSipReply: content-type = %s\n",reply.body.getCTStr().c_str());
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

void SBCCalleeSession::onSendRequest(AmSipRequest& req, int flags)
{
  if (NULL != auth) {
    DBG("auth->onSendRequest cseq = %d\n", req.cseq);
    auth->onSendRequest(req, flags);
  }
  
  AmB2BCalleeSession::onSendRequest(req, flags);
}

void SBCCalleeSession::onControlCmd(string& cmd, AmArg& params) {
  if (cmd == "teardown") {
    DBG("relaying teardown control cmd to A leg\n");
    relayEvent(new SBCControlEvent(cmd, params));
    return;
  }
  DBG("ignoring unknown control cmd : '%s'\n", cmd.c_str());
}

void SBCCalleeSession::filterBody(AmSipRequest &req, AmSdp &sdp)
{
  ::filterBody(req, sdp, call_profile, a_leg);
}

void SBCCalleeSession::filterBody(AmSipReply &reply, AmSdp &sdp)
{
  ::filterBody(reply, sdp, call_profile, a_leg);
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
