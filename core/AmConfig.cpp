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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include "AmConfig.h"
#include "sems.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmSession.h"

#include <cctype>
#include <algorithm>

string       AmConfig::ConfigurationFile       = CONFIG_FILE;
string       AmConfig::ModConfigPath           = MOD_CFG_PATH;
string       AmConfig::PlugInPath              = PLUG_IN_PATH;
string       AmConfig::LoadPlugins             = "";
string       AmConfig::ExcludePlugins          = "";
string       AmConfig::ExcludePayloads         = "";
int          AmConfig::LogLevel                = L_INFO;
bool         AmConfig::LogStderr               = false;

vector<AmConfig::IP_interface>  AmConfig::Ifs;
map<string,unsigned short>      AmConfig::If_names;
multimap<string,unsigned short> AmConfig::LocalSIPIP2If;

#ifndef DISABLE_DAEMON_MODE
bool         AmConfig::DaemonMode              = DEFAULT_DAEMON_MODE;
string       AmConfig::DaemonPidFile           = DEFAULT_DAEMON_PID_FILE;
string       AmConfig::DaemonUid               = DEFAULT_DAEMON_UID;
string       AmConfig::DaemonGid               = DEFAULT_DAEMON_GID;
#endif

unsigned int AmConfig::MaxShutdownTime         = DEFAULT_MAX_SHUTDOWN_TIME;

int          AmConfig::SessionProcessorThreads = NUM_SESSION_PROCESSORS;
int          AmConfig::MediaProcessorThreads   = NUM_MEDIA_PROCESSORS;
int          AmConfig::SIPServerThreads        = NUM_SIP_SERVERS;
string       AmConfig::OutboundProxy           = "";
bool         AmConfig::ForceOutboundProxy      = false;
bool         AmConfig::ProxyStickyAuth         = false;
bool         AmConfig::DisableDNSSRV           = false;
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

unsigned int AmConfig::SessionLimit            = 0;
unsigned int AmConfig::SessionLimitErrCode     = 503;
string       AmConfig::SessionLimitErrReason   = "Server overload";

unsigned int AmConfig::OptionsSessionLimit            = 0;
unsigned int AmConfig::OptionsSessionLimitErrCode     = 503;
string       AmConfig::OptionsSessionLimitErrReason   = "Server overload";

AmSipDialog::provisional_100rel AmConfig::rel100      = REL100_SUPPORTED;

vector <string> AmConfig::CodecOrder;

Dtmf::InbandDetectorType 
AmConfig::DefaultDTMFDetector     = Dtmf::SEMSInternal;
bool AmConfig::IgnoreSIGCHLD      = true;
bool AmConfig::IgnoreSIGPIPE      = true;

static int readInterfaces(AmConfigReader& cfg);

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

  if(cfg.hasParameter("proxy_sticky_auth")) {
    ProxyStickyAuth = (cfg.getParameter("proxy_sticky_auth") == "yes");
  }

  if(cfg.hasParameter("disable_dns_srv")) {
    DisableDNSSRV = (cfg.getParameter("disable_dns_srv") == "yes");
  }
  
  // plugin_path
  if (cfg.hasParameter("plugin_path"))
    PlugInPath = cfg.getParameter("plugin_path");

  // load_plugins
  if (cfg.hasParameter("load_plugins"))
    LoadPlugins = cfg.getParameter("load_plugins");

  // exclude_plugins
  if (cfg.hasParameter("exclude_plugins"))
    ExcludePlugins = cfg.getParameter("exclude_plugins");

  // exclude_plugins
  if (cfg.hasParameter("exclude_payload"))
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

  // log_level
  if(cfg.hasParameter("loglevel")){
    if(!setLogLevel(cfg.getParameter("loglevel"))){
      ERROR("invalid log level specified\n");
      ret = -1;
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

  if (cfg.hasParameter("100rel")) {
    string rel100s = cfg.getParameter("100rel");
    if (rel100s == "disabled" || rel100s == "off") {
      rel100 = REL100_DISABLED;
    } else if (rel100s == "supported") {
      rel100 = REL100_SUPPORTED;
    } else if (rel100s == "require") {
      rel100 = REL100_REQUIRE;
    } else {
      ERROR("unknown setting for '100rel' config option: '%s'.\n",
	    rel100s.c_str());
      ret = -1;
    }
  }

  INFO("100rel: %d.\n", AmConfig::rel100);

  return ret;
}	

static int readInterface(AmConfigReader& cfg, const string& i_name)
{
  int ret=0;
  AmConfig::IP_interface intf;

  intf.LocalSIPIP   = "";
  intf.LocalSIPPort = 5060;
  intf.LocalIP      = "";
  intf.PublicIP     = "";
  intf.RtpLowPort   = RTP_LOWPORT;
  intf.RtpHighPort  = RTP_HIGHPORT;

  string suffix;
  if(!i_name.empty())
    suffix = "_" + i_name;

  // listen, sip_ip, sip_port, and media_ip
  if(cfg.hasParameter("sip_ip" + suffix)) {
    intf.LocalSIPIP = cfg.getParameter("sip_ip" + suffix);
  }
  else if(!suffix.empty()) {
    ERROR("sip_ip%s parameter is required",suffix.c_str());
    ret = -1;
  }

  if(cfg.hasParameter("sip_port" + suffix)){
    string sip_port_str = cfg.getParameter("sip_port" + suffix);
    if(sscanf(sip_port_str.c_str(),"%u",
	      &(intf.LocalSIPPort)) != 1){
      ERROR("sip_port%s: invalid sip port specified (%s)\n",
	    suffix.c_str(),
	    sip_port_str.c_str());
      ret = -1;
    }
  }

  if(cfg.hasParameter("media_ip" + suffix)) {
    intf.LocalIP = cfg.getParameter("media_ip" + suffix);
  }
  else if(!suffix.empty()) {
    ERROR("media_ip%s parameter is required",suffix.c_str());
    ret = -1;
  }

  // public_ip
  if(cfg.hasParameter("public_ip" + suffix)){
    string p_ip = cfg.getParameter("public_ip" + suffix);
    DBG("Setting public_ip%s parameter to %s.\n", suffix.c_str(), p_ip.c_str());
    intf.PublicIP = p_ip;
  }
  //else {
  //  DBG("Config file has no public_ip%s parameter.",suffix.c_str());
  //}

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

  AmConfig::Ifs.push_back(intf);
  AmConfig::LocalSIPIP2If.insert(std::make_pair(intf.LocalSIPIP,
						AmConfig::Ifs.size()-1));

  return ret;
}

static int readInterfaces(AmConfigReader& cfg)
{
  int ret = 0;

  AmConfig::Ifs.clear();

  // read default params first
  if(readInterface(cfg,"") < 0) {
    return -1;
  }
  
  vector<string> if_names;
  if(cfg.hasParameter("additional_interfaces")) {
    string ifs_str = cfg.getParameter("additional_interfaces");
    if(!ifs_str.empty())
      if_names = explode(ifs_str,",");
  }

  for(vector<string>::iterator it = if_names.begin();
      it != if_names.end(); it++) {

    if(readInterface(cfg,*it) < 0){
      ret = -1;
    }
    
    if(AmConfig::Ifs.size() > 0)
      AmConfig::If_names[*it] = AmConfig::Ifs.size()-1;
  }

  //debug
  if(ret != -1) {
    for(map<string,unsigned short>::iterator it = AmConfig::If_names.begin();
	it != AmConfig::If_names.end(); it++) {

      DBG("BEGIN: interface: '%s'",it->first.c_str());

      AmConfig::IP_interface& it_ref = AmConfig::Ifs[it->second];
      DBG("\tLocalIP='%s'",it_ref.LocalIP.c_str());
      DBG("\tPublicIP='%s'",it_ref.PublicIP.c_str());
      DBG("\tLocalSIPIP='%s'",it_ref.LocalSIPIP.c_str());
      DBG("\tLocalSIPPort=%u",it_ref.LocalSIPPort);
      DBG("\tRtpLowPort=%u",it_ref.RtpLowPort);
      DBG("\tRtpHighPort=%u",it_ref.RtpHighPort);
    }
  }

  return ret;
}

