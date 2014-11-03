/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>

#include "AmConfig.h"
#include "sems.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmSessionContainer.h"
#include "Am100rel.h"
#include "sip/transport.h"
#include "sip/resolver.h"
#include "sip/ip_util.h"
#include "sip/sip_timers.h"
#include "sip/raw_sender.h"

#include <cctype>
#include <algorithm>

using std::make_pair;

string       AmConfig::ConfigurationFile       = CONFIG_FILE;
string       AmConfig::ModConfigPath           = MOD_CFG_PATH;
string       AmConfig::PlugInPath              = PLUG_IN_PATH;
string       AmConfig::LoadPlugins             = "";
string       AmConfig::ExcludePlugins          = "";
string       AmConfig::ExcludePayloads         = "";
int          AmConfig::LogLevel                = L_INFO;
bool         AmConfig::LogStderr               = false;

vector<AmConfig::SIP_interface> AmConfig::SIP_Ifs;
vector<AmConfig::RTP_interface> AmConfig::RTP_Ifs;
map<string,unsigned short>      AmConfig::SIP_If_names;
map<string,unsigned short>      AmConfig::RTP_If_names;
map<string,unsigned short>      AmConfig::LocalSIPIP2If;
vector<AmConfig::SysIntf>         AmConfig::SysIfs;

#ifndef DISABLE_DAEMON_MODE
bool         AmConfig::DaemonMode              = DEFAULT_DAEMON_MODE;
string       AmConfig::DaemonPidFile           = DEFAULT_DAEMON_PID_FILE;
string       AmConfig::DaemonUid               = DEFAULT_DAEMON_UID;
string       AmConfig::DaemonGid               = DEFAULT_DAEMON_GID;
#endif

unsigned int AmConfig::MaxShutdownTime         = DEFAULT_MAX_SHUTDOWN_TIME;

int          AmConfig::SessionProcessorThreads = NUM_SESSION_PROCESSORS;
int          AmConfig::MediaProcessorThreads   = NUM_MEDIA_PROCESSORS;
int          AmConfig::RTPReceiverThreads      = NUM_RTP_RECEIVERS;
int          AmConfig::SIPServerThreads        = NUM_SIP_SERVERS;
string       AmConfig::OutboundProxy           = "";
bool         AmConfig::ForceOutboundProxy      = false;
string       AmConfig::NextHop                 = "";
bool         AmConfig::NextHop1stReq           = false;
bool         AmConfig::ProxyStickyAuth         = false;
bool         AmConfig::ForceOutboundIf         = false;
bool         AmConfig::ForceSymmetricRtp       = false;
bool         AmConfig::SipNATHandling          = false;
bool         AmConfig::UseRawSockets           = false;
bool         AmConfig::IgnoreNotifyLowerCSeq   = false;
string       AmConfig::Signature               = "";
unsigned int AmConfig::MaxForwards             = MAX_FORWARDS;
bool	     AmConfig::SingleCodecInOK	       = false;
unsigned int AmConfig::DeadRtpTime             = DEAD_RTP_TIME;
bool         AmConfig::IgnoreRTPXHdrs          = false;
string       AmConfig::Application             = "";
AmConfig::ApplicationSelector AmConfig::AppSelect        = AmConfig::App_SPECIFIED;
RegexMappingVector AmConfig::AppMapping;
bool         AmConfig::LogSessions             = false;
bool         AmConfig::LogEvents               = false;
int          AmConfig::UnhandledReplyLoglevel  = 0;

#ifdef WITH_ZRTP
bool         AmConfig::enable_zrtp             = true;
bool         AmConfig::enable_zrtp_debuglog    = true;
#endif

unsigned int AmConfig::SessionLimit            = 0;
unsigned int AmConfig::SessionLimitErrCode     = 503;
string       AmConfig::SessionLimitErrReason   = "Server overload";

unsigned int AmConfig::OptionsSessionLimit            = 0;
unsigned int AmConfig::OptionsSessionLimitErrCode     = 503;
string       AmConfig::OptionsSessionLimitErrReason   = "Server overload";

unsigned int AmConfig::CPSLimitErrCode     = 503;
string       AmConfig::CPSLimitErrReason   = "Server overload";

bool         AmConfig::AcceptForkedDialogs     = true;

bool         AmConfig::ShutdownMode            = false;
unsigned int AmConfig::ShutdownModeErrCode     = 503;
string       AmConfig::ShutdownModeErrReason   = "Server shutting down";
  
string AmConfig::OptionsTranscoderOutStatsHdr; // empty by default
string AmConfig::OptionsTranscoderInStatsHdr; // empty by default
string AmConfig::TranscoderOutStatsHdr; // empty by default
string AmConfig::TranscoderInStatsHdr; // empty by default

Am100rel::State AmConfig::rel100 = Am100rel::REL100_SUPPORTED;

vector <string> AmConfig::CodecOrder;

Dtmf::InbandDetectorType 
AmConfig::DefaultDTMFDetector     = Dtmf::SEMSInternal;
bool AmConfig::IgnoreSIGCHLD      = true;
bool AmConfig::IgnoreSIGPIPE      = true;

#ifdef USE_LIBSAMPLERATE
#ifndef USE_INTERNAL_RESAMPLER
AmAudio::ResamplingImplementationType AmConfig::ResamplingImplementationType = AmAudio::LIBSAMPLERATE;
#endif
#endif
#ifdef USE_INTERNAL_RESAMPLER
AmAudio::ResamplingImplementationType AmConfig::ResamplingImplementationType = AmAudio::INTERNAL_RESAMPLER;
#endif
#ifndef USE_LIBSAMPLERATE
#ifndef USE_INTERNAL_RESAMPLER
AmAudio::ResamplingImplementationType AmConfig::ResamplingImplementationType = AmAudio::UNAVAILABLE;
#endif
#endif

static int readInterfaces(AmConfigReader& cfg);

AmConfig::IP_interface::IP_interface()
  : LocalIP(),
    PublicIP()
{
}

AmConfig::SIP_interface::SIP_interface()
  : IP_interface(),
    LocalPort(5060),
    SigSockOpts(0),
    RtpInterface(-1)
{
}

AmConfig::RTP_interface::RTP_interface()
  : IP_interface(),
    RtpLowPort(RTP_LOWPORT),
    RtpHighPort(RTP_HIGHPORT),
    next_rtp_port(-1)
{
}

int AmConfig::RTP_interface::getNextRtpPort()
{
    
  int port=0;

  next_rtp_port_mut.lock();
  if(next_rtp_port < 0){
    next_rtp_port = RtpLowPort;
  }
    
  port = next_rtp_port & 0xfffe;
  next_rtp_port += 2;

  if(next_rtp_port >= RtpHighPort){
    next_rtp_port = RtpLowPort;
  }
  next_rtp_port_mut.unlock();
    
  return port;
}


int AmConfig::setLogLevel(const string& level, bool apply)
{
  int n;

  if (sscanf(level.c_str(), "%i", &n) == 1) {
    if (n < L_ERR || n > L_DBG) {
      return 0;
    }
  } else {
    string s(level);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "error" || s == "err") {
      n = L_ERR;
    } else if (s == "warning" || s == "warn") {
      n = L_WARN;
    } else if (s == "info") {
      n = L_INFO;
    } else if (s=="debug" || s == "dbg") {
      n = L_DBG;
    } else {
      return 0;
    }
  }

  LogLevel = n;
  if (apply) {
    log_level = LogLevel;
  }
  return 1;
}

int AmConfig::setLogStderr(const string& s, bool apply)
{
  if ( strcasecmp(s.c_str(), "yes") == 0 ) {
    LogStderr = true;
  } else if ( strcasecmp(s.c_str(), "no") == 0 ) {
    LogStderr = false;
  } else {
    return 0;
  }
  if (apply) {
    log_stderr = LogStderr;
  }
  return 1;
}

#ifndef DISABLE_DAEMON_MODE

int AmConfig::setDaemonMode(const string& fork) {
  if ( strcasecmp(fork.c_str(), "yes") == 0 ) {
    DaemonMode = true;
  } else if ( strcasecmp(fork.c_str(), "no") == 0 ) {
    DaemonMode = false;
  } else {
    return 0;
  }
  return 1;
}		

#endif /* !DISABLE_DAEMON_MODE */

int AmConfig::setSessionProcessorThreads(const string& th) {
  if(sscanf(th.c_str(),"%u",&SessionProcessorThreads) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setMediaProcessorThreads(const string& th) {
  if(sscanf(th.c_str(),"%u",&MediaProcessorThreads) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setRTPReceiverThreads(const string& th) {
  if(sscanf(th.c_str(),"%u",&RTPReceiverThreads) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setSIPServerThreads(const string& th){
  if(sscanf(th.c_str(),"%u",&SIPServerThreads) != 1) {
    return 0;
  }
  return 1;
}


int AmConfig::setDeadRtpTime(const string& drt)
{
  if(sscanf(drt.c_str(),"%u",&DeadRtpTime) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::readConfiguration()
{
  DBG("Reading configuration...\n");
  
  AmConfigReader cfg;
  int            ret=0;

  if(cfg.loadFile(AmConfig::ConfigurationFile.c_str())){
    ERROR("while loading main configuration file\n");
    return -1;
  }
       
  // take values from global configuration file
  // they will be overwritten by command line args

  // log_level
  if(cfg.hasParameter("loglevel")){
    if(!setLogLevel(cfg.getParameter("loglevel"))){
      ERROR("invalid log level specified\n");
      ret = -1;
    }
  }

  // stderr 
  if(cfg.hasParameter("stderr")){
    if(!setLogStderr(cfg.getParameter("stderr"), true)){
      ERROR("invalid stderr value specified,"
	    " valid are only yes or no\n");
      ret = -1;
    }
  }

#ifndef DISABLE_SYSLOG_LOG
  if (cfg.hasParameter("syslog_facility")) {
    set_syslog_facility(cfg.getParameter("syslog_facility").c_str());
  }
#endif

  // plugin_config_path
  if (cfg.hasParameter("plugin_config_path")) {
    ModConfigPath = cfg.getParameter("plugin_config_path",ModConfigPath);
  }

  if(!ModConfigPath.empty() && (ModConfigPath[ModConfigPath.length()-1] != '/'))
    ModConfigPath += '/';

  // Reads IP and port parameters
  if(readInterfaces(cfg) == -1)
    ret = -1;
  
  // outbound_proxy
  if (cfg.hasParameter("outbound_proxy"))
    OutboundProxy = cfg.getParameter("outbound_proxy");

  // force_outbound_proxy
  if(cfg.hasParameter("force_outbound_proxy")) {
    ForceOutboundProxy = (cfg.getParameter("force_outbound_proxy") == "yes");
  }

  if(cfg.hasParameter("next_hop")) {
    NextHop = cfg.getParameter("next_hop");
    NextHop1stReq = (cfg.getParameter("next_hop_1st_req") == "yes");
  }

  if(cfg.hasParameter("proxy_sticky_auth")) {
    ProxyStickyAuth = (cfg.getParameter("proxy_sticky_auth") == "yes");
  }

  if(cfg.hasParameter("force_outbound_if")) {
    ForceOutboundIf = (cfg.getParameter("force_outbound_if") == "yes");
  }

  if(cfg.hasParameter("use_raw_sockets")) {
    UseRawSockets = (cfg.getParameter("use_raw_sockets") == "yes");
    if(UseRawSockets && (raw_sender::init() < 0)) {
      UseRawSockets = false;
    }
  }

  if(cfg.hasParameter("ignore_notify_lower_cseq")) {
    IgnoreNotifyLowerCSeq = (cfg.getParameter("ignore_notify_lower_cseq") == "yes");
  }

  if(cfg.hasParameter("force_symmetric_rtp")) {
    ForceSymmetricRtp = (cfg.getParameter("force_symmetric_rtp") == "yes");
  }

  if(cfg.hasParameter("sip_nat_handling")) {
    SipNATHandling = (cfg.getParameter("sip_nat_handling") == "yes");
  }

  if(cfg.hasParameter("disable_dns_srv")) {
    _resolver::disable_srv = (cfg.getParameter("disable_dns_srv") == "yes");
  }
  

  for (int t = STIMER_A; t < __STIMER_MAX; t++) {

    string timer_cfg = string("sip_timer_") + timer_name(t);
    if(cfg.hasParameter(timer_cfg)) {

      sip_timers[t] = cfg.getParameterInt(timer_cfg, sip_timers[t]);
      INFO("Set SIP Timer '%s' to %u ms\n", timer_name(t), sip_timers[t]);
    }
  }

  if (cfg.hasParameter("sip_timer_t2")) {
    sip_timer_t2 = cfg.getParameterInt("sip_timer_t2", DEFAULT_T2_TIMER);
    INFO("Set SIP Timer T2 to %u ms\n", sip_timer_t2);
  }

  // plugin_path
  if (cfg.hasParameter("plugin_path"))
    PlugInPath = cfg.getParameter("plugin_path");

  // load_plugins
  if (cfg.hasParameter("load_plugins"))
    LoadPlugins = cfg.getParameter("load_plugins");

  if (cfg.hasParameter("load_plugins_rtld_global")) {
    vector<string> rtld_global_plugins =
      explode(cfg.getParameter("load_plugins_rtld_global"), ",");
    for (vector<string>::iterator it=
	   rtld_global_plugins.begin(); it != rtld_global_plugins.end(); it++) {
      AmPlugIn::instance()->set_load_rtld_global(*it);
    }
  }


  // exclude_plugins
  if (cfg.hasParameter("exclude_plugins"))
    ExcludePlugins = cfg.getParameter("exclude_plugins");

  // exclude_plugins
  if (cfg.hasParameter("exclude_payloads"))
    ExcludePayloads = cfg.getParameter("exclude_payloads");

  // user_agent
  if (cfg.getParameter("use_default_signature")=="yes")
    Signature = DEFAULT_SIGNATURE;
  else 
    Signature = cfg.getParameter("signature");

  if (cfg.hasParameter("max_forwards")) {
      unsigned int mf=0;
      if(str2i(cfg.getParameter("max_forwards"), mf)) {
	  ERROR("invalid max_forwards specified\n");
      }
      else {
	  MaxForwards = mf;
      }
  }

  if(cfg.hasParameter("log_sessions"))
    LogSessions = cfg.getParameter("log_sessions")=="yes";
  
  if(cfg.hasParameter("log_events"))
    LogEvents = cfg.getParameter("log_events")=="yes";

  if (cfg.hasParameter("unhandled_reply_loglevel")) {
    string msglog = cfg.getParameter("unhandled_reply_loglevel");
    if (msglog == "no") UnhandledReplyLoglevel = -1;
    else if (msglog == "error") UnhandledReplyLoglevel = 0;
    else if (msglog == "warn")  UnhandledReplyLoglevel = 1;
    else if (msglog == "info")  UnhandledReplyLoglevel = 2;
    else if (msglog == "debug") UnhandledReplyLoglevel = 3;
    else ERROR("Could not interpret unhandled_reply_loglevel \"%s\"\n",
	       msglog.c_str());
  }

  Application  = cfg.getParameter("application");

  if (Application == "$(ruri.user)") {
    AppSelect = App_RURIUSER;
  } else if (Application == "$(ruri.param)") {
    AppSelect = App_RURIPARAM;
  } else if (Application == "$(apphdr)") {
    AppSelect = App_APPHDR;
  } else if (Application == "$(mapping)") {
    AppSelect = App_MAPPING;  
    string appcfg_fname = ModConfigPath + "app_mapping.conf"; 
    DBG("Loading application mapping...\n");
    if (!read_regex_mapping(appcfg_fname, "=>", "application mapping",
			    AppMapping)) {
      ERROR("reading application mapping\n");
      ret = -1;
    }
  } else {
    AppSelect = App_SPECIFIED;
  }

#ifndef DISABLE_DAEMON_MODE

  // fork 
  if(cfg.hasParameter("fork")){
    if(!setDaemonMode(cfg.getParameter("fork"))){
      ERROR("invalid fork value specified,"
	    " valid are only yes or no\n");
      ret = -1;
    }
  }

  // daemon (alias for fork)
  if(cfg.hasParameter("daemon")){
    if(!setDaemonMode(cfg.getParameter("daemon"))){
      ERROR("invalid daemon value specified,"
	    " valid are only yes or no\n");
      ret = -1;
    }
  }

  if(cfg.hasParameter("daemon_uid")){
    DaemonUid = cfg.getParameter("daemon_uid");
  }

  if(cfg.hasParameter("daemon_gid")){
    DaemonGid = cfg.getParameter("daemon_gid");
  }

#endif /* !DISABLE_DAEMON_MODE */

  MaxShutdownTime = cfg.getParameterInt("max_shutdown_time",
					DEFAULT_MAX_SHUTDOWN_TIME);

  if(cfg.hasParameter("session_processor_threads")){
#ifdef SESSION_THREADPOOL
    if(!setSessionProcessorThreads(cfg.getParameter("session_processor_threads"))){
      ERROR("invalid session_processor_threads value specified\n");
      ret = -1;
    }
    if (SessionProcessorThreads<1) {
      ERROR("invalid session_processor_threads value specified."
	    " need at least one thread\n");
      ret = -1;
    }
#else
    WARN("session_processor_threads specified in sems.conf,\n");
    WARN("but SEMS is compiled without SESSION_THREADPOOL support.\n");
    WARN("set USE_THREADPOOL in Makefile.defs to enable session thread pool.\n");
    WARN("SEMS will start now, but every call will have its own thread.\n");    
#endif
  }

  if(cfg.hasParameter("media_processor_threads")){
    if(!setMediaProcessorThreads(cfg.getParameter("media_processor_threads"))){
      ERROR("invalid media_processor_threads value specified");
      ret = -1;
    }
  }

  if(cfg.hasParameter("rtp_receiver_threads")){
    if(!setRTPReceiverThreads(cfg.getParameter("rtp_receiver_threads"))){
      ERROR("invalid rtp_receiver_threads value specified");
      ret = -1;
    }
  }

  if(cfg.hasParameter("sip_server_threads")){
    if(!setSIPServerThreads(cfg.getParameter("sip_server_threads"))){
      ERROR("invalid sip_server_threads value specified");
      ret = -1;
    }
  }

  // single codec in 200 OK
  if(cfg.hasParameter("single_codec_in_ok")){
    SingleCodecInOK = (cfg.getParameter("single_codec_in_ok") == "yes");
  }

  // single codec in 200 OK
  if(cfg.hasParameter("ignore_rtpxheaders")){
    IgnoreRTPXHdrs = (cfg.getParameter("ignore_rtpxheaders") == "yes");
  }

  // codec_order
  CodecOrder = explode(cfg.getParameter("codec_order"), ",");

  // dead_rtp_time
  if(cfg.hasParameter("dead_rtp_time")){
    if(!setDeadRtpTime(cfg.getParameter("dead_rtp_time"))){
      ERROR("invalid dead_rtp_time value specified");
      ret = -1;
    }
  }

  if(cfg.hasParameter("dtmf_detector")){
    if (cfg.getParameter("dtmf_detector") == "spandsp") {
#ifndef USE_SPANDSP
      WARN("spandsp support not compiled in.\n");
#endif
      DefaultDTMFDetector = Dtmf::SpanDSP;
    }
  }

#ifdef WITH_ZRTP
  enable_zrtp = cfg.getParameter("enable_zrtp", "yes") == "yes";
  INFO("ZRTP %sabled\n", enable_zrtp ? "en":"dis");

  enable_zrtp_debuglog = cfg.getParameter("enable_zrtp_debuglog", "yes") == "yes";
  INFO("ZRTP debug log %sabled\n", enable_zrtp_debuglog ? "en":"dis");
#endif

  if(cfg.hasParameter("session_limit")){ 
    vector<string> limit = explode(cfg.getParameter("session_limit"), ";");
    if (limit.size() != 3) {
      ERROR("invalid session_limit specified.\n");
    } else {
      if (str2i(limit[0], SessionLimit) || str2i(limit[1], SessionLimitErrCode)) {
	ERROR("invalid session_limit specified.\n");
      }
      SessionLimitErrReason = limit[2];
    }
  }

  if(cfg.hasParameter("options_session_limit")){ 
    vector<string> limit = explode(cfg.getParameter("options_session_limit"), ";");
    if (limit.size() != 3) {
      ERROR("invalid options_session_limit specified.\n");
    } else {
      if (str2i(limit[0], OptionsSessionLimit) || str2i(limit[1], OptionsSessionLimitErrCode)) {
	ERROR("invalid options_session_limit specified.\n");
      }
      OptionsSessionLimitErrReason = limit[2];
    }
  }

  if(cfg.hasParameter("cps_limit")){ 
    unsigned int CPSLimit;
    vector<string> limit = explode(cfg.getParameter("cps_limit"), ";");
    if (limit.size() != 3) {
      ERROR("invalid cps_limit specified.\n");
    } else {
      if (str2i(limit[0], CPSLimit) || str2i(limit[1], CPSLimitErrCode)) {
	ERROR("invalid cps_limit specified.\n");
      }
      CPSLimitErrReason = limit[2];
    }
    AmSessionContainer::instance()->setCPSLimit(CPSLimit);
  }

  if(cfg.hasParameter("accept_forked_dialogs"))
    AcceptForkedDialogs = !(cfg.getParameter("accept_forked_dialogs") == "no");

  if(cfg.hasParameter("shutdown_mode_reply")){
    string c_reply = cfg.getParameter("shutdown_mode_reply");    
    size_t spos = c_reply.find(" ");
    if (spos == string::npos || spos == c_reply.length()) {
      ERROR("invalid shutdown_mode_reply specified, expected \"<code> <reason>\","
	    "e.g. shutdown_mode_reply=\"503 Not At The Moment, Please\".\n");
      ret = -1;

    } else {
      if (str2i(c_reply.substr(0, spos), ShutdownModeErrCode)) {
	ERROR("invalid shutdown_mode_reply specified, expected \"<code> <reason>\","
	      "e.g. shutdown_mode_reply=\"503 Not At The Moment, Please\".\n");
	ret = -1;
      }
      ShutdownModeErrReason = c_reply.substr(spos+1);
    }
  }

  OptionsTranscoderOutStatsHdr = cfg.getParameter("options_transcoder_out_stats_hdr");
  OptionsTranscoderInStatsHdr = cfg.getParameter("options_transcoder_in_stats_hdr");
  TranscoderOutStatsHdr = cfg.getParameter("transcoder_out_stats_hdr");
  TranscoderInStatsHdr = cfg.getParameter("transcoder_in_stats_hdr");

  if (cfg.hasParameter("100rel")) {
    string rel100s = cfg.getParameter("100rel");
    if (rel100s == "disabled" || rel100s == "off") {
      rel100 = Am100rel::REL100_DISABLED;
    } else if (rel100s == "supported") {
      rel100 = Am100rel::REL100_SUPPORTED;
    } else if (rel100s == "require") {
      rel100 = Am100rel::REL100_REQUIRE;
    } else {
      ERROR("unknown setting for '100rel' config option: '%s'.\n",
	    rel100s.c_str());
      ret = -1;
    }
  }

  if (cfg.hasParameter("resampling_library")) {
	string resamplings = cfg.getParameter("resampling_library");
	if (resamplings == "libsamplerate") {
	  ResamplingImplementationType = AmAudio::LIBSAMPLERATE;
	}
  }

  return ret;
}	

int AmConfig::insert_SIP_interface(const SIP_interface& intf)
{
  if(SIP_If_names.find(intf.name) !=
     SIP_If_names.end()) {

    if(intf.name != "default") {
      ERROR("duplicated interface name '%s'\n",intf.name.c_str());
      return -1;
    }

    unsigned int idx = SIP_If_names[intf.name];
    SIP_Ifs[idx] = intf;
  }
  else {
    SIP_Ifs.push_back(intf);
    unsigned int idx = SIP_Ifs.size()-1;
    SIP_If_names[intf.name] = idx;

    if(LocalSIPIP2If.find(intf.LocalIP) == 
       LocalSIPIP2If.end()) {

      LocalSIPIP2If.insert(make_pair(intf.LocalIP,idx));
    }
    else {
      map<string,unsigned short>::iterator it = 
	LocalSIPIP2If.find(intf.LocalIP);

      const SIP_interface& old_intf = SIP_Ifs[it->second];
      if(intf.LocalPort == old_intf.LocalPort) {
	ERROR("duplicated signaling interfaces "
	      "(%s and %s) detected using %s:%u",
	      old_intf.name.c_str(),intf.name.c_str(),
	      intf.LocalIP.c_str(),intf.LocalPort);

	return -1;
      }
      //FIXME: what happens now? shouldn't we insert the interface????
    }
  }

  return 0;
}

static int readSIPInterface(AmConfigReader& cfg, const string& i_name)
{
  int ret=0;
  AmConfig::SIP_interface intf;

  string suffix;
  if(!i_name.empty())
    suffix = "_" + i_name;

  // listen, sip_ip, sip_port, and media_ip
  if(cfg.hasParameter("sip_ip" + suffix)) {
    intf.LocalIP = cfg.getParameter("sip_ip" + suffix);
  }
  else {
    // no sip_ip definition
    return 0;
  }

  if(cfg.hasParameter("sip_port" + suffix)){
    string sip_port_str = cfg.getParameter("sip_port" + suffix);
    if(sscanf(sip_port_str.c_str(),"%u",
	      &(intf.LocalPort)) != 1){
      ERROR("sip_port%s: invalid sip port specified (%s)\n",
	    suffix.c_str(),
	    sip_port_str.c_str());
      ret = -1;
    }
  }

  // public_ip
  if(cfg.hasParameter("public_ip" + suffix)){
    intf.PublicIP = cfg.getParameter("public_ip" + suffix);
  }

  if(cfg.hasParameter("sig_sock_opts" + suffix)){
    vector<string> opt_strs = explode(cfg.getParameter("sig_sock_opts" + suffix),",");
    unsigned int opts = 0;
    for(vector<string>::iterator it_opt = opt_strs.begin();
	it_opt != opt_strs.end(); ++it_opt) {
      if(*it_opt == "force_via_address") {
	opts |= trsp_socket::force_via_address;
      } else if(*it_opt == "no_transport_in_contact") {
	opts |= trsp_socket::no_transport_in_contact;
      } else {
	WARN("unknown signaling socket option '%s' set on interface '%s'\n",
	     it_opt->c_str(),i_name.c_str());
      }
    }
    intf.SigSockOpts = opts;
  }

  intf.tcp_connect_timeout =
    cfg.getParameterInt("tcp_connect_timeout" + suffix,
			DEFAULT_TCP_CONNECT_TIMEOUT);

  intf.tcp_idle_timeout =
    cfg.getParameterInt("tcp_idle_timeout" + suffix, DEFAULT_TCP_IDLE_TIMEOUT);

  if(!i_name.empty())
    intf.name = i_name;
  else
    intf.name = "default";

  return AmConfig::insert_SIP_interface(intf);
}

int AmConfig::insert_RTP_interface(const RTP_interface& intf)
{
  if(RTP_If_names.find(intf.name) !=
     RTP_If_names.end()) {

    if(intf.name != "default") {
      ERROR("duplicated interface '%s'\n",intf.name.c_str());
      return -1;
    }

    unsigned int idx = RTP_If_names[intf.name];
    RTP_Ifs[idx] = intf;
  }
  else {
    // insert interface
    RTP_Ifs.push_back(intf);
    unsigned short rtp_idx = RTP_Ifs.size()-1;
    RTP_If_names[intf.name] = rtp_idx;
    
    // fix RtpInterface index in SIP interface
    map<string,unsigned short>::iterator sip_idx_it = 
      SIP_If_names.find(intf.name);

    if((sip_idx_it != SIP_If_names.end()) &&
       (SIP_Ifs.size() > sip_idx_it->second)) {
      SIP_Ifs[sip_idx_it->second].RtpInterface = rtp_idx;
    }
  }

  return 0;
}

static int readRTPInterface(AmConfigReader& cfg, const string& i_name)
{
  int ret=0;
  AmConfig::RTP_interface intf;

  string suffix;
  if(!i_name.empty())
    suffix = "_" + i_name;

  // media_ip
  if(cfg.hasParameter("media_ip" + suffix)) {
    intf.LocalIP = cfg.getParameter("media_ip" + suffix);
  }
  else {
    // no media definition for this interface name
    return 0;
  }

  // public_ip
  if(cfg.hasParameter("public_ip" + suffix)){
    intf.PublicIP = cfg.getParameter("public_ip" + suffix);
  }

  // rtp_low_port
  if(cfg.hasParameter("rtp_low_port" + suffix)){
    string rtp_low_port_str = cfg.getParameter("rtp_low_port" + suffix);
    if(sscanf(rtp_low_port_str.c_str(),"%u",
	      &(intf.RtpLowPort)) != 1){
      ERROR("rtp_low_port%s: invalid port number (%s)\n",
	    suffix.c_str(),rtp_low_port_str.c_str());
      ret = -1;
    }
  }

  // rtp_high_port
  if(cfg.hasParameter("rtp_high_port" + suffix)){
    string rtp_high_port_str = cfg.getParameter("rtp_high_port" + suffix);
    if(sscanf(rtp_high_port_str.c_str(),"%u",
	      &(intf.RtpHighPort)) != 1){
      ERROR("rtp_high_port%s: invalid port number (%s)\n",
	    suffix.c_str(),rtp_high_port_str.c_str());
      ret = -1;
    }
  }

  if(!i_name.empty())
    intf.name = i_name;
  else
    intf.name = "default";

  return AmConfig::insert_RTP_interface(intf);
}

static int readInterfaces(AmConfigReader& cfg)
{
  if(!cfg.hasParameter("interfaces")) {
    // no interface list defined:
    // read default params
    readSIPInterface(cfg,"");
    readRTPInterface(cfg,"");
    return 0;
  }

  vector<string> if_names;
  string ifs_str = cfg.getParameter("interfaces");
  if(ifs_str.empty()) {
    ERROR("empty interface list.\n");
    return -1;
  }

  if_names = explode(ifs_str,",");
  if(!if_names.size()) {
    ERROR("could not parse interface list.\n");
    return -1;
  }

  for(vector<string>::iterator it = if_names.begin();
      it != if_names.end(); it++) {

    readSIPInterface(cfg,*it);
    readRTPInterface(cfg,*it);

    if((AmConfig::SIP_If_names.find(*it) == AmConfig::SIP_If_names.end()) &&
       (AmConfig::RTP_If_names.find(*it) == AmConfig::RTP_If_names.end())) {
      ERROR("missing interface definition for '%s'\n",it->c_str());
      return -1;
    }
  }

  //TODO: check interfaces
  return 0;
}

/** Get the list of network interfaces with the associated addresses & flags */
static bool fillSysIntfList()
{
  struct ifaddrs *ifap = NULL;

  // socket to grab MTU
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0) {
    ERROR("socket() failed: %s",strerror(errno));
    return false;
  }
  
  if(getifaddrs(&ifap) < 0){
    ERROR("getifaddrs() failed: %s",strerror(errno));
    return false;
  }

  char host[NI_MAXHOST];
  for(struct ifaddrs *p_if = ifap; p_if != NULL; p_if = p_if->ifa_next) {

    if(p_if->ifa_addr == NULL)
      continue;
    
    if( (p_if->ifa_addr->sa_family != AF_INET) &&
        (p_if->ifa_addr->sa_family != AF_INET6) )
      continue;

    if( !(p_if->ifa_flags & IFF_UP) || !(p_if->ifa_flags & IFF_RUNNING) )
      continue;

    if(p_if->ifa_addr->sa_family == AF_INET6) {
      
      struct sockaddr_in6 *addr = (struct sockaddr_in6 *)p_if->ifa_addr;
      if(IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr)){
	// sorry, we don't support link-local addresses...
	continue;

	// convert address from kernel-style to userland
	// addr->sin6_scope_id = ntohs(*(uint16_t *)&addr->sin6_addr.s6_addr[2]);
	// addr->sin6_addr.s6_addr[2] = addr->sin6_addr.s6_addr[3] = 0;
      }
    }

    if (am_inet_ntop((const sockaddr_storage*)p_if->ifa_addr,
		     host, NI_MAXHOST) == NULL) {
      ERROR("am_inet_ntop() failed\n");
      continue;
      // freeifaddrs(ifap);
      // return false;
    }

    string iface_name(p_if->ifa_name);
    vector<AmConfig::SysIntf>::iterator intf_it;
    for(intf_it = AmConfig::SysIfs.begin();
	intf_it != AmConfig::SysIfs.end(); ++intf_it) {

      if(intf_it->name == iface_name)
	break;
    }

    if(intf_it == AmConfig::SysIfs.end()){
      unsigned int sys_if_idx = if_nametoindex(iface_name.c_str());
      if(AmConfig::SysIfs.size() < sys_if_idx+1)
	AmConfig::SysIfs.resize(sys_if_idx+1);

      intf_it = AmConfig::SysIfs.begin() + sys_if_idx;
      intf_it->name  = iface_name;
      intf_it->flags = p_if->ifa_flags;

      struct ifreq ifr;
      strncpy(ifr.ifr_name,p_if->ifa_name,IFNAMSIZ);

      if (ioctl(fd, SIOCGIFMTU, &ifr) < 0 ) {
	ERROR("ioctl: %s",strerror(errno));
	ERROR("setting MTU for this interface to default (1500)");
	intf_it->mtu = 1500;
      }
      else {
	intf_it->mtu = ifr.ifr_mtu;
      }
    }

    DBG("iface='%s';ip='%s';flags=0x%x\n",p_if->ifa_name,host,p_if->ifa_flags);
    intf_it->addrs.push_back(AmConfig::IPAddr(host,p_if->ifa_addr->sa_family));
  }

  freeifaddrs(ifap);
  close(fd);

  // add addresses from SysIntfList, if not present
  for(unsigned int idx = 0; idx < AmConfig::SIP_Ifs.size(); idx++) {

    vector<AmConfig::SysIntf>::iterator intf_it = AmConfig::SysIfs.begin();
    for(;intf_it != AmConfig::SysIfs.end(); ++intf_it) {

      list<AmConfig::IPAddr>::iterator addr_it = intf_it->addrs.begin();
      for(;addr_it != intf_it->addrs.end(); addr_it++) {
	if(addr_it->addr == AmConfig::SIP_Ifs[idx].LocalIP)
	  break;
      }

      // address not in this interface
      if(addr_it == intf_it->addrs.end())
	continue;

      // address is primary
      if(addr_it == intf_it->addrs.begin())
	continue;

      if(AmConfig::LocalSIPIP2If.find(intf_it->addrs.front().addr)
	 == AmConfig::LocalSIPIP2If.end()) {
	
	AmConfig::LocalSIPIP2If[intf_it->addrs.front().addr] = idx;
      }
    }
  }

  return true;
}

/** Get the AF_INET[6] address associated with the network interface */
string fixIface2IP(const string& dev_name, bool v6_for_sip)
{
  struct sockaddr_storage ss;
  if(am_inet_pton(dev_name.c_str(), &ss)) {
    if(v6_for_sip && (ss.ss_family == AF_INET6) && (dev_name[0] != '['))
      return "[" + dev_name + "]";
    else
      return dev_name;
  }

  for(vector<AmConfig::SysIntf>::iterator intf_it = AmConfig::SysIfs.begin();
      intf_it != AmConfig::SysIfs.end(); ++intf_it) {
      
    if(intf_it->name != dev_name)
      continue;

    if(intf_it->addrs.empty()){
      ERROR("No IP address for interface '%s'\n",intf_it->name.c_str());
      return "";
    }
      
    DBG("dev_name = '%s'\n",dev_name.c_str());
    return intf_it->addrs.front().addr;
  }    

  return "";
}

/** Get IP addrese from first non-loopback interface */
static string getDefaultIP()
{
  for(vector<AmConfig::SysIntf>::iterator intf_it = AmConfig::SysIfs.begin();
      intf_it != AmConfig::SysIfs.end(); ++intf_it) {
      
    if(intf_it->flags & IFF_LOOPBACK)
      continue;

    if(intf_it->addrs.empty())
      continue;

    DBG("dev_name = '%s'\n",intf_it->name.c_str());
    return intf_it->addrs.front().addr;
  }

  return "";
}

static int setNetInterface(AmConfig::IP_interface* ip_if)
{
  for(unsigned int i=0; i < AmConfig::SysIfs.size(); i++) {
    
    list<AmConfig::IPAddr>::iterator addr_it = AmConfig::SysIfs[i].addrs.begin();
    while(addr_it != AmConfig::SysIfs[i].addrs.end()) {
      if(ip_if->LocalIP == addr_it->addr) {
	ip_if->NetIf = AmConfig::SysIfs[i].name;
	ip_if->NetIfIdx = i;
	return 0;
      }
      addr_it++;
    }
  }
  
  // not interface found
  return -1;
}

int AmConfig::finalizeIPConfig()
{
  fillSysIntfList();

  // replace system interface names with IPs
  for(vector<SIP_interface>::iterator it = SIP_Ifs.begin();
      it != SIP_Ifs.end(); it++) {
    
    it->LocalIP = fixIface2IP(it->LocalIP,true);
    if(it->LocalIP.empty()) {
      ERROR("could not determine signaling IP for "
	    "interface '%s'\n", it->name.c_str());
      return -1;
    }

    if(!it->LocalPort)
      it->LocalPort = 5060;

    setNetInterface(&(*it));
  }

  for(vector<RTP_interface>::iterator it = RTP_Ifs.begin();
      it != RTP_Ifs.end(); it++) {
    
    if(it->LocalIP.empty()) {
      // try the IP from the signaling interface
      map<string, unsigned short>::iterator sip_if = 
	SIP_If_names.find(it->name);
      if(sip_if != SIP_If_names.end()) {
	it->LocalIP = SIP_Ifs[sip_if->second].LocalIP;
      }
      else {
	ERROR("could not determine media IP for "
	      "interface '%s'\n", it->name.c_str());
	return -1;
      }
    }
    else {
      it->LocalIP = fixIface2IP(it->LocalIP,false);
      if(it->LocalIP.empty()) {
	ERROR("could not determine media IP for "
	      "interface '%s'\n", it->name.c_str());
	return -1;
      }
    }

    setNetInterface(&(*it));
  }

  if(!SIP_Ifs.size()) {
    SIP_interface intf;
    intf.LocalIP = getDefaultIP();
    if(intf.LocalIP.empty()){
      ERROR("could not determine default signaling IP.");
      return -1;
    }
    SIP_Ifs.push_back(intf);
    SIP_If_names["default"] = 0;
  }

  if(!RTP_Ifs.size()) {
    RTP_interface intf;
    intf.LocalIP = SIP_Ifs[0].LocalIP;
    if(intf.LocalIP.empty()){
      ERROR("could not determine default media IP.");
      return -1;
    }
    RTP_Ifs.push_back(intf);
    RTP_If_names["default"] = 0;
  }

  return 0;
}

void AmConfig::dump_Ifs()
{
  INFO("Signaling interfaces:");
  for(int i=0; i<(int)SIP_Ifs.size(); i++) {
    
    SIP_interface& it_ref = SIP_Ifs[i];

    INFO("\t(%i) name='%s'" ";LocalIP='%s'" 
	 ";LocalPort='%u'" ";PublicIP='%s';TCP=%u/%u",
	 i,it_ref.name.c_str(),it_ref.LocalIP.c_str(),
	 it_ref.LocalPort,it_ref.PublicIP.c_str(),
	 it_ref.tcp_connect_timeout,
	 it_ref.tcp_idle_timeout);
  }
  
  INFO("Signaling address map:");
  for(multimap<string,unsigned short>::iterator it = LocalSIPIP2If.begin();
      it != LocalSIPIP2If.end(); ++it) {

    if(SIP_Ifs[it->second].name.empty()){
      INFO("\t%s -> default",it->first.c_str());
    }
    else {
      INFO("\t%s -> %s",it->first.c_str(),
	   SIP_Ifs[it->second].name.c_str());
    }
  }

  INFO("Media interfaces:");
  for(int i=0; i<(int)RTP_Ifs.size(); i++) {
    
    RTP_interface& it_ref = RTP_Ifs[i];

    INFO("\t(%i) name='%s'" ";LocalIP='%s'" 
	 ";Ports=[%u;%u]" ";PublicIP='%s'",
	 i,it_ref.name.c_str(),it_ref.LocalIP.c_str(),
	 it_ref.RtpLowPort,it_ref.RtpHighPort,
	 it_ref.PublicIP.c_str());
  }
}
