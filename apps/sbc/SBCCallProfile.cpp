#include "SBCCallProfile.h"

#include <algorithm>

#include "log.h"
#include "AmUtils.h"
#include "AmPlugIn.h"


bool SBCCallProfile::readFromConfiguration(const string& name,
					   const string profile_file_name) {
  if (cfg.loadFile(profile_file_name)) {
    ERROR("reading SBC call profile from '%s'\n", profile_file_name.c_str());
    return false;
  }

  ruri = cfg.getParameter("RURI");
  from = cfg.getParameter("From");
  to = cfg.getParameter("To");

  callid = cfg.getParameter("Call-ID");

  force_outbound_proxy = cfg.getParameter("force_outbound_proxy") == "yes";
  outbound_proxy = cfg.getParameter("outbound_proxy");

  next_hop_ip = cfg.getParameter("next_hop_ip");
  next_hop_port = cfg.getParameter("next_hop_port");

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

  INFO("SBC: loaded SBC profile '%s':\n", name.c_str());

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
  }

  INFO("SBC:      header filter  is %s, %zd items in list\n",
       FilterType2String(headerfilter), headerfilter_list.size());
  INFO("SBC:      message filter is %s, %zd items in list\n",
       FilterType2String(messagefilter), messagefilter_list.size());
  INFO("SBC:      SDP filter is %sabled, %s, %zd items in list\n",
       sdpfilter_enabled?"en":"dis", FilterType2String(sdpfilter),
       sdpfilter_list.size());

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

  return true;
}
