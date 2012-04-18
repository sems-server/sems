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

#ifndef _SBCCallProfile_h
#define _SBCCallProfile_h

#include "AmConfigReader.h"
#include "HeaderFilter.h"
#include "ampi/UACAuthAPI.h"
#include "ParamReplacer.h"

#include <set>
#include <string>
#include <map>
#include <list>

using std::string;
using std::map;
using std::set;

typedef map<string, AmArg>   SBCVarMapT;
typedef SBCVarMapT::iterator SBCVarMapIteratorT;

struct CCInterface {
  string cc_name;
  string cc_module;
  map<string, string> cc_values;

  CCInterface(string cc_name)
  : cc_name(cc_name) { }
  CCInterface() { }
};

typedef std::list<CCInterface> CCInterfaceListT;
typedef CCInterfaceListT::iterator CCInterfaceListIteratorT;

class PayloadDesc {
  protected:
    std::string name;
    unsigned clock_rate; // 0 means "doesn't matter"

  public:
    bool match(const SdpPayload &p) const;
    std::string print() const;
    bool operator==(const PayloadDesc &other) const;

    /* FIXME: really want all of this?
     * reads from format: name/clock_rate, nothing need to be set
     * for example: 
     *	  PCMU
     *	  bla/48000
     *	  /48000
     * */
    bool read(const std::string &s);
};

struct SBCCallProfile
  : public AmObject {
  string md5hash;
  string profile_file;

  string ruri;       /* updated if set */
  string ruri_host;  /* updated if set */
  string from;       /* updated if set */
  string to;         /* updated if set */
  string contact;

  string callid;

  string outbound_proxy;
  bool force_outbound_proxy;

  string next_hop;
  string next_hop_for_replies;

  FilterType headerfilter;
  set<string> headerfilter_list;

  FilterType messagefilter;
  set<string> messagefilter_list;

  bool sdpfilter_enabled;
  FilterType sdpfilter;
  set<string> sdpfilter_list;
  bool anonymize_sdp;

  bool sdpalinesfilter_enabled;
  FilterType sdpalinesfilter;
  set<string> sdpalinesfilter_list;

  string sst_enabled;
  bool sst_enabled_value;
  string sst_aleg_enabled;
  AmConfigReader sst_a_cfg;    // SST config (A leg)
  AmConfigReader sst_b_cfg;    // SST config (B leg)

  bool auth_enabled;
  UACAuthCred auth_credentials;

  bool auth_aleg_enabled;
  UACAuthCred auth_aleg_credentials;

  CCInterfaceListT cc_interfaces;

  SBCVarMapT cc_vars;

  map<unsigned int, std::pair<unsigned int, string> > reply_translations;

  string append_headers;

  string refuse_with;

  bool rtprelay_enabled;
  string force_symmetric_rtp;
  bool force_symmetric_rtp_value;
  bool msgflags_symmetric_rtp;
  bool rtprelay_transparent_seqno;
  bool rtprelay_transparent_ssrc;

  string rtprelay_interface;
  int rtprelay_interface_value;
  string aleg_rtprelay_interface;
  int aleg_rtprelay_interface_value;

  string outbound_interface;
  int outbound_interface_value;

  struct TranscoderSettings {
    // non-replaced parameters
    string callee_codec_capabilities_str, audio_codecs_str, transcoder_mode_str;

    std::vector<PayloadDesc> callee_codec_capabilities;
    std::vector<SdpPayload> audio_codecs;
    enum { Always, OnMissingCompatible, Never } transcoder_mode;
    bool readTranscoderMode(const std::string &src);
  
    bool enabled;
    
    bool evaluate(const AmSipRequest& req, 
                  const string& app_param, 
                  AmUriParser& ruri_parser, 
                  AmUriParser& from_parser, 
                  AmUriParser& to_parser);

    bool readConfig(AmConfigReader &cfg);
    void infoPrint() const;
    bool operator==(const TranscoderSettings& rhs) const;
    string print() const;

    bool isActive() { return enabled; }
  } transcoder;

  struct CodecPreferences {
    // non-replaced parameters
    string aleg_prefer_existing_payloads_str, aleg_payload_order_str;
    string bleg_prefer_existing_payloads_str, bleg_payload_order_str;

    /** when reordering payloads in relayed SDP from B leg to A leg prefer already
     * present payloads to the added ones by transcoder; i.e. transcoder codecs
     * are not ordered but added after ordering is done */
    bool aleg_prefer_existing_payloads;
    std::vector<PayloadDesc> aleg_payload_order;
    
    /** when reordering payloads in relayed SDP from A leg to B leg prefer already
     * present payloads to the added ones by transcoder; i.e. transcoder codecs
     * are not ordered but added after ordering is done */
    bool bleg_prefer_existing_payloads;
    std::vector<PayloadDesc> bleg_payload_order;

    bool readConfig(AmConfigReader &cfg);
    void infoPrint() const;
    bool operator==(const CodecPreferences& rhs) const;
    string print() const;
  
    void orderSDP(AmSdp& sdp, bool a_leg); // do the SDP changes
    bool shouldOrderPayloads(bool a_leg); // returns if call to orderSDP is needed

    // return true if ordering should be done before adding transcoder codecs
    bool preferExistingCodecs(bool a_leg) {
      return a_leg ? bleg_prefer_existing_payloads : aleg_prefer_existing_payloads;
    }

    bool evaluate(const AmSipRequest& req, 
                  const string& app_param, 
                  AmUriParser& ruri_parser, 
                  AmUriParser& from_parser, 
                  AmUriParser& to_parser);

    // default settings
    CodecPreferences(): aleg_prefer_existing_payloads(false) ,bleg_prefer_existing_payloads(false) { }
  } codec_prefs;

  // todo: RTP transcoding mode

  SBCCallProfile()
  : headerfilter(Undefined),
    messagefilter(Undefined),
    sdpfilter_enabled(false),
    sdpfilter(Undefined),
    sdpalinesfilter_enabled(false),
    sdpalinesfilter(Undefined),
    auth_enabled(false),
    sst_enabled_value(false),
    rtprelay_enabled(false),
    force_symmetric_rtp_value(false),
    rtprelay_transparent_seqno(true),
    rtprelay_transparent_ssrc(true),
    rtprelay_interface_value(-1),
    aleg_rtprelay_interface_value(-1),
    outbound_interface_value(-1)
  { }

  ~SBCCallProfile()
  { }

  bool readFromConfiguration(const string& name, const string profile_file_name);

  bool operator==(const SBCCallProfile& rhs) const;
  string print() const;

  bool evaluate(const AmSipRequest& req,
      const string& app_param,
      AmUriParser& ruri_parser, AmUriParser& from_parser,
      AmUriParser& to_parser);
};

#endif // _SBCCallProfile_h
