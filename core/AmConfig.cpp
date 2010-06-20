/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
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

#include <fstream>
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

#ifndef DISABLE_DAEMON_MODE
bool         AmConfig::DaemonMode              = DEFAULT_DAEMON_MODE;
string       AmConfig::DaemonPidFile           = DEFAULT_DAEMON_PID_FILE;
string       AmConfig::DaemonUid               = DEFAULT_DAEMON_UID;
string       AmConfig::DaemonGid               = DEFAULT_DAEMON_GID;
#endif

string       AmConfig::LocalIP                 = "";
string       AmConfig::PublicIP                = "";
string       AmConfig::PrefixSep               = PREFIX_SEPARATOR;
int          AmConfig::RtpLowPort              = RTP_LOWPORT;
int          AmConfig::RtpHighPort             = RTP_HIGHPORT;
int          AmConfig::SessionProcessorThreads = NUM_SESSION_PROCESSORS;
int          AmConfig::MediaProcessorThreads   = NUM_MEDIA_PROCESSORS;
int          AmConfig::SIPServerThreads        = NUM_SIP_SERVERS;
int          AmConfig::LocalSIPPort            = 5060;
string       AmConfig::LocalSIPIP              = "";
string       AmConfig::OutboundProxy           = "";
bool         AmConfig::ForceOutboundProxy      = false;
string       AmConfig::Signature               = "";
unsigned int AmConfig::MaxForwards             = MAX_FORWARDS;
bool	     AmConfig::SingleCodecInOK	       = false;
unsigned int AmConfig::DeadRtpTime             = DEAD_RTP_TIME;
bool         AmConfig::IgnoreRTPXHdrs          = false;
string       AmConfig::Application             = "";
AmConfig::ApplicationSelector AmConfig::AppSelect        = AmConfig::App_SPECIFIED;
AmConfig::AppMappingVector AmConfig::AppMapping;
bool         AmConfig::LogSessions             = false;
bool         AmConfig::LogEvents               = false;
int          AmConfig::UnhandledReplyLoglevel  = 0;

unsigned int AmConfig::SessionLimit            = 0;
unsigned int AmConfig::SessionLimitErrCode     = 503;
string       AmConfig::SessionLimitErrReason   = "Server overload";

unsigned int AmConfig::OptionsSessionLimit            = 0;
unsigned int AmConfig::OptionsSessionLimitErrCode     = 503;
string       AmConfig::OptionsSessionLimitErrReason   = "Server overload";

unsigned char AmConfig::rel100                 = REL100_SUPPORTED;

vector <string> AmConfig::CodecOrder;

Dtmf::InbandDetectorType 
AmConfig::DefaultDTMFDetector     = Dtmf::SEMSInternal;
bool AmConfig::IgnoreSIGCHLD      = true;

int AmConfig::setSIPPort(const string& port) 
{
  if(sscanf(port.c_str(),"%u",&LocalSIPPort) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setRtpLowPort(const string& port)
{
  if(sscanf(port.c_str(),"%i",&RtpLowPort) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setRtpHighPort(const string& port)
{
  if(sscanf(port.c_str(),"%i",&RtpHighPort) != 1) {
    return 0;
  }
  return 1;
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

  if(cfg.loadFile(AmConfig::ConfigurationFile.c_str())){
    ERROR("while loading main configuration file\n");
    return -1;
  }
       
  // take values from global configuration file
  // they will be overwritten by command line args

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

  // listen, sip_ip, sip_port, and media_ip
  if(cfg.hasParameter("sip_ip")) {
    LocalSIPIP = cfg.getParameter("sip_ip");
  }
  if(cfg.hasParameter("sip_port")){
    if(!setSIPPort(cfg.getParameter("sip_port").c_str())){
      ERROR("invalid sip port specified\n");
      return -1;
    }		
  }
  if(cfg.hasParameter("media_ip")) {
    LocalIP = cfg.getParameter("media_ip");
  }

  // public_ip
  if(cfg.hasParameter("public_ip")){
    string p_ip = cfg.getParameter("public_ip");
    DBG("Setting public_ip parameter to %s.\n", p_ip.c_str());
    PublicIP = p_ip;
  }
  else {
    DBG("Config file has no public_ip parameter.");
  }
  
  // outbound_proxy
  if (cfg.hasParameter("outbound_proxy"))
    OutboundProxy = cfg.getParameter("outbound_proxy");

  // force_outbound_proxy
  if(cfg.hasParameter("force_outbound_proxy")) {
    ForceOutboundProxy = (cfg.getParameter("force_outbound_proxy") == "yes");
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
      return -1;
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
    std::ifstream appcfg(appcfg_fname.c_str());
    if (!appcfg.good()) {
      ERROR("could not load application mapping  file at '%s'\n",
	    appcfg_fname.c_str());
      return -1;
    }

    while (!appcfg.eof()) {
      string entry;
      getline (appcfg,entry);
      if (!entry.length() || entry[0] == '#')
	continue;
      vector<string> re_v = explode(entry, "=>");
      if (re_v.size() != 2) {
	ERROR("Incorrect line '%s' in %s: expected format 'regexp=>app_name'\n",
	      entry.c_str(), appcfg_fname.c_str());
	return -1;
      }
      regex_t app_re;
      if (regcomp(&app_re, re_v[0].c_str(), REG_EXTENDED | REG_NOSUB)) {
	ERROR("compiling regex '%s' in %s.\n", 
	      re_v[0].c_str(), appcfg_fname.c_str());
	return -1;
      }
      DBG("adding application mapping '%s' => '%s'\n",
	  re_v[0].c_str(),re_v[1].c_str());
      AppMapping.push_back(make_pair(app_re, re_v[1]));
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
      return -1;
    }
  }

  // daemon (alias for fork)
  if(cfg.hasParameter("daemon")){
    if(!setDaemonMode(cfg.getParameter("daemon"))){
      ERROR("invalid daemon value specified,"
	    " valid are only yes or no\n");
      return -1;
    }
  }

  if(cfg.hasParameter("daemon_uid")){
    DaemonUid = cfg.getParameter("daemon_uid");
  }

  if(cfg.hasParameter("daemon_gid")){
    DaemonGid = cfg.getParameter("daemon_gid");
  }

#endif /* !DISABLE_DAEMON_MODE */

  // stderr 
  if(cfg.hasParameter("stderr")){
    if(!setLogStderr(cfg.getParameter("stderr"), false)){
      ERROR("invalid stderr value specified,"
	    " valid are only yes or no\n");
      return -1;
    }
  }

  // user_prefix_separator
  PrefixSep = cfg.getParameter("user_prefix_separator",PrefixSep);

  // rtp_low_port
  if(cfg.hasParameter("rtp_low_port")){
    if(!setRtpLowPort(cfg.getParameter("rtp_low_port"))){
      ERROR("invalid rtp low port specified\n");
      return -1;
    }
  }

  // rtp_high_port
  if(cfg.hasParameter("rtp_high_port")){
    if(!setRtpHighPort(cfg.getParameter("rtp_high_port"))){
      ERROR("invalid rtp high port specified\n");
      return -1;
    }
  }

  if(cfg.hasParameter("session_processor_threads")){
#ifdef SESSION_THREADPOOL
    if(!setSessionProcessorThreads(cfg.getParameter("session_processor_threads"))){
      ERROR("invalid session_processor_threads value specified\n");
      return -1;
    }
    if (SessionProcessorThreads<1) {
      ERROR("invalid session_processor_threads value specified."
	    " need at least one thread\n");
      return -1;
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
      return -1;
    }
  }

  if(cfg.hasParameter("sip_server_threads")){
    if(!setSIPServerThreads(cfg.getParameter("sip_server_threads"))){
      ERROR("invalid sip_server_threads value specified");
      return -1;
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
      return -1;
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
      ERROR("unknown setting for '100rel' config option.\n");
      return -1;
    }
  }
  INFO("100rel: %d.\n", AmConfig::rel100);

  return 0;
}	
