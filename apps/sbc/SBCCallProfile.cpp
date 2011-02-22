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

#include <algorithm>

#include "log.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmConfig.h"

bool SBCCallProfile::readFromConfiguration(const string& name,
					   const string profile_file_name) {
  if (cfg.loadFile(profile_file_name)) {
    ERROR("reading SBC call profile from '%s'\n", profile_file_name.c_str());
    return false;
  }

  profile_file = profile_file_name;

  ruri = cfg.getParameter("RURI");
  from = cfg.getParameter("From");
  to = cfg.getParameter("To");

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
  for (vector<string>::iterator it=elems.begin(); it != elems.end(); it++)
    headerfilter_list.insert(*it);

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
  }

  sst_enabled = cfg.getParameter("enable_session_timer", "no") == "yes";
  use_global_sst_config = !cfg.hasParameter("session_expires");
  
  auth_enabled = cfg.getParameter("enable_auth", "no") == "yes";
  auth_credentials.user = cfg.getParameter("auth_user");
  auth_credentials.pwd = cfg.getParameter("auth_pwd");

  call_timer_enabled = cfg.getParameter("enable_call_timer", "no") == "yes";
  call_timer = cfg.getParameter("call_timer");

  prepaid_enabled = cfg.getParameter("enable_prepaid", "no") == "yes";
  prepaid_accmodule = cfg.getParameter("prepaid_accmodule");
  prepaid_uuid = cfg.getParameter("prepaid_uuid");
  prepaid_acc_dest = cfg.getParameter("prepaid_acc_dest");

  // check for acc module if configured statically
  if (prepaid_enabled &&
      (prepaid_accmodule.find('$') == string::npos) &&
      (NULL == AmPlugIn::instance()->getFactory4Di(prepaid_accmodule))) {
    ERROR("prepaid accounting module '%s' used in call profile "
	  "'%s' is not loaded\n", prepaid_accmodule.c_str(), name.c_str());
    return false;
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

  outbound_interface = cfg.getParameter("outbound_interface");

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
    INFO("SBC:      SDP filter is %sabled, %s, %zd items in list\n",
	 sdpfilter_enabled?"en":"dis", FilterType2String(sdpfilter),
	 sdpfilter_list.size());

    INFO("SBC:      RTP relay %sabled\n", rtprelay_enabled?"en":"dis");
    if (rtprelay_enabled) {
      if (!force_symmetric_rtp.empty()) {
	INFO("SBC:      RTP force symmetric RTP: %s\n",
	     force_symmetric_rtp.c_str());
      }
      if (msgflags_symmetric_rtp) {
	INFO("SBC:      P-MsgFlags symmetric RTP detection enabled\n");
      }
    }

    INFO("SBC:      SST %sabled\n", sst_enabled?"en":"dis");
    INFO("SBC:      SIP auth %sabled\n", auth_enabled?"en":"dis");
    INFO("SBC:      call timer %sabled\n", call_timer_enabled?"en":"dis");
    if (call_timer_enabled) {
      INFO("SBC:                  %s seconds\n", call_timer.c_str());
    }
    INFO("SBC:      prepaid %sabled\n", prepaid_enabled?"en":"dis");
    if (prepaid_enabled) {
      INFO("SBC:                    acc_module = '%s'\n", prepaid_accmodule.c_str());
      INFO("SBC:                    uuid       = '%s'\n", prepaid_uuid.c_str());
      INFO("SBC:                    acc_dest   = '%s'\n", prepaid_acc_dest.c_str());
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

  return true;
}

bool SBCCallProfile::operator==(const SBCCallProfile& rhs) const {
  bool res =
    ruri == rhs.ruri &&
    from == rhs.from &&
    to == rhs.to &&
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
    use_global_sst_config == rhs.use_global_sst_config &&
    auth_enabled == rhs.auth_enabled &&
    call_timer_enabled == rhs.call_timer_enabled &&
    prepaid_enabled == rhs.prepaid_enabled &&
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
  if (call_timer_enabled) {
    res = res &&
      call_timer == rhs.call_timer;
  }
  if (prepaid_enabled) {
    res = res &&
      prepaid_accmodule == rhs.prepaid_accmodule &&
      prepaid_uuid == rhs.prepaid_uuid &&
      prepaid_acc_dest == rhs.prepaid_acc_dest;
  }

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
  res += "sst_enabled:          " + string(sst_enabled?"true":"false") + "\n";
  res += "use_global_sst_config:" + string(use_global_sst_config?"true":"false") + "\n";
  res += "auth_enabled:         " + string(auth_enabled?"true":"false") + "\n";
  res += "auth_user:            " + auth_credentials.user+"\n";
  res += "auth_pwd:             " + auth_credentials.pwd+"\n";
  res += "call_timer_enabled:   " + string(call_timer_enabled?"true":"false") + "\n";
  res += "call_timer:           " + call_timer + "\n";
  res += "prepaid_enabled:      " + string(prepaid_enabled?"true":"false") + "\n";
  res += "prepaid_accmodule:    " + prepaid_accmodule + "\n";
  res += "prepaid_uuid:         " + prepaid_uuid + "\n";
  res += "prepaid_acc_dest:     " + prepaid_acc_dest + "\n";
  res += "rtprelay_enabled:     " + string(rtprelay_enabled?"true":"false") + "\n";
  res += "force_symmetric_rtp:  " + force_symmetric_rtp;
  res += "msgflags_symmetric_rtp: " + string(msgflags_symmetric_rtp?"true":"false") + "\n";


  if (reply_translations.size()) {
    string reply_trans_codes;
    for(map<unsigned int, std::pair<unsigned int, string> >::const_iterator it=
	  reply_translations.begin(); it != reply_translations.end(); it++)
      reply_trans_codes += int2str(it->first)+"=>"+
	int2str(it->second.first)+" " + it->second.second+", ";
    reply_trans_codes.erase(reply_trans_codes.length()-2);

    res += "prepaid_acc_dest:     " + reply_trans_codes +"\n";
  }
  res += "append_headers:     " + append_headers + "\n";
  res += "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
  return res;
}

