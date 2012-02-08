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

bool SBCCallProfile::readFromConfiguration(const string& name,
					   const string profile_file_name) {
  AmConfigReader cfg;
  if (cfg.loadFile(profile_file_name)) {
    ERROR("reading SBC call profile from '%s'\n", profile_file_name.c_str());
    return false;
  }

  profile_file = profile_file_name;

  ruri = cfg.getParameter("RURI");
  from = cfg.getParameter("From");
  to = cfg.getParameter("To");
  contact = cfg.getParameter("Contact");

  callid = cfg.getParameter("Call-ID");

  force_outbound_proxy = cfg.getParameter("force_outbound_proxy") == "yes";
  outbound_proxy = cfg.getParameter("outbound_proxy");

  next_hop_ip = cfg.getParameter("next_hop_ip");
  next_hop_port = cfg.getParameter("next_hop_port");
  next_hop_for_replies = cfg.getParameter("next_hop_for_replies");

  string hf_type = cfg.getParameter("header_filter", "transparent");
  if (hf_type=="transparent")
    headerfilter = Transparent;
  else if (hf_type=="whitelist")
    headerfilter = Whitelist;
  else if (hf_type=="blacklist")
    headerfilter = Blacklist;
  else {
    ERROR("invalid header_filter mode '%s'\n", hf_type.c_str());
    return false;
  }
  
  vector<string> elems = explode(cfg.getParameter("header_list"), ",");
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++) {
    transform(it->begin(), it->end(), it->begin(), ::tolower);
    headerfilter_list.insert(*it);
  }

  string mf_type = cfg.getParameter("message_filter", "transparent");
  if (mf_type=="transparent")
    messagefilter = Transparent;
  else if (mf_type=="whitelist")
    messagefilter = Whitelist;
  else if (hf_type=="blacklist")
    messagefilter = Blacklist;
  else {
    ERROR("invalid message_filter mode '%s'\n", mf_type.c_str());
    return false;
  }
  
  elems = explode(cfg.getParameter("message_list"), ",");
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++)
    messagefilter_list.insert(*it);

  string sdp_filter = cfg.getParameter("sdp_filter");
  if (sdp_filter=="transparent") {
    sdpfilter_enabled = true;
    sdpfilter = Transparent;
  } else if (sdp_filter=="whitelist") {
    sdpfilter_enabled = true;
    sdpfilter = Whitelist;
  } else if (sdp_filter=="blacklist") {
    sdpfilter_enabled = true;
    sdpfilter = Blacklist;
  } else {
    sdpfilter_enabled = false;
  }
  if (sdpfilter_enabled) {
    vector<string> c_elems = explode(cfg.getParameter("sdpfilter_list"), ",");
    for (vector<string>::iterator it=c_elems.begin(); it != c_elems.end(); it++) {
      string c = *it;
      std::transform(c.begin(), c.end(), c.begin(), ::tolower);
      sdpfilter_list.insert(c);
    }
    anonymize_sdp = cfg.getParameter("sdp_anonymize", "no") == "yes";
  }

  string cfg_sdp_alines_filter = cfg.getParameter("sdp_alines_filter");
  if (cfg_sdp_alines_filter=="whitelist") {
    sdpalinesfilter_enabled = true;
    sdpalinesfilter = Whitelist;
  } else if (cfg_sdp_alines_filter=="blacklist") {
    sdpalinesfilter_enabled = true;
    sdpalinesfilter = Blacklist;
  } else {
    sdpalinesfilter_enabled = false;
  }
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

  if (!readPayloadOrder(cfg.getParameter("payload_order"))) return false;
  if (payload_order.size() && (!sdpfilter_enabled)) {
    sdpfilter_enabled = true;
    sdpfilter = Transparent;
  }

  md5hash = "<unknown>";
  if (!cfg.getMD5(profile_file_name, md5hash)){
    ERROR("calculating MD5 of file %s\n", profile_file_name.c_str());
  }

  INFO("SBC: loaded SBC profile '%s' - MD5: %s\n", name.c_str(), md5hash.c_str());

  if (!refuse_with.empty()) {
    INFO("SBC:      refusing calls with '%s'\n", refuse_with.c_str());
  } else {
    INFO("SBC:      RURI = '%s'\n", ruri.c_str());
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
    if (!next_hop_ip.empty()) {
      INFO("SBC:      next hop = %s%s\n", next_hop_ip.c_str(),
	   next_hop_port.empty()? "" : (":"+next_hop_port).c_str());

      if (!next_hop_for_replies.empty()) {
	INFO("SBC:      next hop used for replies: '%s'\n", next_hop_for_replies.c_str());
      }
    }

    INFO("SBC:      header filter  is %s, %zd items in list\n",
	 FilterType2String(headerfilter), headerfilter_list.size());
    INFO("SBC:      message filter is %s, %zd items in list\n",
	 FilterType2String(messagefilter), messagefilter_list.size());
    INFO("SBC:      SDP filter is %sabled, %s, %zd items in list, %sanonymizing SDP\n",
	 sdpfilter_enabled?"en":"dis", FilterType2String(sdpfilter),
	 sdpfilter_list.size(), anonymize_sdp?"":"not ");
    INFO("SBC:      SDP alines-filter is %sabled, %s, %zd items in list\n",
	 sdpalinesfilter_enabled?"en":"dis", FilterType2String(sdpalinesfilter),
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

  if (payload_order.size() > 0) {
    INFO("SBC:      payload order:\n");
    for (vector<PayloadDesc>::iterator i = payload_order.begin(); i != payload_order.end(); ++i)
    INFO("SBC:         - %s\n", i->print().c_str());
  }

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
    from == rhs.from &&
    to == rhs.to &&
    contact == rhs.contact &&
    callid == rhs.callid &&
    outbound_proxy == rhs.outbound_proxy &&
    force_outbound_proxy == rhs.force_outbound_proxy &&
    next_hop_ip == rhs.next_hop_ip &&
    next_hop_port == rhs.next_hop_port &&
    next_hop_port_i == rhs.next_hop_port_i &&
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
  res = res && payloadDescsEqual(payload_order, rhs.payload_order);
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
  res += "from:                 " + from + "\n";
  res += "to:                   " + to + "\n";
  res += "contact:              " + contact + "\n";
  res += "callid:               " + callid + "\n";
  res += "outbound_proxy:       " + outbound_proxy + "\n";
  res += "force_outbound_proxy: " + string(force_outbound_proxy?"true":"false") + "\n";
  res += "next_hop_ip:          " + next_hop_ip + "\n";
  res += "next_hop_port:        " + next_hop_port + "\n";
  res += "next_hop_port_i:      " + int2str(next_hop_port_i) + "\n";
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
  res += "payload_order:        ";
  for (vector<PayloadDesc>::const_iterator i = payload_order.begin(); i != payload_order.end(); ++i) {
    if (i != payload_order.begin()) res += ",";
    res += i->print();
  }
  res += "\n";


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

bool SBCCallProfile::readPayloadOrder(const std::string &src)
{
  vector<string> elems = explode(src, ",");
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); ++it) {
    PayloadDesc payload;
    if (!payload.read(*it)) return false;
    payload_order.push_back(payload);
  }
  return true;
}

void SBCCallProfile::orderSDP(AmSdp& sdp)
{
  if (payload_order.size() < 1) return; // nothing to do - no predefined order

  DBG("ordering SDP\n");
  for (vector<SdpMedia>::iterator m_it =
	 sdp.media.begin(); m_it != sdp.media.end(); ++m_it) {
    SdpMedia& media = *m_it;

    int pos = 0;
    unsigned idx;
    unsigned cnt = media.payloads.size();

    // TODO: optimize
    // for each predefined payloads in their order
    for (vector<PayloadDesc>::iterator i = payload_order.begin(); i != payload_order.end(); ++i) {
      // try to find this payload in SDP 
      // (not needed to go through already sorted members and current
      // destination pos)
      for (idx = pos + 1; idx < cnt; idx++) {
        if (i->match(media.payloads[idx])) {
          // found, exchange elements at pos and idx
          SdpPayload p = media.payloads[idx]; 
          media.payloads[idx] = media.payloads[pos];
          media.payloads[pos] = p;
	
	  ++pos; // next payload index
          break;
	}
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////

bool PayloadDesc::match(const SdpPayload &p) const
{
  //FIXME: payload names case sensitive?
  if ((name.size() > 0) && (name != p.encoding_name)) return false;
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
