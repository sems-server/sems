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

#include "ampi/SBCCallControlAPI.h"
#include "RTPParameters.h"
#include "SDPFilter.h"

typedef vector<SdpPayload>::iterator PayloadIterator;
static string payload2str(const SdpPayload &p);


//////////////////////////////////////////////////////////////////////////////////
// helper defines for parameter evaluation

#define REPLACE_VALS req, app_param, ruri_parser, from_parser, to_parser

// FIXME: r_type in replaceParameters is just for debug output?

#define REPLACE_STR(what) do { \
  what = replaceParameters(what, #what, REPLACE_VALS); \
  DBG(#what " = '%s'\n", what.c_str()); \
} while(0)

#define REPLACE_NONEMPTY_STR(what) do { \
  if (!what.empty()) { \
    REPLACE_STR(what); \
  } \
} while(0)

#define REPLACE_NUM(what, dst_num) do { \
  if (!what.empty()) { \
    what = replaceParameters(what, #what, REPLACE_VALS); \
    unsigned int num; \
    if (str2i(what, num)) { \
      ERROR(#what " '%s' not understood\n", what.c_str()); \
      return false; \
    } \
    DBG(#what " = '%s'\n", what.c_str()); \
    dst_num = num; \
  } \
} while(0)

#define REPLACE_BOOL(what, dst_value) do { \
  if (!what.empty()) { \
    what = replaceParameters(what, #what, REPLACE_VALS); \
    if (!what.empty()) { \
      if (!str2bool(what, dst_value)) { \
      ERROR(#what " '%s' not understood\n", what.c_str()); \
        return false; \
      } \
    } \
    DBG(#what " = '%s'\n", dst_value ? "yes" : "no"); \
  } \
} while(0)

#define REPLACE_IFACE(what, iface) do { \
  if (!what.empty()) { \
    what = replaceParameters(what, #what, REPLACE_VALS); \
    DBG("set " #what " to '%s'\n", what.c_str()); \
    if (!what.empty()) { \
      if (what == "default") iface = 0; \
      else { \
        map<string,unsigned short>::iterator name_it = AmConfig::If_names.find(what); \
        if (name_it != AmConfig::If_names.end()) iface = name_it->second; \
        else { \
          ERROR("selected " #what " '%s' does not exist as an interface. " \
              "Please check the 'additional_interfaces' " \
              "parameter in the main configuration file.", \
              what.c_str()); \
          return false; \
        } \
      } \
    } \
  } \
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
  contact = cfg.getParameter("Contact");

  callid = cfg.getParameter("Call-ID");

  force_outbound_proxy = cfg.getParameter("force_outbound_proxy") == "yes";
  outbound_proxy = cfg.getParameter("outbound_proxy");

  next_hop = cfg.getParameter("next_hop");
  next_hop_for_replies = cfg.getParameter("next_hop_for_replies");

  if (cfg.hasParameter("header_filter")) {
    string hf_type = cfg.getParameter("header_filter");
    headerfilter = String2FilterType(hf_type.c_str());
    if (Undefined == headerfilter) {
      ERROR("invalid header_filter mode '%s'\n", hf_type.c_str());
      return false;
    }
  }
  
  vector<string> elems = explode(cfg.getParameter("header_list"), ",");
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++) {
    transform(it->begin(), it->end(), it->begin(), ::tolower);
    headerfilter_list.insert(*it);
  }

  if (cfg.hasParameter("message_filter")) {
    string mf_type = cfg.getParameter("message_filter");
    messagefilter = String2FilterType(mf_type.c_str());
    if (messagefilter == Undefined) {
      ERROR("invalid message_filter mode '%s'\n", mf_type.c_str());
      return false;
    }
  }

  elems = explode(cfg.getParameter("message_list"), ",");
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++)
    messagefilter_list.insert(*it);

  string sdp_filter = cfg.getParameter("sdp_filter");
  sdpfilter = String2FilterType(sdp_filter.c_str());
  sdpfilter_enabled = sdpfilter != Undefined;

  if (sdpfilter_enabled) {
    vector<string> c_elems = explode(cfg.getParameter("sdpfilter_list"), ",");
    for (vector<string>::iterator it=c_elems.begin(); it != c_elems.end(); it++) {
      string c = *it;
      std::transform(c.begin(), c.end(), c.begin(), ::tolower);
      sdpfilter_list.insert(c);
    }
    anonymize_sdp = cfg.getParameter("sdp_anonymize", "no") == "yes";
  }

  string cfg_sdp_alines_filter = cfg.getParameter("sdp_alines_filter", "transparent");
  sdpalinesfilter = String2FilterType(cfg_sdp_alines_filter.c_str());
  sdpalinesfilter_enabled =
    (sdpalinesfilter!=Undefined) && (sdpalinesfilter!=Transparent);

  if (sdpalinesfilter_enabled) {
    vector<string> c_elems = explode(cfg.getParameter("sdp_alinesfilter_list"), ",");
    for (vector<string>::iterator it=c_elems.begin(); it != c_elems.end(); it++) {
      string c = *it;
      std::transform(c.begin(), c.end(), c.begin(), ::tolower);
      sdpalinesfilter_list.insert(c);
    }
  }

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
    } else if (SBCFactory::cfg.hasParameter(cfgkey)) {			\
      dstcfg.setParameter(cfgkey, SBCFactory::cfg.getParameter(cfgkey)); \
    }

  if (sst_enabled != "no") {
    if (NULL == SBCFactory::session_timer_fact) {
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

  if (sst_aleg_enabled != "no") {
    sst_a_cfg.setParameter("enable_session_timer", "yes");
    // create sst_a_cfg superimposing values from aleg_*
    CP_SST_CFGVAR("aleg_", "session_expires", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "minimum_timer", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "maximum_timer", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "session_refresh_method", sst_a_cfg);
    CP_SST_CFGVAR("aleg_", "accept_501_reply", sst_a_cfg);
  }
#undef CP_SST_CFGVAR
  
  auth_enabled = cfg.getParameter("enable_auth", "no") == "yes";
  auth_credentials.user = cfg.getParameter("auth_user");
  auth_credentials.pwd = cfg.getParameter("auth_pwd");

  auth_aleg_enabled = cfg.getParameter("enable_aleg_auth", "no") == "yes";
  auth_aleg_credentials.user = cfg.getParameter("auth_aleg_user");
  auth_aleg_credentials.pwd = cfg.getParameter("auth_aleg_pwd");

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

  refuse_with = cfg.getParameter("refuse_with");

  rtprelay_enabled = cfg.getParameter("enable_rtprelay") == "yes";
  force_symmetric_rtp = cfg.getParameter("rtprelay_force_symmetric_rtp");
  msgflags_symmetric_rtp = cfg.getParameter("rtprelay_msgflags_symmetric_rtp") == "yes";

  rtprelay_interface = cfg.getParameter("rtprelay_interface");
  aleg_rtprelay_interface = cfg.getParameter("aleg_rtprelay_interface");

  rtprelay_transparent_seqno =
    cfg.getParameter("rtprelay_transparent_seqno", "yes") == "yes";
  rtprelay_transparent_ssrc =
    cfg.getParameter("rtprelay_transparent_ssrc", "yes") == "yes";

  outbound_interface = cfg.getParameter("outbound_interface");

  if (!codec_prefs.readConfig(cfg)) return false;
  if (!transcoder.readConfig(cfg)) return false;

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
    if (!contact.empty()) {
      INFO("SBC:      Contact   = '%s'\n", contact.c_str());
    }
    if (!callid.empty()) {
      INFO("SBC:      Call-ID   = '%s'\n", callid.c_str());
    }

    INFO("SBC:      force outbound proxy: %s\n", force_outbound_proxy?"yes":"no");
    INFO("SBC:      outbound proxy = '%s'\n", outbound_proxy.c_str());
    if (!next_hop.empty()) {
      INFO("SBC:      next hop = %s\n", next_hop.c_str());

      if (!next_hop_for_replies.empty()) {
	INFO("SBC:      next hop used for replies: '%s'\n", next_hop_for_replies.c_str());
      }
    }

    INFO("SBC:      header filter  is %s, %zd items in list\n",
	 isActiveFilter(headerfilter) ? FilterType2String(headerfilter) : "disabled",
	 headerfilter_list.size());
    INFO("SBC:      message filter is %s, %zd items in list\n",
	 isActiveFilter(messagefilter) ? FilterType2String(messagefilter) : "disabled",
	 messagefilter_list.size());
    INFO("SBC:      SDP filter is %sabled, %s, %zd items in list, %sanonymizing SDP\n",
	 sdpfilter_enabled?"en":"dis",
	 isActiveFilter(sdpfilter) ? FilterType2String(sdpfilter) : "inactive",
	 sdpfilter_list.size(), anonymize_sdp?"":"not ");
    INFO("SBC:      SDP alines-filter is %sabled, %s, %zd items in list\n",
	 sdpalinesfilter_enabled?"en":"dis",
	 isActiveFilter(sdpalinesfilter) ? FilterType2String(sdpalinesfilter) : "inactive",
	 sdpalinesfilter_list.size());

    INFO("SBC:      RTP relay %sabled\n", rtprelay_enabled?"en":"dis");
    if (rtprelay_enabled) {
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
    }

    INFO("SBC:      SST on A leg enabled: '%s'\n", sst_aleg_enabled.empty() ?
	 "no" : sst_aleg_enabled.c_str());
    if (sst_aleg_enabled != "no") {
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
    if (sst_enabled != "no") {
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
    contact == rhs.contact &&
    callid == rhs.callid &&
    outbound_proxy == rhs.outbound_proxy &&
    force_outbound_proxy == rhs.force_outbound_proxy &&
    next_hop == rhs.next_hop &&
    next_hop_for_replies == rhs.next_hop_for_replies &&
    headerfilter == rhs.headerfilter &&
    headerfilter_list == rhs.headerfilter_list &&
    messagefilter == rhs.messagefilter &&
    messagefilter_list == rhs.messagefilter_list &&
    sdpfilter_enabled == rhs.sdpfilter_enabled &&
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

  if (sdpfilter_enabled) {
    res = res &&
      sdpfilter == rhs.sdpfilter &&
      sdpfilter_list == rhs.sdpfilter_list;
  }
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
  res += "contact:              " + contact + "\n";
  res += "callid:               " + callid + "\n";
  res += "outbound_proxy:       " + outbound_proxy + "\n";
  res += "force_outbound_proxy: " + string(force_outbound_proxy?"true":"false") + "\n";
  res += "next_hop:             " + next_hop + "\n";
  res += "next_hop_for_replies: " + next_hop_for_replies + "\n";
  res += "headerfilter:         " + string(FilterType2String(headerfilter)) + "\n";
  res += "headerfilter_list:    " + stringset_print(headerfilter_list) + "\n";
  res += "messagefilter:        " + string(FilterType2String(messagefilter)) + "\n";
  res += "messagefilter_list:   " + stringset_print(messagefilter_list) + "\n";
  res += "sdpfilter_enabled:    " + string(sdpfilter_enabled?"true":"false") + "\n";
  res += "sdpfilter:            " + string(FilterType2String(sdpfilter)) + "\n";
  res += "sdpfilter_list:       " + stringset_print(sdpfilter_list) + "\n";
  res += "sdpalinesfilter:      " + string(FilterType2String(sdpalinesfilter)) + "\n";
  res += "sdpalinesfilter_list: " + stringset_print(sdpalinesfilter_list) + "\n";
  res += "sst_enabled:          " + sst_enabled + "\n";
  res += "sst_aleg_enabled:     " + sst_aleg_enabled + "\n";
  res += "auth_enabled:         " + string(auth_enabled?"true":"false") + "\n";
  res += "auth_user:            " + auth_credentials.user+"\n";
  res += "auth_pwd:             " + auth_credentials.pwd+"\n";
  res += "auth_aleg_enabled:    " + string(auth_aleg_enabled?"true":"false") + "\n";
  res += "auth_aleg_user:       " + auth_aleg_credentials.user+"\n";
  res += "auth_aleg_pwd:        " + auth_aleg_credentials.pwd+"\n";
  res += "rtprelay_enabled:     " + string(rtprelay_enabled?"true":"false") + "\n";
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

/* translates string value into bool, returns false on error */
static bool str2bool(const string &s, bool &dst)
{
  // TODO: optimize
  if ((s == "yes") || (s == "true") || (s == "1")) {
    dst = true;
    return true;
  }
  if ((s == "no") || (s == "false") || (s == "0")) {
    dst = false;
    return true;
  }
  return false;
}

static bool isTranscoderNeeded(const AmSipRequest& req, vector<PayloadDesc> &caps, bool default_value)
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
  normalizeSDP(sdp, false);

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

bool SBCCallProfile::evaluate(const AmSipRequest& req,
    const string& app_param,
    AmUriParser& ruri_parser, AmUriParser& from_parser,
    AmUriParser& to_parser)
{
  REPLACE_NONEMPTY_STR(ruri);
  REPLACE_NONEMPTY_STR(ruri_host);
  REPLACE_NONEMPTY_STR(from);
  REPLACE_NONEMPTY_STR(to);
  REPLACE_NONEMPTY_STR(contact);
  REPLACE_NONEMPTY_STR(callid);

  REPLACE_NONEMPTY_STR(outbound_proxy);

  if (!next_hop.empty()) {
    REPLACE_STR(next_hop);
    REPLACE_NONEMPTY_STR(next_hop_for_replies);
  }

  if (rtprelay_enabled) {
    // evaluate other RTP relay related params only if enabled
    // FIXME: really not evaluate rtprelay_enabled itself?
    REPLACE_NONEMPTY_STR(force_symmetric_rtp);
    REPLACE_BOOL(force_symmetric_rtp, force_symmetric_rtp_value);

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
      }
    }

    REPLACE_IFACE(rtprelay_interface, rtprelay_interface_value);
    REPLACE_IFACE(aleg_rtprelay_interface, aleg_rtprelay_interface_value);
  }

  REPLACE_BOOL(sst_enabled, sst_enabled_value);
  if (sst_enabled_value) {
    AmConfigReader& sst_cfg = sst_b_cfg;
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

  REPLACE_NONEMPTY_STR(append_headers);

  if (auth_enabled) {
    auth_credentials.user = replaceParameters(auth_credentials.user, "auth_user", REPLACE_VALS);
    auth_credentials.pwd = replaceParameters(auth_credentials.pwd, "auth_pwd", REPLACE_VALS);
  }
  
  if (auth_aleg_enabled) {
    auth_aleg_credentials.user = replaceParameters(auth_aleg_credentials.user, "auth_aleg_user", REPLACE_VALS);
    auth_aleg_credentials.pwd = replaceParameters(auth_aleg_credentials.pwd, "auth_aleg_pwd", REPLACE_VALS);
  }

  REPLACE_IFACE(outbound_interface, outbound_interface_value);

  if (!transcoder.evaluate(REPLACE_VALS)) return false;
  if (!codec_prefs.evaluate(REPLACE_VALS)) return false;

  // TODO: activate filter if transcoder or codec_prefs is set?
/*  if ((!aleg_payload_order.empty() || !bleg_payload_order.empty()) && (!sdpfilter_enabled)) {
    sdpfilter_enabled = true;
    sdpfilter = Transparent;
  }*/

  return true;
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

  for (vector<string>::iterator it=elems.begin(); it != elems.end(); ++it) {
    SdpPayload p;
    if (!readPayload(p, *it)) return false;
    codecs.push_back(p);
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
          break;
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

  res += "codec_preference:     ";
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

bool SBCCallProfile::CodecPreferences::evaluate(const AmSipRequest& req,
    const string& app_param,
    AmUriParser& ruri_parser, AmUriParser& from_parser,
    AmUriParser& to_parser)
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

void SBCCallProfile::TranscoderSettings::infoPrint() const
{
  if (audio_codecs.size() > 0) {
    INFO("SBC:      transcode audio:\n");
    for (vector<SdpPayload>::const_iterator i = audio_codecs.begin(); i != audio_codecs.end(); ++i)
      INFO("SBC:         - %s\n", payload2str(*i).c_str());
  }
  if (callee_codec_capabilities.size() > 0) {
    INFO("SBC:      callee codec capabilities:\n");
    for (vector<PayloadDesc>::const_iterator i = callee_codec_capabilities.begin(); 
        i != callee_codec_capabilities.end(); ++i)
    {
      INFO("SBC:         - %s\n", i->print().c_str());
    }
  }
  string s("?");
  switch (transcoder_mode) {
    case Always: s = "always"; break;
    case Never: s = "never"; break;
    case OnMissingCompatible: s = "on_missing_compatible"; break;
  }
  INFO("SBC:      enable transcoder: %s\n", s.c_str());
}

bool SBCCallProfile::TranscoderSettings::readConfig(AmConfigReader &cfg)
{
  // store string values for later evaluation
  audio_codecs_str = cfg.getParameter("transcoder_codecs");
  callee_codec_capabilities_str = cfg.getParameter("callee_codeccaps");
  transcoder_mode_str = cfg.getParameter("enable_transcoder");

  return true;
}

bool SBCCallProfile::TranscoderSettings::operator==(const TranscoderSettings& rhs) const
{
  // TODO:transcoder_audio_codecs, mode, ...
  return true;
}

string SBCCallProfile::TranscoderSettings::print() const
{
  string res;
  // TODO: transcoder_audio_codecs, mode, ...
  return res;
}
  
bool SBCCallProfile::TranscoderSettings::evaluate(const AmSipRequest& req,
    const string& app_param,
    AmUriParser& ruri_parser, AmUriParser& from_parser,
    AmUriParser& to_parser)
{
  REPLACE_NONEMPTY_STR(transcoder_mode_str);
  REPLACE_NONEMPTY_STR(audio_codecs_str);
  REPLACE_NONEMPTY_STR(callee_codec_capabilities_str);

  if (!read(audio_codecs_str, audio_codecs)) return false;

  // TODO: verify that transcoder_audio_codecs are really supported natively!
  
  if (!readPayloadList(callee_codec_capabilities, callee_codec_capabilities_str)) 
    return false;
  
  if (!readTranscoderMode(transcoder_mode_str)) return false;

  // enable transcoder according to transcoder mode and optionally request's SDP
  switch (transcoder_mode) {
    case Always: enabled = true; break;
    case Never: enabled = false; break;
    case OnMissingCompatible: 
      enabled = isTranscoderNeeded(req, callee_codec_capabilities, 
                                 true /* if SDP can't be analyzed, enable transcoder */); 
      break;
  }

  if (enabled && audio_codecs.empty()) {
    ERROR("transcoder is enabled but no transcoder codecs selected ... disabling it\n");
    enabled = false;
  }

  return true;
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
