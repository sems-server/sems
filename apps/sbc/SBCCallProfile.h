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
#include "atomic_types.h"
#include "sip/msg_logger.h"

#include <set>
#include <string>
#include <map>
#include <list>

using std::string;
using std::map;
using std::set;
using std::pair;

typedef map<string, AmArg>   SBCVarMapT;
typedef SBCVarMapT::iterator SBCVarMapIteratorT;
typedef SBCVarMapT::const_iterator SBCVarMapConstIteratorT;

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
typedef CCInterfaceListT::const_iterator CCInterfaceListConstIteratorT;

template <class T>
class ref_counted_ptr
{
  private:
    T *ptr;

  public:
    void reset(T *p) { if (ptr) dec_ref(ptr); ptr = p; if (ptr) inc_ref(ptr); }
    T *get() const { return ptr; }

    ref_counted_ptr(): ptr(0) { }
    ~ref_counted_ptr() { if (ptr) dec_ref(ptr); }

    ref_counted_ptr(const ref_counted_ptr &other): ptr(other.ptr) { if (ptr) inc_ref(ptr); }
    ref_counted_ptr &operator=(const ref_counted_ptr &other) { reset(other.ptr); return *this; }

};

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

typedef pair<unsigned int, std::string> ReplyCodeReasonPair;
typedef map<unsigned int, ReplyCodeReasonPair> ReplyTranslationMap;

struct SBCCallProfile
  : public AmObject {
  string md5hash;
  string profile_file;

  string ruri;       /* updated if set */
  string ruri_host;  /* updated if set */
  string from;       /* updated if set */
  string to;         /* updated if set */

  struct Contact {
    string displayname;
    string user;
    string host;
    string port;

    bool   hiding;
    string hiding_prefix;
    string hiding_vars;
  };
  
  Contact contact;

  string callid;

  string dlg_contact_params;
  string bleg_dlg_contact_params;

  bool transparent_dlg_id;
  bool dlg_nat_handling;
  bool keep_vias;
  bool bleg_keep_vias;

  string outbound_proxy;
  bool force_outbound_proxy;

  string aleg_outbound_proxy;
  bool aleg_force_outbound_proxy;

  string next_hop;
  bool next_hop_1st_req;
  bool patch_ruri_next_hop;
  bool next_hop_fixed;

  string aleg_next_hop;

  bool allow_subless_notify;

  vector<FilterEntry> headerfilter;
  vector<FilterEntry> messagefilter;

  bool anonymize_sdp;
  vector<FilterEntry> sdpfilter;
  vector<FilterEntry> sdpalinesfilter;
  vector<FilterEntry> mediafilter;

  string sst_enabled;
  bool sst_enabled_value;
  string sst_aleg_enabled;
  AmConfigReader sst_a_cfg;    // SST config (A leg)
  AmConfigReader sst_b_cfg;    // SST config (B leg)

  string fix_replaces_inv;
  string fix_replaces_ref;

  bool auth_enabled;
  UACAuthCred auth_credentials;

  bool auth_aleg_enabled;
  UACAuthCred auth_aleg_credentials;
  bool uas_auth_bleg_enabled;
  UACAuthCred uas_auth_bleg_credentials;

  CCInterfaceListT cc_interfaces;

  SBCVarMapT cc_vars;

  ReplyTranslationMap reply_translations;

  string append_headers;
  string append_headers_req;
  string aleg_append_headers_req;

  string refuse_with;

  string rtprelay_enabled;
  bool rtprelay_enabled_value;
  string force_symmetric_rtp;
  string aleg_force_symmetric_rtp;
  bool force_symmetric_rtp_value;
  bool aleg_force_symmetric_rtp_value;

  bool msgflags_symmetric_rtp;
  bool rtprelay_transparent_seqno;
  bool rtprelay_transparent_ssrc;
  bool rtprelay_dtmf_filtering;
  bool rtprelay_dtmf_detection;

  string rtprelay_interface;
  int rtprelay_interface_value;
  string aleg_rtprelay_interface;
  int aleg_rtprelay_interface_value;

  int rtprelay_bw_limit_rate;
  int rtprelay_bw_limit_peak;

  list<atomic_int*> aleg_rtp_counters;
  list<atomic_int*> bleg_rtp_counters;

  string outbound_interface;
  int outbound_interface_value;

  string aleg_outbound_interface;
  int aleg_outbound_interface_value;

  struct TranscoderSettings {
    // non-replaced parameters
    string callee_codec_capabilities_str, audio_codecs_str, 
      transcoder_mode_str, lowfi_codecs_str, dtmf_mode_str,
      audio_codecs_norelay_str, audio_codecs_norelay_aleg_str;

    std::vector<PayloadDesc> callee_codec_capabilities;
    std::vector<SdpPayload> audio_codecs;
    std::vector<SdpPayload> audio_codecs_norelay;
    std::vector<SdpPayload> audio_codecs_norelay_aleg;
    std::vector<SdpPayload> lowfi_codecs;
    enum { Always, OnMissingCompatible, Never } transcoder_mode;
    enum { DTMFAlways, DTMFLowFiCodecs, DTMFNever } dtmf_mode;
    bool readTranscoderMode(const std::string &src);
    bool readDTMFMode(const std::string &src);
  
    bool enabled;
    
    bool evaluate(ParamReplacerCtx& ctx, const AmSipRequest& req);

    bool readConfig(AmConfigReader &cfg);
    void infoPrint() const;
    bool operator==(const TranscoderSettings& rhs) const;
    string print() const;

    bool isActive() { return enabled; }
    TranscoderSettings(): enabled(false), transcoder_mode(Never) { }
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

    bool evaluate(ParamReplacerCtx& ctx, const AmSipRequest& req);

    // default settings
    CodecPreferences(): aleg_prefer_existing_payloads(false) ,bleg_prefer_existing_payloads(false) { }
  } codec_prefs;

  bool contact_hiding;
  string contact_hiding_prefix;
  string contact_hiding_vars;

  bool reg_caching;
  unsigned int min_reg_expires;
  unsigned int max_ua_expires;

  // todo: RTP transcoding mode

  // hold settings
  class HoldSettings {
    public:
        enum Activity { sendrecv, sendonly, recvonly, inactive };

    private:
      struct HoldParams {
        // non-replaced params
        string mark_zero_connection_str, activity_str, alter_b2b_str;

        bool mark_zero_connection;
        Activity activity;
        bool alter_b2b; // transform B2B hold requests (not locally generated ones)

        bool setActivity(const string &s);
        HoldParams(): mark_zero_connection(false), activity(sendonly), alter_b2b(false) { }
      } aleg, bleg;

    public:
      bool mark_zero_connection(bool a_leg) { return a_leg ? aleg.mark_zero_connection : bleg.mark_zero_connection; }
      Activity activity(bool a_leg) { return a_leg ? aleg.activity : bleg.activity; }
      const string &activity_str(bool a_leg) { return a_leg ? aleg.activity_str : bleg.activity_str; }
      bool alter_b2b(bool a_leg) { return a_leg ? aleg.alter_b2b : bleg.alter_b2b; }

      void readConfig(AmConfigReader &cfg);
      bool evaluate(ParamReplacerCtx& ctx, const AmSipRequest& req);
  } hold_settings;

  // maximum retry time for repeating reINVITE after 491 response (in
  // milliseconds), according to RFC 3261 should be 2000 ms
  int max_491_retry_time;

 private:
  // message logging feature
  string msg_logger_path;
  ref_counted_ptr<msg_logger> logger;

  void create_logger(const AmSipRequest& req);

 public:
  bool log_rtp;
  bool log_sip;
  bool has_logger() { return logger.get() != NULL; }
  msg_logger* get_logger(const AmSipRequest& req);
  void set_logger_path(const std::string path) { msg_logger_path = path; }
  const string &get_logger_path() { return msg_logger_path; }

  SBCCallProfile()
  : auth_enabled(false),
    transparent_dlg_id(false),
    dlg_nat_handling(false),
    keep_vias(false),bleg_keep_vias(false),
    sst_enabled_value(false),
    rtprelay_enabled_value(false),
    force_symmetric_rtp_value(false),
    aleg_force_symmetric_rtp_value(false),
    rtprelay_transparent_seqno(true),
    rtprelay_transparent_ssrc(true),
    rtprelay_interface_value(-1),
    aleg_rtprelay_interface_value(-1),
    rtprelay_bw_limit_rate(-1),
    rtprelay_bw_limit_peak(-1),
    outbound_interface_value(-1),
    contact_hiding(false), 
    reg_caching(false),
    log_rtp(false),
    log_sip(false),
    patch_ruri_next_hop(false),
    next_hop_1st_req(false),
    next_hop_fixed(false),
    allow_subless_notify(false),
    max_491_retry_time(2000)
  { }

  ~SBCCallProfile()
  { }

  bool readFromConfiguration(const string& name, const string profile_file_name);

  bool operator==(const SBCCallProfile& rhs) const;
  string print() const;

  int refuse(ParamReplacerCtx& ctx, const AmSipRequest& req) const;

  int apply_a_routing(ParamReplacerCtx& ctx,
		      const AmSipRequest& req,
		      AmBasicSipDialog& dlg) const;
  
  int apply_b_routing(ParamReplacerCtx& ctx,
		      const AmSipRequest& req,
		      AmBasicSipDialog& dlg) const;

  int apply_common_fields(ParamReplacerCtx& ctx,
			  AmSipRequest& req) const;

  bool evaluateOutboundInterface();

  bool evaluate(ParamReplacerCtx& ctx,
		const AmSipRequest& req);

  bool evaluateRTPRelayInterface();
  bool evaluateRTPRelayAlegInterface();

  void eval_sst_config(ParamReplacerCtx& ctx,
		       const AmSipRequest& req,
		       AmConfigReader& sst_cfg);

  void replace_cc_values(ParamReplacerCtx& ctx,
			 const AmSipRequest& req,
			 AmArg* values);

  void eval_cc_list(ParamReplacerCtx& ctx, const AmSipRequest& req);
  
  void fix_append_hdrs(ParamReplacerCtx& ctx, const AmSipRequest& req);

  void fix_reg_contact(ParamReplacerCtx& ctx, const AmSipRequest& req,
		       AmUriParser& contact) const;

  /**
   * Reg-cache lookup:
   * - searches for alias in the reg-cache.
   * - sets next-hop & outbound_interface on the given dialog
   * @return retargeted R-URI
   */
  string retarget(const string& alias, AmBasicSipDialog& dlg) const;

  /**
   * Reg-cache lookup:
   * - searches for alias in the reg-cache.
   * - sets next-hop & outbound_interface in this profile
   * @return retargeted R-URI
   */
  string retarget(const string& alias);
};

#endif // _SBCCallProfile_h
