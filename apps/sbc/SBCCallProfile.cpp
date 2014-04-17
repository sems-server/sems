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

#include "SBCCallProfile.h"
#include "SBC.h"
#include <algorithm>

#include "log.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmConfig.h"

#include "SBCCallControlAPI.h"
#include "RTPParameters.h"
#include "SDPFilter.h"
#include "RegisterCache.h"

#include "sip/pcap_logger.h"

typedef vector<SdpPayload>::iterator PayloadIterator;
static string payload2str(const SdpPayload &p);


//////////////////////////////////////////////////////////////////////////////////
// helper defines for parameter evaluation

#define REPLACE_VALS req, app_param, ruri_parser, from_parser, to_parser

// FIXME: r_type in replaceParameters is just for debug output?

#define REPLACE_STR(what) do {			  \
    what = ctx.replaceParameters(what, #what, req);	\
    DBG(#what " = '%s'\n", what.c_str());		\
  } while(0)

#define REPLACE_NONEMPTY_STR(what) do {		\
    if (!what.empty()) {			\
      REPLACE_STR(what);			\
    }						\
  } while(0)

#define REPLACE_NUM(what, dst_num) do {		\
    if (!what.empty()) {			    \
      what = ctx.replaceParameters(what, #what, req);	\
      unsigned int num;					\
      if (str2i(what, num)) {				   \
	ERROR(#what " '%s' not understood\n", what.c_str());	\
	return false;						\
      }								\
      DBG(#what " = '%s'\n", what.c_str());			\
      dst_num = num;						\
    }								\
  } while(0)

#define REPLACE_BOOL(what, dst_value) do {	\
    if (!what.empty()) {			    \
      what = ctx.replaceParameters(what, #what, req);	\
      if (!what.empty()) {				\
	if (!str2bool(what, dst_value)) {			\
	  ERROR(#what " '%s' not understood\n", what.c_str());	\
	  return false;						\
	}							\
      }								\
      DBG(#what " = '%s'\n", dst_value ? "yes" : "no");		\
    }								\
  } while(0)

#define REPLACE_IFACE_RTP(what, iface) do {				\
    if (!what.empty()) {						\
      what = ctx.replaceParameters(what, #what, req);			\
      DBG("set " #what " to '%s'\n", what.c_str());			\
      if (!what.empty()) {						\
	EVALUATE_IFACE_RTP(what, iface);				\
      }									\
    }									\
  } while(0)

#define EVALUATE_IFACE_RTP(what, iface) do {				\
    if (what == "default") iface = 0;					\
    else {								\
      map<string,unsigned short>::iterator name_it =			\
	AmConfig::RTP_If_names.find(what);				\
      if (name_it != AmConfig::RTP_If_names.end())			\
	iface = name_it->second;					\
      else {								\
	ERROR("selected " #what " '%s' does not exist as a media interface. " \
	      "Please check the 'additional_interfaces' "		\
	      "parameter in the main configuration file.",		\
	      what.c_str());						\
	return false;							\
      }									\
    }									\
  } while(0)

#define REPLACE_IFACE_SIP(what, iface) do {		\
    if (!what.empty()) {			    \
      what = ctx.replaceParameters(what, #what, req);	\
      DBG("set " #what " to '%s'\n", what.c_str());	\
      if (!what.empty()) {				\
	if (what == "default") iface = 0;		\
	else {								\
	  map<string,unsigned short>::iterator name_it =		\
	    AmConfig::SIP_If_names.find(what);				\
	  if (name_it != AmConfig::RTP_If_names.end()) \
	    iface = name_it->second;					\
	  else {							\
	    ERROR("selected " #what " '%s' does not exist as a signaling" \
		  " interface. "					\
		  "Please check the 'additional_interfaces' "		\
		  "parameter in the main configuration file.",		\
		  what.c_str());					\
	    return false;						\
	  }								\
	}								\
      }									\
    }									\
  } while(0)

//////////////////////////////////////////////////////////////////////////////////

bool SBCCallProfile::readFromConfiguration(const string& name,
					   const string profile_file_name) {
  AmConfigReader cfg;
  if (cfg.loadFile(profile_file_name)) {
    ERROR("reading SBC call profile from '%s'\n", profile_file_name.c_str());
    return false;
  }

  profile_file = profile_file_name;

  ruri = cfg.getParameter("RURI");
  ruri_host = cfg.getParameter("RURI_host");
  from = cfg.getParameter("From");
  to = cfg.getParameter("To");
  //contact = cfg.getParameter("Contact");

  callid = cfg.getParameter("Call-ID");

  transparent_dlg_id = cfg.getParameter("transparent_dlg_id") == "yes";
  dlg_nat_handling = cfg.getParameter("dlg_nat_handling") == "yes";

  force_outbound_proxy = cfg.getParameter("force_outbound_proxy") == "yes";
  outbound_proxy = cfg.getParameter("outbound_proxy");

  aleg_force_outbound_proxy = cfg.getParameter("aleg_force_outbound_proxy") == "yes";
  aleg_outbound_proxy = cfg.getParameter("aleg_outbound_proxy");

  next_hop = cfg.getParameter("next_hop");
  next_hop_1st_req = cfg.getParameter("next_hop_1st_req") == "yes";
  patch_ruri_next_hop = cfg.getParameter("patch_ruri_next_hop") == "yes";
  next_hop_fixed = cfg.getParameter("next_hop_fixed") == "yes";

  aleg_next_hop = cfg.getParameter("aleg_next_hop");

  allow_subless_notify = cfg.getParameter("allow_subless_notify", "yes") == "yes";

  if (!readFilter(cfg, "header_filter", "header_list", headerfilter, false))
    return false;
  
  if (!readFilter(cfg, "message_filter", "message_list", messagefilter, false))
    return false;

  if (!readFilter(cfg, "sdp_filter", "sdpfilter_list", sdpfilter, true))
    return false;

  if (!readFilter(cfg, "media_filter", "mediafilter_list", mediafilter, true))
    return false;

  anonymize_sdp = cfg.getParameter("sdp_anonymize", "no") == "yes";

  // SDP alines filter
  if (!readFilter(cfg, "sdp_alines_filter", "sdp_alinesfilter_list", 
		  sdpalinesfilter, false))
    return false;

  sst_enabled = cfg.getParameter("enable_session_timer");
  if (cfg.hasParameter("enable_aleg_session_timer")) {
    sst_aleg_enabled = cfg.getParameter("enable_aleg_session_timer");
  } else {
    sst_aleg_enabled = sst_enabled;
  }

#define CP_SST_CFGVAR(cfgprefix, cfgkey, dstcfg)			\
    if (cfg.hasParameter(cfgprefix cfgkey)) {				\
      dstcfg.setParameter(cfgkey, cfg.getParameter(cfgprefix cfgkey));	\
    } else if (cfg.hasParameter(cfgkey)) {				\
      dstcfg.setParameter(cfgkey, cfg.getParameter(cfgkey));		\
    } else if (SBCFactory::instance()->cfg.hasParameter(cfgkey)) {	\
      dstcfg.setParameter(cfgkey, SBCFactory::instance()->		\
			  cfg.getParameter(cfgkey));			\
    }

  if (sst_enabled.size() && sst_enabled != "no") {
    if (NULL == SBCFactory::instance()->session_timer_fact) {
      ERROR("session_timer module not loaded thus SST not supported, but "
	    "required for profile '%s' (%s)\n", name.c_str(), profile_file_name.c_str());
      return false;
    }

    sst_b_cfg.setParameter("enable_session_timer", "yes");
    // create sst_cfg with values from aleg_*
    CP_SST_CFGVAR("", "session_expires", sst_b_cfg);
    CP_SST_CFGVAR("", "minimum_timer", sst_b_cfg);
    CP_SST_CFGVAR("", "maximum_timer", sst_b_cfg);
    CP_SST_CFGVAR("", "session_refresh_method", sst_b_cfg);
    CP_SST_CFGVAR("", "accept_501_reply", sst_b_cfg);
  }

  if (sst_aleg_enabled.size() && sst_aleg_enabled != "no") {
    sst_a_cfg.setParameter("enable_session_timer", "yes");
    // create sst_a_cfg superimposing values from aleg_*
    CP_SST_CFGVAR("aleg_", "session_expires", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "minimum_timer", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "maximum_timer", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "session_refresh_method", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "accept_501_reply", sst_a_cfg);
  }
#undef CP_SST_CFGVAR

  fix_replaces_inv = cfg.getParameter("fix_replaces_inv");
  fix_replaces_ref = cfg.getParameter("fix_replaces_ref");;
  
  auth_enabled = cfg.getParameter("enable_auth", "no") == "yes";
  auth_credentials.user = cfg.getParameter("auth_user");
  auth_credentials.pwd = cfg.getParameter("auth_pwd");

  auth_aleg_enabled = cfg.getParameter("enable_aleg_auth", "no") == "yes";
  auth_aleg_credentials.user = cfg.getParameter("auth_aleg_user");
  auth_aleg_credentials.pwd = cfg.getParameter("auth_aleg_pwd");

  uas_auth_bleg_enabled = cfg.getParameter("enable_bleg_uas_auth", "no") == "yes";
  uas_auth_bleg_credentials.realm = cfg.getParameter("uas_auth_bleg_realm");
  uas_auth_bleg_credentials.user = cfg.getParameter("uas_auth_bleg_user");
  uas_auth_bleg_credentials.pwd = cfg.getParameter("uas_auth_bleg_pwd");

  if (!cfg.getParameter("call_control").empty()) {
    vector<string> cc_sections = explode(cfg.getParameter("call_control"), ",");
    for (vector<string>::iterator it =
	   cc_sections.begin(); it != cc_sections.end(); it++) {
      DBG("reading call control '%s'\n", it->c_str());
      cc_interfaces.push_back(CCInterface(*it));
      CCInterface& cc_if = cc_interfaces.back();
      cc_if.cc_module = cfg.getParameter(*it + "_module");

      AmArg mandatory_values;

      // check if module is loaded and if, get mandatory config values
      if (cc_if.cc_module.find('$') == string::npos && 
	  cc_if.cc_name.find('$') == string::npos) {
	AmDynInvokeFactory* df = AmPlugIn::instance()->getFactory4Di(cc_if.cc_module);
	if (NULL == df) {
	  ERROR("Call control module '%s' used in call profile "
		"'%s' is not loaded\n", cc_if.cc_module.c_str(), name.c_str());
	  return false;
	}
	AmDynInvoke* di = df->getInstance();
	AmArg args;
	try {
	  di->invoke(CC_INTERFACE_MAND_VALUES_METHOD, args, mandatory_values);
	} catch (AmDynInvoke::NotImplemented& ni) { }
      }

      size_t cc_name_prefix_len = it->length()+1;

      // read interface values
      for (std::map<string,string>::const_iterator cfg_it =
	     cfg.begin(); cfg_it != cfg.end(); cfg_it++) {
	if (cfg_it->first.substr(0, cc_name_prefix_len) != *it+"_")
	  continue;

	if (cfg_it->first.size() <= cc_name_prefix_len ||
	    cfg_it->first == *it+"_module")
	  continue;

	cc_if.cc_values[cfg_it->first.substr(cc_name_prefix_len)] = cfg_it->second;
      }

      if (isArgArray(mandatory_values)) {
	for (size_t i=0;i<mandatory_values.size();i++) {
	  if (!isArgCStr(mandatory_values[i])) continue;
	  if (cc_if.cc_values.find(mandatory_values[i].asCStr()) == cc_if.cc_values.end()) {
	    ERROR("value '%s' for SBC profile '%s' in '%s' not defined. set %s_%s=...\n",
		  mandatory_values[i].asCStr(), name.c_str(), profile_file_name.c_str(),
		  it->c_str(), mandatory_values[i].asCStr());
	    return false;
	  }
	}
      }
    }
  }

  vector<string> reply_translations_v =
    explode(cfg.getParameter("reply_translations"), "|");
  for (vector<string>::iterator it =
	 reply_translations_v.begin(); it != reply_translations_v.end(); it++) {
    // expected: "603=>488 Not acceptable here"
    vector<string> trans_components = explode(*it, "=>");
    if (trans_components.size() != 2) {
      ERROR("%s: entry '%s' in reply_translations could not be understood.\n",
	    name.c_str(), it->c_str());
      ERROR("expected 'from_code=>to_code reason'\n");
      return false;
    }

    unsigned int from_code, to_code;
    if (str2i(trans_components[0], from_code)) {
      ERROR("%s: code '%s' in reply_translations not understood.\n",
	    name.c_str(), trans_components[0].c_str());
      return false;
    }
    unsigned int s_pos = 0;
    string to_reply = trans_components[1];
    while (s_pos < to_reply.length() && to_reply[s_pos] != ' ')
      s_pos++;
    if (str2i(to_reply.substr(0, s_pos), to_code)) {
      ERROR("%s: code '%s' in reply_translations not understood.\n",
	    name.c_str(), to_reply.substr(0, s_pos).c_str());
      return false;
    }

    if (s_pos < to_reply.length())
      s_pos++;
    // DBG("got translation %u => %u %s\n",
    // 	from_code, to_code, to_reply.substr(s_pos).c_str());
    reply_translations[from_code] = make_pair(to_code, to_reply.substr(s_pos));
  }

  // append_headers=P-Received-IP: $si\r\nP-Received-Port:$sp\r\n
  append_headers = cfg.getParameter("append_headers");
  append_headers_req = cfg.getParameter("append_headers_req");
  aleg_append_headers_req = cfg.getParameter("aleg_append_headers_req");

  refuse_with = cfg.getParameter("refuse_with");

  rtprelay_enabled = cfg.getParameter("enable_rtprelay");
  aleg_force_symmetric_rtp = cfg.getParameter("rtprelay_force_symmetric_rtp");
  force_symmetric_rtp = aleg_force_symmetric_rtp;
  msgflags_symmetric_rtp = cfg.getParameter("rtprelay_msgflags_symmetric_rtp") == "yes";

  rtprelay_interface = cfg.getParameter("rtprelay_interface");
  aleg_rtprelay_interface = cfg.getParameter("aleg_rtprelay_interface");

  rtprelay_transparent_seqno =
    cfg.getParameter("rtprelay_transparent_seqno", "yes") == "yes";
  rtprelay_transparent_ssrc =
    cfg.getParameter("rtprelay_transparent_ssrc", "yes") == "yes";
  rtprelay_dtmf_filtering =
    cfg.getParameter("rtprelay_dtmf_filtering", "no") == "yes";
  rtprelay_dtmf_detection =
    cfg.getParameter("rtprelay_dtmf_detection", "no") == "yes";

  outbound_interface = cfg.getParameter("outbound_interface");
  aleg_outbound_interface = cfg.getParameter("aleg_outbound_interface");

  contact.displayname = cfg.getParameter("contact_displayname");
  contact.user = cfg.getParameter("contact_user");
  contact.host = cfg.getParameter("contact_host");
  contact.port = cfg.getParameter("contact_port");

  contact.hiding = cfg.getParameter("enable_contact_hiding", "no") == "yes";
  contact.hiding_prefix = cfg.getParameter("contact_hiding_prefix");
  contact.hiding_vars = cfg.getParameter("contact_hiding_vars");

  if (!codec_prefs.readConfig(cfg)) return false;
  if (!transcoder.readConfig(cfg)) return false;
  hold_settings.readConfig(cfg);

  msg_logger_path = cfg.getParameter("msg_logger_path");
  log_rtp = cfg.getParameter("log_rtp","no") == "yes";
  log_sip = cfg.getParameter("log_sip","yes") == "yes";

  reg_caching = cfg.getParameter("enable_reg_caching","no") == "yes";
  min_reg_expires = cfg.getParameterInt("min_reg_expires",0);
  max_ua_expires = cfg.getParameterInt("max_ua_expires",0);

  max_491_retry_time = cfg.getParameterInt("max_491_retry_time", 2000);

  md5hash = "<unknown>";
  if (!cfg.getMD5(profile_file_name, md5hash)){
    ERROR("calculating MD5 of file %s\n", profile_file_name.c_str());
  }

  INFO("SBC: loaded SBC profile '%s' - MD5: %s\n", name.c_str(), md5hash.c_str());

  if (!refuse_with.empty()) {
    INFO("SBC:      refusing calls with '%s'\n", refuse_with.c_str());
  } else {
    INFO("SBC:      RURI      = '%s'\n", ruri.c_str());
    INFO("SBC:      RURI-host = '%s'\n", ruri_host.c_str());
    INFO("SBC:      From = '%s'\n", from.c_str());
    INFO("SBC:      To   = '%s'\n", to.c_str());
    // if (!contact.empty()) {
    //   INFO("SBC:      Contact   = '%s'\n", contact.c_str());
    // }
    if (!callid.empty()) {
      INFO("SBC:      Call-ID   = '%s'\n", callid.c_str());
    }

    INFO("SBC:      force outbound proxy: %s\n", force_outbound_proxy?"yes":"no");
    INFO("SBC:      outbound proxy = '%s'\n", outbound_proxy.c_str());

    if (!outbound_interface.empty()) {
      INFO("SBC:      outbound interface = '%s'\n", outbound_interface.c_str());
    }
    
    if (!aleg_outbound_interface.empty()) {
      INFO("SBC:      A leg outbound interface = '%s'\n", aleg_outbound_interface.c_str());
    }

    INFO("SBC:      A leg force outbound proxy: %s\n", aleg_force_outbound_proxy?"yes":"no");
    INFO("SBC:      A leg outbound proxy = '%s'\n", aleg_outbound_proxy.c_str());

    if (!next_hop.empty()) {
      INFO("SBC:      next hop = %s (%s, %s)\n", next_hop.c_str(),
	   next_hop_1st_req ? "1st req" : "all reqs", next_hop_fixed?"fixed":"not fixed");
    }

    if (!aleg_next_hop.empty()) {
      INFO("SBC:      A leg next hop = %s\n", aleg_next_hop.c_str());
    }

    string filter_type; size_t filter_elems;
    filter_type = headerfilter.size() ?
      FilterType2String(headerfilter.back().filter_type) : "disabled";
    filter_elems = headerfilter.size() ? headerfilter.back().filter_list.size() : 0;
    INFO("SBC:      header filter  is %s, %zd items in list\n",
	 filter_type.c_str(), filter_elems);

    filter_type = messagefilter.size() ?
      FilterType2String(messagefilter.back().filter_type) : "disabled";
    filter_elems = messagefilter.size() ? messagefilter.back().filter_list.size() : 0;
    INFO("SBC:      message filter is %s, %zd items in list\n",
	 filter_type.c_str(), filter_elems);

    filter_type = sdpfilter.size() ?
      FilterType2String(sdpfilter.back().filter_type) : "disabled";
    filter_elems = sdpfilter.size() ? sdpfilter.back().filter_list.size() : 0;
    INFO("SBC:      SDP filter is %sabled, %s, %zd items in list, %sanonymizing SDP\n",
	 sdpfilter.size()?"en":"dis", filter_type.c_str(), filter_elems,
	 anonymize_sdp?"":"not ");

    filter_type = sdpalinesfilter.size() ?
      FilterType2String(sdpalinesfilter.back().filter_type) : "disabled";
    filter_elems = sdpalinesfilter.size() ? sdpalinesfilter.back().filter_list.size() : 0;
    INFO("SBC:      SDP alines-filter is %sabled, %s, %zd items in list\n",
	 sdpalinesfilter.size()?"en":"dis", filter_type.c_str(), filter_elems);
    
    filter_type = mediafilter.size() ?
      FilterType2String(mediafilter.back().filter_type) : "disabled";
    filter_elems = mediafilter.size() ? mediafilter.back().filter_list.size() : 0;
    INFO("SBC:      SDP filter is %sabled, %s, %zd items in list\n",
	 mediafilter.size()?"en":"dis", filter_type.c_str(), filter_elems);

    INFO("SBC:      fixing Replaces in INVITE: '%s'\n", fix_replaces_inv.c_str());
    INFO("SBC:      fixing Replaces in REFER: '%s'\n", fix_replaces_ref.c_str());


    INFO("SBC:      RTP relay enabled: '%s'\n", rtprelay_enabled.c_str());
    if (!rtprelay_enabled.empty() && rtprelay_enabled != "no") {
      if (!force_symmetric_rtp.empty()) {
	INFO("SBC:      RTP force symmetric RTP: %s\n",
	     force_symmetric_rtp.c_str());
      }
      if (msgflags_symmetric_rtp) {
	INFO("SBC:      P-MsgFlags symmetric RTP detection enabled\n");
      }
      if (!aleg_rtprelay_interface.empty()) {
	INFO("SBC:      RTP Relay interface A leg '%s'\n", aleg_rtprelay_interface.c_str());
      }
      if (!rtprelay_interface.empty()) {
	INFO("SBC:      RTP Relay interface B leg '%s'\n", rtprelay_interface.c_str());
      }

      INFO("SBC:      RTP Relay %s seqno\n",
	   rtprelay_transparent_seqno?"transparent":"opaque");
      INFO("SBC:      RTP Relay %s SSRC\n",
	   rtprelay_transparent_ssrc?"transparent":"opaque");
      INFO("SBC:      RTP Relay RTP DTMF filtering %sabled\n",
	   rtprelay_dtmf_filtering?"en":"dis");
      INFO("SBC:      RTP Relay RTP DTMF detection %sabled\n",
	   rtprelay_dtmf_detection?"en":"dis");
    }

    INFO("SBC:      SST on A leg enabled: '%s'\n", sst_aleg_enabled.empty() ?
	 "no" : sst_aleg_enabled.c_str());
    if (sst_aleg_enabled.size() && sst_aleg_enabled != "no") {
      INFO("SBC:              session_expires=%s\n",
	   sst_a_cfg.getParameter("session_expires").c_str());
      INFO("SBC:              minimum_timer=%s\n",
	   sst_a_cfg.getParameter("minimum_timer").c_str());
      INFO("SBC:              maximum_timer=%s\n",
	   sst_a_cfg.getParameter("maximum_timer").c_str());
      INFO("SBC:              session_refresh_method=%s\n",
	   sst_a_cfg.getParameter("session_refresh_method").c_str());
      INFO("SBC:              accept_501_reply=%s\n",
	   sst_a_cfg.getParameter("accept_501_reply").c_str());
    }
    INFO("SBC:      SST on B leg enabled: '%s'\n", sst_enabled.empty() ?
	 "no" : sst_enabled.c_str());
    if (sst_enabled.size() && sst_enabled != "no") {
      INFO("SBC:              session_expires=%s\n",
	   sst_b_cfg.getParameter("session_expires").c_str());
      INFO("SBC:              minimum_timer=%s\n",
	   sst_b_cfg.getParameter("minimum_timer").c_str());
      INFO("SBC:              maximum_timer=%s\n",
	   sst_b_cfg.getParameter("maximum_timer").c_str());
      INFO("SBC:              session_refresh_method=%s\n",
	   sst_b_cfg.getParameter("session_refresh_method").c_str());
      INFO("SBC:              accept_501_reply=%s\n",
	   sst_b_cfg.getParameter("accept_501_reply").c_str());
    }

    INFO("SBC:      SIP auth %sabled\n", auth_enabled?"en":"dis");
    INFO("SBC:      SIP auth for A leg %sabled\n", auth_aleg_enabled?"en":"dis");
    INFO("SBC:      SIP UAS auth for B leg %sabled\n", uas_auth_bleg_enabled?"en":"dis");

    if (cc_interfaces.size()) {
      string cc_if_names;
      for (CCInterfaceListIteratorT it =
	     cc_interfaces.begin(); it != cc_interfaces.end(); it++) {
	cc_if_names = it->cc_name+",";
	cc_if_names.erase(cc_if_names.size()-1,1);
	INFO("SBC:      Call Control: %s\n", cc_if_names.c_str());
      }
    }

    if (reply_translations.size()) {
      string reply_trans_codes;
      for(map<unsigned int, std::pair<unsigned int, string> >::iterator it=
	    reply_translations.begin(); it != reply_translations.end(); it++)
	reply_trans_codes += int2str(it->first)+", ";
      reply_trans_codes.erase(reply_trans_codes.length()-2);
      INFO("SBC:      reply translation for  %s\n", reply_trans_codes.c_str());
    }
  }

  if (append_headers.size()) {
    INFO("SBC:      append headers '%s'\n", append_headers.c_str());
  }

  INFO("SBC:      reg-caching: '%s'\n", reg_caching ? "yes" : "no");
  INFO("SBC:      min_reg_expires: %i\n", min_reg_expires);
  INFO("SBC:      max_ua_expires: %i\n", max_ua_expires);

  codec_prefs.infoPrint();
  transcoder.infoPrint();

  return true;
}

static bool payloadDescsEqual(const vector<PayloadDesc> &a, const vector<PayloadDesc> &b)
{
  // not sure if this function is really needed (seems that vectors can be
  // compared using builtin operator== but anyway ...)
  if (a.size() != b.size()) return false;
  vector<PayloadDesc>::const_iterator ia = a.begin();
  vector<PayloadDesc>::const_iterator ib = b.begin();
  for (; ia != a.end(); ++ia, ++ib) {
    if (!(*ia == *ib)) return false;
  }

  return true;
}

bool SBCCallProfile::operator==(const SBCCallProfile& rhs) const {
  bool res =
    ruri == rhs.ruri &&
    ruri_host == rhs.ruri_host &&
    from == rhs.from &&
    to == rhs.to &&
    //contact == rhs.contact &&
    callid == rhs.callid &&
    outbound_proxy == rhs.outbound_proxy &&
    force_outbound_proxy == rhs.force_outbound_proxy &&
    aleg_outbound_proxy == rhs.aleg_outbound_proxy &&
    aleg_force_outbound_proxy == rhs.aleg_force_outbound_proxy &&
    next_hop == rhs.next_hop &&
    next_hop_1st_req == rhs.next_hop_1st_req &&
    next_hop_fixed == rhs.next_hop_fixed &&
    patch_ruri_next_hop == rhs.patch_ruri_next_hop &&
    aleg_next_hop == rhs.aleg_next_hop &&
    headerfilter == rhs.headerfilter &&
    //headerfilter_list == rhs.headerfilter_list &&
    messagefilter == rhs.messagefilter &&
    //messagefilter_list == rhs.messagefilter_list &&
    //sdpfilter_enabled == rhs.sdpfilter_enabled &&
    sdpfilter == rhs.sdpfilter &&
    mediafilter == rhs.mediafilter &&
    sst_enabled == rhs.sst_enabled &&
    sst_aleg_enabled == rhs.sst_aleg_enabled &&
    auth_enabled == rhs.auth_enabled &&
    auth_aleg_enabled == rhs.auth_aleg_enabled &&
    reply_translations == rhs.reply_translations &&
    append_headers == rhs.append_headers &&
    refuse_with == rhs.refuse_with &&
    rtprelay_enabled == rhs.rtprelay_enabled &&
    force_symmetric_rtp == rhs.force_symmetric_rtp &&
    msgflags_symmetric_rtp == rhs.msgflags_symmetric_rtp;

  if (auth_enabled) {
    res = res &&
      auth_credentials.user == rhs.auth_credentials.user &&
      auth_credentials.pwd == rhs.auth_credentials.pwd;
  }
  if (auth_aleg_enabled) {
    res = res &&
      auth_aleg_credentials.user == rhs.auth_aleg_credentials.user &&
      auth_aleg_credentials.pwd == rhs.auth_aleg_credentials.pwd;
  }
  res = res && (codec_prefs == rhs.codec_prefs);
  res = res && (transcoder == rhs.transcoder);
  return res;
}

string stringset_print(const set<string>& s) {
  string res;
  for (set<string>::const_iterator i=s.begin(); i != s.end(); i++)
    res += *i+" ";
  return res;
}

string SBCCallProfile::print() const {
  string res = 
    "SBC call profile dump: ~~~~~~~~~~~~~~~~~\n";
  res += "ruri:                 " + ruri + "\n";
  res += "ruri_host:            " + ruri_host + "\n";
  res += "from:                 " + from + "\n";
  res += "to:                   " + to + "\n";
  // res += "contact:              " + contact + "\n";
  res += "callid:               " + callid + "\n";
  res += "outbound_proxy:       " + outbound_proxy + "\n";
  res += "force_outbound_proxy: " + string(force_outbound_proxy?"true":"false") + "\n";
  res += "aleg_outbound_proxy:       " + aleg_outbound_proxy + "\n";
  res += "aleg_force_outbound_proxy: " + string(aleg_force_outbound_proxy?"true":"false") + "\n";
  res += "next_hop:             " + next_hop + "\n";
  res += "next_hop_1st_req:     " + string(next_hop_1st_req ? "true":"false") + "\n";
  res += "next_hop_fixed:       " + string(next_hop_fixed ? "true":"false") + "\n";
  res += "aleg_next_hop:        " + aleg_next_hop + "\n";
  // res += "headerfilter:         " + string(FilterType2String(headerfilter)) + "\n";
  // res += "headerfilter_list:    " + stringset_print(headerfilter_list) + "\n";
  // res += "messagefilter:        " + string(FilterType2String(messagefilter)) + "\n";
  // res += "messagefilter_list:   " + stringset_print(messagefilter_list) + "\n";
  // res += "sdpfilter_enabled:    " + string(sdpfilter_enabled?"true":"false") + "\n";
  // res += "sdpfilter:            " + string(FilterType2String(sdpfilter)) + "\n";
  // res += "sdpfilter_list:       " + stringset_print(sdpfilter_list) + "\n";
  // res += "sdpalinesfilter:      " + string(FilterType2String(sdpalinesfilter)) + "\n";
  // res += "sdpalinesfilter_list: " + stringset_print(sdpalinesfilter_list) + "\n";
  res += "sst_enabled:          " + sst_enabled + "\n";
  res += "sst_aleg_enabled:     " + sst_aleg_enabled + "\n";
  res += "auth_enabled:         " + string(auth_enabled?"true":"false") + "\n";
  res += "auth_user:            " + auth_credentials.user+"\n";
  res += "auth_pwd:             " + auth_credentials.pwd+"\n";
  res += "auth_aleg_enabled:    " + string(auth_aleg_enabled?"true":"false") + "\n";
  res += "auth_aleg_user:       " + auth_aleg_credentials.user+"\n";
  res += "auth_aleg_pwd:        " + auth_aleg_credentials.pwd+"\n";
  res += "rtprelay_enabled:     " + rtprelay_enabled + "\n";
  res += "force_symmetric_rtp:  " + force_symmetric_rtp;
  res += "msgflags_symmetric_rtp: " + string(msgflags_symmetric_rtp?"true":"false") + "\n";
  
  res += codec_prefs.print();
  res += transcoder.print();

  if (reply_translations.size()) {
    string reply_trans_codes;
    for(map<unsigned int, std::pair<unsigned int, string> >::const_iterator it=
	  reply_translations.begin(); it != reply_translations.end(); it++)
      reply_trans_codes += int2str(it->first)+"=>"+
	int2str(it->second.first)+" " + it->second.second+", ";
    reply_trans_codes.erase(reply_trans_codes.length()-2);

    res += "reply_trans_codes:     " + reply_trans_codes +"\n";
  }
  res += "append_headers:     " + append_headers + "\n";
  res += "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
  return res;
}

static bool isTranscoderNeeded(const AmSipRequest& req, vector<PayloadDesc> &caps,
			       bool default_value)
{
  const AmMimeBody* body = req.body.hasContentType(SIP_APPLICATION_SDP);
  if (!body) return default_value;

  AmSdp sdp;
  int res = sdp.parse((const char *)body->getPayload());
  if (res != 0) {
    DBG("SDP parsing failed!\n");
    return default_value;
  }
  
  // not nice, but we need to compare codec names and thus normalized SDP is
  // required
  normalizeSDP(sdp, false, "");

  // go through payloads and try find one of the supported ones
  for (vector<SdpMedia>::iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) { 
    for (vector<SdpPayload>::iterator p = m->payloads.begin(); p != m->payloads.end(); ++p) {
      for (vector<PayloadDesc>::iterator i = caps.begin(); i != caps.end(); ++i) {
        if (i->match(*p)) return false; // found compatible codec
      }
    }
  }

  return true; // no compatible codec found, transcoding needed
}

void SBCCallProfile::eval_sst_config(ParamReplacerCtx& ctx,
				     const AmSipRequest& req,
				     AmConfigReader& sst_cfg)
{

#define SST_CFG_PARAM_COUNT 5  // Change if you add/remove params in below
  
  static const char* _sst_cfg_params[] = {
    "session_expires",
    "minimum_timer",
    "maximum_timer",
    "session_refresh_method",
    "accept_501_reply",
  };

  for(unsigned int i=0; i<SST_CFG_PARAM_COUNT; i++) {
    if (sst_cfg.hasParameter(_sst_cfg_params[i])) {
      string newval = 
	ctx.replaceParameters(sst_cfg.getParameter(_sst_cfg_params[i]),
			      _sst_cfg_params[i],req);
      if (newval.empty()) {
	sst_cfg.eraseParameter(_sst_cfg_params[i]);
      } else{
	sst_cfg.setParameter(_sst_cfg_params[i],newval);
      }
    }
  }
}

bool SBCCallProfile::evaluate(ParamReplacerCtx& ctx,
			      const AmSipRequest& req)
{
  REPLACE_NONEMPTY_STR(ruri);
  REPLACE_NONEMPTY_STR(ruri_host);
  REPLACE_NONEMPTY_STR(from);
  REPLACE_NONEMPTY_STR(to);
  REPLACE_NONEMPTY_STR(callid);

  REPLACE_NONEMPTY_STR(dlg_contact_params);
  REPLACE_NONEMPTY_STR(bleg_dlg_contact_params);

  REPLACE_NONEMPTY_STR(outbound_proxy);
  REPLACE_NONEMPTY_STR(next_hop);

  if (!transcoder.evaluate(ctx,req)) return false;

  REPLACE_BOOL(rtprelay_enabled, rtprelay_enabled_value);

  if (rtprelay_enabled_value || transcoder.isActive()) {
    // evaluate other RTP relay related params only if enabled
    REPLACE_BOOL(force_symmetric_rtp, force_symmetric_rtp_value);
    REPLACE_BOOL(aleg_force_symmetric_rtp, aleg_force_symmetric_rtp_value);

    // enable symmetric RTP by P-MsgFlags?
    // SBC need not to know if it is from P-MsgFlags or from profile parameter
    if (msgflags_symmetric_rtp) {
      string str_msg_flags = getHeader(req.hdrs,"P-MsgFlags", true);
      unsigned int msg_flags = 0;
      if(reverse_hex2int(str_msg_flags,msg_flags)){
        ERROR("while parsing 'P-MsgFlags' header\n");
        msg_flags = 0;
      }
      if (msg_flags & FL_FORCE_ACTIVE) {
        DBG("P-MsgFlags indicates forced symmetric RTP (passive mode)");
        force_symmetric_rtp_value = true;
	aleg_force_symmetric_rtp_value = true;
      }
    }

    REPLACE_IFACE_RTP(rtprelay_interface, rtprelay_interface_value);
    REPLACE_IFACE_RTP(aleg_rtprelay_interface, aleg_rtprelay_interface_value);
  }

  REPLACE_BOOL(sst_enabled, sst_enabled_value);
  if (sst_enabled_value) {
    AmConfigReader& sst_cfg = sst_b_cfg;
    eval_sst_config(ctx,req,sst_cfg);
  }

  REPLACE_NONEMPTY_STR(append_headers);

  if (auth_enabled) {
    auth_credentials.user = ctx.replaceParameters(auth_credentials.user, 
						  "auth_user", req);
    auth_credentials.pwd = ctx.replaceParameters(auth_credentials.pwd, 
						 "auth_pwd", req);
  }
  
  if (auth_aleg_enabled) {
    auth_aleg_credentials.user = ctx.replaceParameters(auth_aleg_credentials.user,
						       "auth_aleg_user", req);
    auth_aleg_credentials.pwd = ctx.replaceParameters(auth_aleg_credentials.pwd, 
						      "auth_aleg_pwd", req);
  }

  if (uas_auth_bleg_enabled) {
    uas_auth_bleg_credentials.realm =
      ctx.replaceParameters(uas_auth_bleg_credentials.realm, "uas_auth_bleg_realm", req);
    uas_auth_bleg_credentials.user =
      ctx.replaceParameters(uas_auth_bleg_credentials.user, "uas_auth_bleg_user", req);
    uas_auth_bleg_credentials.pwd =
      ctx.replaceParameters(uas_auth_bleg_credentials.pwd, "uas_auth_bleg_pwd", req);
  }

  fix_replaces_inv = ctx.replaceParameters(fix_replaces_inv, "fix_replaces_inv", req);
  fix_replaces_ref = ctx.replaceParameters(fix_replaces_ref, "fix_replaces_ref", req);

  REPLACE_IFACE_SIP(outbound_interface, outbound_interface_value);

  if (!codec_prefs.evaluate(ctx,req)) return false;
  if (!hold_settings.evaluate(ctx,req)) return false;

  // TODO: activate filter if transcoder or codec_prefs is set?
/*  if ((!aleg_payload_order.empty() || !bleg_payload_order.empty()) && (!sdpfilter_enabled)) {
    sdpfilter_enabled = true;
    sdpfilter = Transparent;
  }*/

  return true;
}


bool SBCCallProfile::evaluateOutboundInterface() {
  if (outbound_interface == "default") {
    outbound_interface_value = 0;
  } else {
    map<string,unsigned short>::iterator name_it =
      AmConfig::SIP_If_names.find(outbound_interface);
    if (name_it != AmConfig::RTP_If_names.end()) {
      outbound_interface_value = name_it->second;
    } else {
      ERROR("selected outbound_interface '%s' does not exist as a signaling"
	    " interface. "
	    "Please check the 'additional_interfaces' "
	    "parameter in the main configuration file.",
	    outbound_interface.c_str());
      return false;
    }
  }

  return true;
}

bool SBCCallProfile::evaluateRTPRelayInterface() {
  EVALUATE_IFACE_RTP(rtprelay_interface, rtprelay_interface_value);
  return true;
}

bool SBCCallProfile::evaluateRTPRelayAlegInterface() {
  EVALUATE_IFACE_RTP(aleg_rtprelay_interface, aleg_rtprelay_interface_value);
  return true;
}

static int apply_outbound_interface(const string& oi, AmBasicSipDialog& dlg)
{
  if (oi == "default")
    dlg.setOutboundInterface(0);
  else {
    map<string,unsigned short>::iterator name_it = AmConfig::SIP_If_names.find(oi);
    if (name_it != AmConfig::SIP_If_names.end()) {
      dlg.setOutboundInterface(name_it->second);
    } else {
      ERROR("selected [aleg_]outbound_interface '%s' "
	    "does not exist as an interface. "
	    "Please check the 'additional_interfaces' "
	    "parameter in the main configuration file.",
	    oi.c_str());
      
      return -1;
    }
  }

  return 0;
}

int SBCCallProfile::apply_a_routing(ParamReplacerCtx& ctx,
				    const AmSipRequest& req,
				    AmBasicSipDialog& dlg) const
{
  if (!aleg_outbound_interface.empty()) {
    string aleg_oi =
      ctx.replaceParameters(aleg_outbound_interface, 
			    "aleg_outbound_interface", req);

    if(apply_outbound_interface(aleg_oi,dlg) < 0)
      return -1;
  }

  if (!aleg_next_hop.empty()) {

    string aleg_nh = ctx.replaceParameters(aleg_next_hop, 
					   "aleg_next_hop", req);

    DBG("set next hop ip to '%s'\n", aleg_nh.c_str());
    dlg.setNextHop(aleg_nh);
  }
  else {
    dlg.nat_handling = dlg_nat_handling;
    if(dlg_nat_handling && req.first_hop) {
      string nh = req.remote_ip + ":"
	+ int2str(req.remote_port)
	+ "/" + req.trsp;
      dlg.setNextHop(nh);
      dlg.setNextHop1stReq(false);
    }
  }

  if (!aleg_outbound_proxy.empty()) {
    string aleg_op = 
      ctx.replaceParameters(aleg_outbound_proxy, "aleg_outbound_proxy", req);
    dlg.outbound_proxy = aleg_op;
    dlg.force_outbound_proxy = aleg_force_outbound_proxy;
  }

  return 0;
}

int SBCCallProfile::apply_b_routing(ParamReplacerCtx& ctx,
				    const AmSipRequest& req,
				    AmBasicSipDialog& dlg) const
{
  if (!outbound_interface.empty()) {
    string oi = 
      ctx.replaceParameters(outbound_interface, "outbound_interface", req);

    if(apply_outbound_interface(oi,dlg) < 0)
      return -1;
  }

  if (!next_hop.empty()) {

    string nh = ctx.replaceParameters(next_hop, "next_hop", req);

    DBG("set next hop to '%s' (1st_req=%s,fixed=%s)\n",
	nh.c_str(), next_hop_1st_req?"true":"false",
	next_hop_fixed?"true":"false");
    dlg.setNextHop(nh);
    dlg.setNextHop1stReq(next_hop_1st_req);
    dlg.setNextHopFixed(next_hop_fixed);
  }

  DBG("patch_ruri_next_hop = %i",patch_ruri_next_hop);
  dlg.setPatchRURINextHop(patch_ruri_next_hop);

  if (!outbound_proxy.empty()) {
    string op = ctx.replaceParameters(outbound_proxy, "outbound_proxy", req);
    dlg.outbound_proxy = op;
    dlg.force_outbound_proxy = force_outbound_proxy;
  }

  return 0;
}

int SBCCallProfile::apply_common_fields(ParamReplacerCtx& ctx,
					AmSipRequest& req) const
{
  if(!ruri.empty()) {
    req.r_uri = ctx.replaceParameters(ruri, "RURI", req);
  }

  if (!ruri_host.empty()) {
    string ruri_h = ctx.replaceParameters(ruri_host, "RURI-host", req);

    ctx.ruri_parser.uri = req.r_uri;
    if (!ctx.ruri_parser.parse_uri()) {
      WARN("Error parsing R-URI '%s'\n", ctx.ruri_parser.uri.c_str());
      return -1;
    }
    else {
      ctx.ruri_parser.uri_port.clear();
      ctx.ruri_parser.uri_host = ruri_host;
      req.r_uri = ctx.ruri_parser.uri_str();
    }
  }

  if(!from.empty()) {
    req.from = ctx.replaceParameters(from, "From", req);
  }

  if(!to.empty()) {
    req.to = ctx.replaceParameters(to, "To", req);
  }

  if(!callid.empty()){
    req.callid = ctx.replaceParameters(callid, "Call-ID", req);
  }

  return 0;
}

void SBCCallProfile::replace_cc_values(ParamReplacerCtx& ctx,
				       const AmSipRequest& req,
				       AmArg* values)
{
  for (CCInterfaceListIteratorT cc_it = cc_interfaces.begin();
       cc_it != cc_interfaces.end(); cc_it++) {

    CCInterface& cc_if = *cc_it;
    
    DBG("processing replacements for call control interface '%s'\n", 
	cc_if.cc_name.c_str());

    for (map<string, string>::iterator it = cc_if.cc_values.begin(); 
	 it != cc_if.cc_values.end(); it++) {

      it->second = ctx.replaceParameters(it->second, it->first.c_str(), req);
      if(values) (*values)[it->first] = it->second;
    }
  }
}

int SBCCallProfile::refuse(ParamReplacerCtx& ctx, const AmSipRequest& req) const
{
  string m_refuse_with = ctx.replaceParameters(refuse_with, "refuse_with", req);
  if (m_refuse_with.empty()) {
    ERROR("refuse_with empty after replacing (was '%s' in profile %s)\n",
	  refuse_with.c_str(), profile_file.c_str());
    return -1;
  }

  size_t spos = m_refuse_with.find(' ');
  unsigned int refuse_with_code;
  if (spos == string::npos || spos == m_refuse_with.size() ||
      str2i(m_refuse_with.substr(0, spos), refuse_with_code)) {
    ERROR("invalid refuse_with '%s'->'%s' in  %s. Expected <code> <reason>\n",
	  refuse_with.c_str(), m_refuse_with.c_str(), profile_file.c_str());
    return -1;
  }

  string refuse_with_reason = m_refuse_with.substr(spos+1);
  string hdrs = ctx.replaceParameters(append_headers, "append_headers", req);
  //TODO: hdrs = remove_empty_headers(hdrs);
  if (hdrs.size()>2) assertEndCRLF(hdrs);

  DBG("refusing call with %u %s\n", refuse_with_code, refuse_with_reason.c_str());
  AmSipDialog::reply_error(req, refuse_with_code, refuse_with_reason, hdrs);

  return 0;
}

static void fixupCCInterface(const string& val, CCInterface& cc_if)
{
  DBG("instantiating CC interface from '%s'\n", val.c_str());
  size_t spos, last = val.length() - 1;
  if (val.length() == 0) {
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
	cc_if.cc_values.insert(make_pair(val.substr(spos+1, epos-spos-1), ""));

	DBG("    '%s'='%s'\n", 
	    val.substr(spos+1, epos-spos-1).c_str(), "");
	return;
      }
      size_t qpos = val.find('"', epos + 2);
      if (qpos == string::npos) {
	cc_if.cc_values.insert(make_pair(val.substr(spos+1, epos-spos-1), 
					 val.substr(epos + 2)));
	DBG("    '%s'='%s'\n", 
	    val.substr(spos+1, epos-spos-1).c_str(), 
	    val.substr(epos + 2).c_str());

	return;
      }
      cc_if.cc_values.insert(make_pair(val.substr(spos+1, epos-spos-1), 
				       val.substr(epos+2, qpos-epos-2)));
      DBG("    '%s'='%s'\n", 
	  val.substr(spos+1, epos-spos-1).c_str(), 
	  val.substr(epos+2, qpos-epos-2).c_str());

      if (qpos < last) {
	spos = val.find(";", qpos + 1);
      } else {
	return;
      }
    } else {
      size_t new_spos = val.find(";", epos + 1);
      if (new_spos == string::npos) {
	cc_if.cc_values.insert(make_pair(val.substr(spos+1, epos-spos-1), 
					 val.substr(epos+1)));

	DBG("    '%s'='%s'\n", 
	    val.substr(spos+1, epos-spos-1).c_str(), 
	    val.substr(epos+1).c_str());

	return;
      }

      cc_if.cc_values.insert(make_pair(val.substr(spos+1, epos-spos-1), 
				       val.substr(epos+1, new_spos-epos-1)));

      DBG("    '%s'='%s'\n", 
	  val.substr(spos+1, epos-spos-1).c_str(), 
	  val.substr(epos+1, new_spos-epos-1).c_str());

      spos = new_spos;
    }
  }
}

void SBCCallProfile::eval_cc_list(ParamReplacerCtx& ctx, const AmSipRequest& req)
{
  unsigned int cc_dynif_count = 0;

  // fix up replacements in cc list
  CCInterfaceListIteratorT cc_rit = cc_interfaces.begin();
  while (cc_rit != cc_interfaces.end()) {
    CCInterfaceListIteratorT curr_if = cc_rit;
    cc_rit++;
    //      CCInterfaceListIteratorT next_cc = cc_rit+1;
    if (curr_if->cc_name.find('$') != string::npos) {
      curr_if->cc_name = ctx.replaceParameters(curr_if->cc_name, 
					       "cc_interfaces", req);
      vector<string> dyn_ccinterfaces = explode(curr_if->cc_name, ",");
      if (!dyn_ccinterfaces.size()) {
	DBG("call_control '%s' did not produce any call control instances\n",
	    curr_if->cc_name.c_str());
	cc_interfaces.erase(curr_if);
      } else {
	// fill first CC interface (replacement item)
	vector<string>::iterator it = dyn_ccinterfaces.begin();
	curr_if->cc_name = "cc_dyn_"+int2str(cc_dynif_count++);
	fixupCCInterface(trim(*it, " \t"), *curr_if);
	it++;

	// insert other CC interfaces (in order!)
	while (it != dyn_ccinterfaces.end()) {
	  CCInterfaceListIteratorT new_cc =
	    cc_interfaces.insert(cc_rit, CCInterface());
	  fixupCCInterface(trim(*it, " \t"), *new_cc);
	  new_cc->cc_name = "cc_dyn_"+int2str(cc_dynif_count++);
	  it++;
	}
      }
    }
  }
}

/** removes headers with empty values from headers list separated by "\r\n" */
static string remove_empty_headers(const string& s)
{
  string res(s), hdr;
  size_t start = 0, end = 0, len = 0, col = 0;
  DBG("SBCCallProfile::remove_empty_headers '%s'", s.c_str());

  if (res.empty())
    return res;

  do {
    end = res.find_first_of("\n", start);
    len = (end == string::npos ? res.size() - start : end - start + 1);
    hdr = res.substr(start, len);
    col = hdr.find_first_of(':');

    if (col && hdr.find_first_not_of(": \r\n", col) == string::npos) {
      // remove empty header
      WARN("Ignored empty header: %s\n", res.substr(start, len).c_str());
      res.erase(start, len);
      // start remains the same
    }
    else {
      if (string::npos == col)
        WARN("Malformed append header: %s\n", hdr.c_str());
      start = end + 1;
    }
  } while (end != string::npos && start < res.size());

  return res;
}

static void fix_append_hdr_list(const AmSipRequest& req, ParamReplacerCtx& ctx,
				string& append_hdr, const char* field_name)
{
  append_hdr = ctx.replaceParameters(append_hdr, field_name, req);
  append_hdr = remove_empty_headers(append_hdr);
  if (append_hdr.size()>2) assertEndCRLF(append_hdr);
}

void SBCCallProfile::fix_append_hdrs(ParamReplacerCtx& ctx,
				     const AmSipRequest& req)
{
  fix_append_hdr_list(req, ctx, append_headers, "append_headers");
  fix_append_hdr_list(req, ctx, append_headers_req,"append_headers_req");
  fix_append_hdr_list(req, ctx, aleg_append_headers_req,"aleg_append_headers_req");
}


void SBCCallProfile::fix_reg_contact(ParamReplacerCtx& ctx,
				     const AmSipRequest& req,
				     AmUriParser& contact) const
{
  string user = contact.uri_user;
  string host = contact.uri_host;
  string port = contact.uri_port;

  if (!this->contact.displayname.empty()) {
    contact.display_name = 
      ctx.replaceParameters(this->contact.displayname, "Contact DN", req);
  }
  if (!this->contact.user.empty()) {
    contact.uri_user = 
      ctx.replaceParameters(this->contact.user, "Contact User", req);
  }
  if (!this->contact.host.empty()) {
    contact.uri_host = 
      ctx.replaceParameters(this->contact.host, "Contact host", req);
  }
  if (!this->contact.port.empty()) {
    contact.uri_port =
      ctx.replaceParameters(this->contact.port, "Contact port", req);
  }
}

string SBCCallProfile::retarget(const string& alias)
{
    // REG-Cache lookup
    AliasEntry alias_entry;
    if(!RegisterCache::instance()->findAliasEntry(alias, alias_entry)) {
      throw AmSession::Exception(404,"User not found");
    }
    string new_r_uri = alias_entry.contact_uri;
    DBG("setting from registration cache: r_uri='%s'\n",new_r_uri.c_str());

    // fix NAT
    string nh = alias_entry.source_ip;
    if(alias_entry.source_port != 5060)
      nh += ":" + int2str(alias_entry.source_port);

    DBG("setting from registration cache: next_hop='%s'\n", nh.c_str());
    next_hop = nh;

    // sticky interface
    DBG("setting from registration cache: outbound_interface='%s'\n",
	AmConfig::SIP_Ifs[alias_entry.local_if].name.c_str());
    outbound_interface = AmConfig::SIP_Ifs[alias_entry.local_if].name;
    outbound_interface_value = alias_entry.local_if;

    return new_r_uri;
}

string SBCCallProfile::retarget(const string& alias, AmBasicSipDialog& dlg) const
{
    // REG-Cache lookup
    AliasEntry alias_entry;
    if(!RegisterCache::instance()->findAliasEntry(alias, alias_entry)) {
      throw AmSession::Exception(404,"User not found");
    }
    string new_r_uri = alias_entry.contact_uri;
    DBG("setting from registration cache: r_uri='%s'\n",new_r_uri.c_str());

    // fix NAT
    string nh = alias_entry.source_ip;
    if(alias_entry.source_port != 5060)
      nh += ":" + int2str(alias_entry.source_port);

    DBG("setting from registration cache: next_hop='%s'\n", nh.c_str());
    dlg.setNextHop(nh);

    // sticky interface
    DBG("setting from registration cache: outbound_interface='%s'\n",
	AmConfig::SIP_Ifs[alias_entry.local_if].name.c_str());
    dlg.setOutboundInterface(alias_entry.local_if);

    return new_r_uri;
}

static bool readPayloadList(std::vector<PayloadDesc> &dst, const std::string &src)
{
  dst.clear();
  vector<string> elems = explode(src, ",");
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); ++it) {
    PayloadDesc payload;
    if (!payload.read(*it)) return false;
    dst.push_back(payload);
  }
  return true;
}

static bool readPayload(SdpPayload &p, const string &src)
{
  vector<string> elems = explode(src, "/");

  if (elems.size() < 1) return false;

  if (elems.size() > 2) str2int(elems[1], p.encoding_param);
  if (elems.size() > 1) str2int(elems[1], p.clock_rate);
  else p.clock_rate = 8000; // default value
  p.encoding_name = elems[0];
  
  string pname = p.encoding_name;
  transform(pname.begin(), pname.end(), pname.begin(), ::tolower);

  // fix static payload type numbers
  // (http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xml)
  for (int i = 0; i < IANA_RTP_PAYLOADS_SIZE; i++) {
    string s = IANA_RTP_PAYLOADS[i].payload_name;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (p.encoding_name == s && 
        (unsigned)p.clock_rate == IANA_RTP_PAYLOADS[i].clock_rate && 
        (p.encoding_param == -1 || ((unsigned)p.encoding_param == IANA_RTP_PAYLOADS[i].channels))) 
      p.payload_type = i;
  }

  return true;
}

static string payload2str(const SdpPayload &p)
{
  string s(p.encoding_name);
  s += "/";
  s += int2str(p.clock_rate);
  return s;
}

static bool read(const std::string &src, vector<SdpPayload> &codecs)
{
  vector<string> elems = explode(src, ",");

  AmPlugIn* plugin = AmPlugIn::instance();

  for (vector<string>::iterator it=elems.begin(); it != elems.end(); ++it) {
    SdpPayload p;
    if (!readPayload(p, *it)) return false;
    int payload_id = plugin->getDynPayload(p.encoding_name, p.clock_rate, 0);
    amci_payload_t* payload = plugin->payload(payload_id);
    if(!payload) {
      ERROR("Ignoring unknown payload found in call profile: %s/%i\n",
	    p.encoding_name.c_str(), p.clock_rate);
    }
    else {
      if(payload_id < DYNAMIC_PAYLOAD_TYPE_START)
	p.payload_type = payload->payload_id;
      else
	p.payload_type = -1;

      codecs.push_back(p);
    }
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////////

void SBCCallProfile::CodecPreferences::orderSDP(AmSdp& sdp, bool a_leg)
{
  // get order of payloads for the other leg!
  std::vector<PayloadDesc> &payload_order = a_leg ? bleg_payload_order: aleg_payload_order;

  if (payload_order.size() < 1) return; // nothing to do - no predefined order

  DBG("ordering SDP\n");
  for (vector<SdpMedia>::iterator m_it =
	 sdp.media.begin(); m_it != sdp.media.end(); ++m_it) {
    SdpMedia& media = *m_it;

    unsigned pos = 0;
    unsigned idx;
    unsigned cnt = media.payloads.size();

    // TODO: optimize
    // for each predefined payloads in their order
    for (vector<PayloadDesc>::iterator i = payload_order.begin(); i != payload_order.end(); ++i) {
      // try to find this payload in SDP 
      // (not needed to go through already sorted members)
      for (idx = pos; idx < cnt; idx++) {
        if (i->match(media.payloads[idx])) {
          // found, insert found element at pos and delete the occurence on idx
          // (can not swap these elements to avoid changing order of codecs
          // which are not in the payload_order list)
          if (idx != pos) {
            media.payloads.insert(media.payloads.begin() + pos, media.payloads[idx]);
            media.payloads.erase(media.payloads.begin() + idx + 1);
          }
	
	  ++pos; // next payload index
          // do not terminate the inner loop because one PayloadDesc can match
          // more payloads!
	}
      }
    }
  }
}

bool SBCCallProfile::CodecPreferences::shouldOrderPayloads(bool a_leg)
{
  // returns true if order of payloads for the other leg is set! (i.e. if we
  // have to order payloads)
  if (a_leg) return !bleg_payload_order.empty();
  else return !aleg_payload_order.empty();
}

bool SBCCallProfile::CodecPreferences::readConfig(AmConfigReader &cfg)
{
  // store string values for later evaluation
  bleg_payload_order_str = cfg.getParameter("codec_preference");
  bleg_prefer_existing_payloads_str = cfg.getParameter("prefer_existing_codecs");
  
  aleg_payload_order_str = cfg.getParameter("codec_preference_aleg");
  aleg_prefer_existing_payloads_str = cfg.getParameter("prefer_existing_codecs_aleg");

  return true;
}

void SBCCallProfile::CodecPreferences::infoPrint() const
{
  INFO("SBC:      A leg codec preference: %s\n", aleg_payload_order_str.c_str());
  INFO("SBC:      A leg prefer existing codecs: %s\n", aleg_prefer_existing_payloads_str.c_str());
  INFO("SBC:      B leg codec preference: %s\n", bleg_payload_order_str.c_str());
  INFO("SBC:      B leg prefer existing codecs: %s\n", bleg_prefer_existing_payloads_str.c_str());
}

bool SBCCallProfile::CodecPreferences::operator==(const CodecPreferences& rhs) const
{
  if (!payloadDescsEqual(aleg_payload_order, rhs.aleg_payload_order)) return false;
  if (!payloadDescsEqual(bleg_payload_order, rhs.bleg_payload_order)) return false;
  if (aleg_prefer_existing_payloads != rhs.aleg_prefer_existing_payloads) return false;
  if (bleg_prefer_existing_payloads != rhs.bleg_prefer_existing_payloads) return false;
  return true;
}

string SBCCallProfile::CodecPreferences::print() const
{
  string res;

  res += "codec_preference: ";
  for (vector<PayloadDesc>::const_iterator i = bleg_payload_order.begin(); i != bleg_payload_order.end(); ++i) {
    if (i != bleg_payload_order.begin()) res += ",";
    res += i->print();
  }
  res += "\n";
  
  res += "prefer_existing_codecs: ";
  if (bleg_prefer_existing_payloads) res += "yes\n"; 
  else res += "no\n";

  res += "codec_preference_aleg:    ";
  for (vector<PayloadDesc>::const_iterator i = aleg_payload_order.begin(); i != aleg_payload_order.end(); ++i) {
    if (i != aleg_payload_order.begin()) res += ",";
    res += i->print();
  }
  res += "\n";
  
  res += "prefer_existing_codecs_aleg: ";
  if (aleg_prefer_existing_payloads) res += "yes\n"; 
  else res += "no\n";

  return res;
}

bool SBCCallProfile::CodecPreferences::evaluate(ParamReplacerCtx& ctx,
						const AmSipRequest& req)
{
  REPLACE_BOOL(aleg_prefer_existing_payloads_str, aleg_prefer_existing_payloads);
  REPLACE_BOOL(bleg_prefer_existing_payloads_str, bleg_prefer_existing_payloads);
  
  REPLACE_NONEMPTY_STR(aleg_payload_order_str);
  REPLACE_NONEMPTY_STR(bleg_payload_order_str);

  if (!readPayloadList(bleg_payload_order, bleg_payload_order_str)) return false;
  if (!readPayloadList(aleg_payload_order, aleg_payload_order_str)) return false;

  return true;
}

//////////////////////////////////////////////////////////////////////////////////

bool SBCCallProfile::TranscoderSettings::readTranscoderMode(const std::string &src)
{
  static const string always("always");
  static const string never("never");
  static const string on_missing_compatible("on_missing_compatible");

  if (src == always) { transcoder_mode = Always; return true; }
  if (src == never) { transcoder_mode = Never; return true; }
  if (src == on_missing_compatible) { transcoder_mode = OnMissingCompatible; return true; }
  if (src.empty()) { transcoder_mode = Never; return true; } // like default value
  ERROR("unknown value of enable_transcoder option: %s\n", src.c_str());

  return false;
}

bool SBCCallProfile::TranscoderSettings::readDTMFMode(const std::string &src)
{
  static const string always("always");
  static const string never("never");
  static const string lowfi_codec("lowfi_codec");

  if (src == always) { dtmf_mode = DTMFAlways; return true; }
  if (src == never) { dtmf_mode = DTMFNever; return true; }
  if (src == lowfi_codec) { dtmf_mode = DTMFLowFiCodecs; return true; }
  if (src.empty()) { dtmf_mode = DTMFNever; return true; } // like default value
  ERROR("unknown value of dtmf_transcoding_mode option: %s\n", src.c_str());

  return false;
}

void SBCCallProfile::TranscoderSettings::infoPrint() const
{
  INFO("SBC:      transcoder audio codecs: %s\n", audio_codecs_str.c_str());
  INFO("SBC:      callee codec capabilities: %s\n", callee_codec_capabilities_str.c_str());
  INFO("SBC:      enable transcoder: %s\n", transcoder_mode_str.c_str());
  INFO("SBC:      norelay audio codecs: %s\n", audio_codecs_norelay_str.c_str());
  INFO("SBC:      norelay audio codecs (aleg): %s\n", audio_codecs_norelay_aleg_str.c_str());
}

bool SBCCallProfile::TranscoderSettings::readConfig(AmConfigReader &cfg)
{
  // store string values for later evaluation
  audio_codecs_str = cfg.getParameter("transcoder_codecs");
  callee_codec_capabilities_str = cfg.getParameter("callee_codeccaps");
  transcoder_mode_str = cfg.getParameter("enable_transcoder");
  dtmf_mode_str = cfg.getParameter("dtmf_transcoding");
  lowfi_codecs_str = cfg.getParameter("lowfi_codecs");
  audio_codecs_norelay_str = cfg.getParameter("prefer_transcoding_for_codecs");
  audio_codecs_norelay_aleg_str = cfg.getParameter("prefer_transcoding_for_codecs_aleg");

  return true;
}

bool SBCCallProfile::TranscoderSettings::operator==(const TranscoderSettings& rhs) const
{
  bool res = (transcoder_mode == rhs.transcoder_mode);
  res = res && (enabled == rhs.enabled);
  res = res && (payloadDescsEqual(callee_codec_capabilities, rhs.callee_codec_capabilities));
  res = res && (audio_codecs == rhs.audio_codecs);
  return res;
}

string SBCCallProfile::TranscoderSettings::print() const
{
  string res("transcoder audio codecs:");
  for (vector<SdpPayload>::const_iterator i = audio_codecs.begin(); i != audio_codecs.end(); ++i) {
    res += " ";
    res += payload2str(*i);
  }

  res += "\ncallee codec capabilities:";
  for (vector<PayloadDesc>::const_iterator i = callee_codec_capabilities.begin(); 
      i != callee_codec_capabilities.end(); ++i)
  {
    res += " ";
    res += i->print();
  }

  string s("?");
  switch (transcoder_mode) {
    case Always: s = "always"; break;
    case Never: s = "never"; break;
    case OnMissingCompatible: s = "on_missing_compatible"; break;
  }
  res += "\nenable transcoder: " + s;
  
  res += "\ntranscoder currently enabled: ";
  if (enabled) res += "yes\n";
  else res += "no\n";
  
  return res;
}
  
bool SBCCallProfile::TranscoderSettings::evaluate(ParamReplacerCtx& ctx,
						  const AmSipRequest& req)
{
  REPLACE_NONEMPTY_STR(transcoder_mode_str);
  REPLACE_NONEMPTY_STR(audio_codecs_str);
  REPLACE_NONEMPTY_STR(audio_codecs_norelay_str);
  REPLACE_NONEMPTY_STR(audio_codecs_norelay_aleg_str);
  REPLACE_NONEMPTY_STR(callee_codec_capabilities_str);
  REPLACE_NONEMPTY_STR(lowfi_codecs_str);  

  if (!read(audio_codecs_str, audio_codecs)) return false;
  if (!read(audio_codecs_norelay_str, audio_codecs_norelay)) return false;
  if (!read(audio_codecs_norelay_aleg_str, audio_codecs_norelay_aleg)) return false;

  if (!readPayloadList(callee_codec_capabilities, callee_codec_capabilities_str)) 
    return false;
  
  if (!readTranscoderMode(transcoder_mode_str)) return false;

  if (!readDTMFMode(dtmf_mode_str)) return false;

  if (!read(lowfi_codecs_str, lowfi_codecs)) return false;

  // enable transcoder according to transcoder mode and optionally request's SDP
  switch (transcoder_mode) {
    case Always: enabled = true; break;
    case Never: enabled = false; break;
    case OnMissingCompatible: 
      enabled = isTranscoderNeeded(req, callee_codec_capabilities, 
                                 true /* if SDP can't be analyzed, enable transcoder */); 
      break;
  }

  DBG("transcoder is %s\n", enabled ? "enabled": "disabled");

  if (enabled && audio_codecs.empty()) {
    ERROR("transcoder is enabled but no transcoder codecs selected ... disabling it\n");
    enabled = false;
  }

  return true;
}

void SBCCallProfile::create_logger(const AmSipRequest& req)
{
  if (msg_logger_path.empty()) return;

  ParamReplacerCtx ctx(this);
  string log_path = ctx.replaceParameters(msg_logger_path, "msg_logger_path", req);
  if (log_path.empty()) return;

  file_msg_logger *log = new pcap_logger();

  if(log->open(log_path.c_str()) != 0) {
    // open error
    delete log;
    return;
  }

  // opened successfully
  logger.reset(log);
}

msg_logger* SBCCallProfile::get_logger(const AmSipRequest& req)
{
  if (!logger.get() && !msg_logger_path.empty()) create_logger(req);
  return logger.get();
}

//////////////////////////////////////////////////////////////////////////////////

bool PayloadDesc::match(const SdpPayload &p) const
{
  string enc_name = p.encoding_name;
  transform(enc_name.begin(), enc_name.end(), enc_name.begin(), ::tolower);
      
  if ((name.size() > 0) && (name != enc_name)) return false;
  if (clock_rate && (p.clock_rate > 0) && clock_rate != (unsigned)p.clock_rate) return false;
  return true;
}

bool PayloadDesc::read(const std::string &s)
{
  vector<string> elems = explode(s, "/");
  if (elems.size() > 1) {
    name = elems[0];
    str2i(elems[1], clock_rate);
  }
  else if (elems.size() > 0) {
    name = elems[0];
    clock_rate = 0;
  }
  transform(name.begin(), name.end(), name.begin(), ::tolower);
  return true;
}

string PayloadDesc::print() const
{
    std::string s(name); 
    s += " / "; 
    if (!clock_rate) s += "whatever rate";
    else s += int2str(clock_rate); 
    return s; 
}
    
bool PayloadDesc::operator==(const PayloadDesc &other) const
{
  if (name != other.name) return false;
  if (clock_rate != other.clock_rate) return false;
  return true;
}

//////////////////////////////////////////////////////////////////////////////////

void SBCCallProfile::HoldSettings::readConfig(AmConfigReader &cfg)
{
  // store string values for later evaluation
  aleg.mark_zero_connection_str = cfg.getParameter("hold_zero_connection_aleg");
  aleg.activity_str = cfg.getParameter("hold_activity_aleg");
  aleg.alter_b2b_str = cfg.getParameter("hold_alter_b2b_aleg");

  bleg.mark_zero_connection_str = cfg.getParameter("hold_zero_connection_bleg");
  bleg.activity_str = cfg.getParameter("hold_activity_bleg");
  bleg.alter_b2b_str = cfg.getParameter("hold_alter_b2b_bleg");
}

bool SBCCallProfile::HoldSettings::HoldParams::setActivity(const string &s)
{
  if (s == "sendrecv") activity = sendrecv;
  else if (s == "sendonly") activity = sendonly;
  else if (s == "recvonly") activity = recvonly;
  else if (s == "inactive") activity = inactive;
  else {
    ERROR("unsupported hold stream activity: %s\n", s.c_str());
    return false;
  }

  return true;
}

bool SBCCallProfile::HoldSettings::evaluate(ParamReplacerCtx& ctx, const AmSipRequest& req)
{
  REPLACE_BOOL(aleg.mark_zero_connection_str, aleg.mark_zero_connection);
  REPLACE_STR(aleg.activity_str);
  REPLACE_BOOL(aleg.alter_b2b_str, aleg.alter_b2b);

  REPLACE_BOOL(bleg.mark_zero_connection_str, bleg.mark_zero_connection);
  REPLACE_STR(bleg.activity_str);
  REPLACE_BOOL(bleg.alter_b2b_str, bleg.alter_b2b);

  if (!aleg.activity_str.empty() && !aleg.setActivity(aleg.activity_str)) return false;
  if (!bleg.activity_str.empty() && !bleg.setActivity(bleg.activity_str)) return false;

  return true;
}
