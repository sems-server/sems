
#ifndef _SBCCallProfile_h
#define _SBCCallProfile_h

#include "AmConfigReader.h"
#include "HeaderFilter.h"
#include "ampi/UACAuthAPI.h"

#include <set>
#include <string>
#include <map>

using std::string;
using std::map;
using std::set;

struct SBCCallProfile {

  AmConfigReader cfg;

  string ruri;       /* updated if set */
  string from;       /* updated if set */
  string to;         /* updated if set */

  string callid;

  string outbound_proxy;
  bool force_outbound_proxy;

  string next_hop_ip;
  string next_hop_port;
  unsigned short next_hop_port_i;

  FilterType headerfilter;
  set<string> headerfilter_list;

  FilterType messagefilter;
  set<string> messagefilter_list;

  bool sdpfilter_enabled;
  FilterType sdpfilter;
  set<string> sdpfilter_list;

  bool sst_enabled;
  bool use_global_sst_config;

  bool auth_enabled;
  UACAuthCred auth_credentials;

  bool call_timer_enabled;
  string call_timer;

  bool prepaid_enabled;
  string prepaid_accmodule;
  string prepaid_uuid;
  string prepaid_acc_dest;

  map<unsigned int, std::pair<unsigned int, string> > reply_translations;

  // todo: RTP forwarding mode
  // todo: RTP transcoding mode

  SBCCallProfile()
  : headerfilter(Transparent),
    messagefilter(Transparent),
    sdpfilter_enabled(false),
    sdpfilter(Transparent),
    sst_enabled(false),
    auth_enabled(false),
    call_timer_enabled(false),
    prepaid_enabled(false)
  { }

  bool readFromConfiguration(const string& name, const string profile_file_name);
};

#endif // _SBCCallProfile_h
