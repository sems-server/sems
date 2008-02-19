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

#include <fstream>

string       AmConfig::ConfigurationFile       = CONFIG_FILE;
string       AmConfig::ModConfigPath           = MOD_CFG_PATH;
string       AmConfig::PlugInPath              = PLUG_IN_PATH;
string       AmConfig::LoadPlugins             = "";
string       AmConfig::ExcludePlugins          = "";
string       AmConfig::ExcludePayloads         = "";
int          AmConfig::DaemonMode              = DEFAULT_DAEMON_MODE;
string       AmConfig::LocalIP                 = "";
string       AmConfig::PrefixSep               = PREFIX_SEPARATOR;
int          AmConfig::RtpLowPort              = RTP_LOWPORT;
int          AmConfig::RtpHighPort             = RTP_HIGHPORT;
int          AmConfig::MediaProcessorThreads   = NUM_MEDIA_PROCESSORS;
int          AmConfig::LocalSIPPort            = 5060;
string       AmConfig::LocalSIPIP              = "";
string       AmConfig::OutboundProxy           = "";
string       AmConfig::Signature               = "";
bool	     AmConfig::SingleCodecInOK	       = false;
unsigned int AmConfig::DeadRtpTime             = DEAD_RTP_TIME;
bool         AmConfig::IgnoreRTPXHdrs          = false;
string       AmConfig::Application             = "";
AmConfig::ApplicationSelector AmConfig::AppSelect        = AmConfig::App_SPECIFIED;
AmConfig::AppMappingVector AmConfig::AppMapping;

vector <string> AmConfig::CodecOrder;

Dtmf::InbandDetectorType 
AmConfig::DefaultDTMFDetector     = Dtmf::SEMSInternal;

AmSessionTimerConfig AmConfig::defaultSessionTimerConfig;

int AmConfig::setSIPPort(const string& port) 
{
  if(sscanf(port.c_str(),"%u",&AmConfig::LocalSIPPort) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setRtpLowPort(const string& port)
{
  if(sscanf(port.c_str(),"%i",&AmConfig::RtpLowPort) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setRtpHighPort(const string& port)
{
  if(sscanf(port.c_str(),"%i",&AmConfig::RtpHighPort) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setLoglevel(const string& ll) {
    
  if(sscanf(ll.c_str(),"%u",&log_level) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::setFork(const string& fork) {
  if ( strcasecmp(fork.c_str(), "yes") == 0 ) {
    DaemonMode = 1;
  } else if ( strcasecmp(fork.c_str(), "no") == 0 ) {
    DaemonMode = 0;
  } else {
    return 0;
  }	
  return 1;
}		

int AmConfig::setStderr(const string& s) {
  if ( strcasecmp(s.c_str(), "yes") == 0 ) {
    log_stderr = 1;
    AmConfig::DaemonMode = 0;
  } else if ( strcasecmp(s.c_str(), "no") == 0 ) {
    log_stderr = 0;
  } else {
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

int AmConfig::setDeadRtpTime(const string& drt)
{
  if(sscanf(drt.c_str(),"%u",&AmConfig::DeadRtpTime) != 1) {
    return 0;
  }
  return 1;
}

int AmConfig::readConfiguration()
{
  AmConfigReader cfg;

  if(cfg.loadFile(ConfigurationFile.c_str())){
    ERROR("while loading main configuration file\n");
    return -1;
  }
       
  // take values from global configuration file
  // they will be overwritten by command line args

  // plugin_config_path
  ModConfigPath = cfg.getParameter("plugin_config_path",ModConfigPath);

  if(!ModConfigPath.empty() && (ModConfigPath[ModConfigPath.length()-1] != '/'))
    ModConfigPath += '/';

  // local_ip
  LocalIP = cfg.getParameter("listen");

  if(cfg.hasParameter("sip_port")){
    if(!setSIPPort(cfg.getParameter("sip_port").c_str())){
      ERROR("invalid sip port specified\n");
      return -1;
    }		
  }

  if(cfg.hasParameter("sip_ip"))
    LocalSIPIP = cfg.getParameter("sip_ip");
  
  // outbound_proxy
  OutboundProxy = cfg.getParameter("outbound_proxy");
  
  // plugin_path
  PlugInPath = cfg.getParameter("plugin_path");

  // load_plugins
  LoadPlugins = cfg.getParameter("load_plugins");

  // exclude_plugins
  ExcludePlugins = cfg.getParameter("exclude_plugins");

  // exclude_plugins
  ExcludePayloads = cfg.getParameter("exclude_payloads");

  // user_agent
  if (cfg.getParameter("use_default_signature")=="yes")
    Signature = DEFAULT_SIGNATURE;
  else 
    Signature = cfg.getParameter("signature");

  // log_level
  if(cfg.hasParameter("loglevel")){
    if(!setLoglevel(cfg.getParameter("loglevel"))){
      ERROR("invalid log level specified\n");
      return -1;
    }
  }

  Application  = cfg.getParameter("application");

  if (Application == "$(ruri.user)") {
    AppSelect = App_RURIUSER;
  } else if (Application == "$(ruri.param)") {
    AppSelect = App_RURIPARAM;
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
      if (regcomp(&app_re, re_v[0].c_str(), REG_NOSUB)) {
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

  // fork 
  if(cfg.hasParameter("fork")){
    if(!setFork(cfg.getParameter("fork"))){
      ERROR("invalid fork value specified,"
	    " valid are only yes or no\n");
      return -1;
    }
  }

  // stderr 
  if(cfg.hasParameter("stderr")){
    if(!setStderr(cfg.getParameter("stderr"))){
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

  if(cfg.hasParameter("media_processor_threads")){
    if(!setMediaProcessorThreads(cfg.getParameter("media_processor_threads"))){
      ERROR("invalid media_processor_threads value specified");
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

  return defaultSessionTimerConfig.readFromConfig(cfg);
}	

int AmConfig::init()
{
  return 0;
}

AmSessionTimerConfig::AmSessionTimerConfig()
  : EnableSessionTimer(DEFAULT_ENABLE_SESSION_TIMER), 
    SessionExpires(SESSION_EXPIRES), 
    MinimumTimer(MINIMUM_TIMER)
{

}
AmSessionTimerConfig::~AmSessionTimerConfig() 
{
}

int AmSessionTimerConfig::readFromConfig(AmConfigReader& cfg)
{
  // enable_session_timer
  if(cfg.hasParameter("enable_session_timer")){
    if(!setEnableSessionTimer(cfg.getParameter("enable_session_timer"))){
      ERROR("invalid enable_session_timer specified\n");
      return -1;
    }
  }

  // session_expires
  if(cfg.hasParameter("session_expires")){
    if(!setSessionExpires(cfg.getParameter("session_expires"))){
      ERROR("invalid session_expires specified\n");
      return -1;
    }
  }

  // minimum_timer
  if(cfg.hasParameter("minimum_timer")){
    if(!setMinimumTimer(cfg.getParameter("minimum_timer"))){
      ERROR("invalid minimum_timer specified\n");
      return -1;
    }
  }
  return 0;
}

int AmSessionTimerConfig::setEnableSessionTimer(const string& enable) {
  if ( strcasecmp(enable.c_str(), "yes") == 0 ) {
    EnableSessionTimer = 1;
  } else if ( strcasecmp(enable.c_str(), "no") == 0 ) {
    EnableSessionTimer = 0;
  } else {
    return 0;
  }	
  return 1;
}		

int AmSessionTimerConfig::setSessionExpires(const string& se) {
  if(sscanf(se.c_str(),"%u",&SessionExpires) != 1) {
    return 0;
  }
  DBG("setSessionExpires(%i)\n",SessionExpires);
  return 1;
} 

int AmSessionTimerConfig::setMinimumTimer(const string& minse) {
  if(sscanf(minse.c_str(),"%u",&MinimumTimer) != 1) {
    return 0;
  }
  DBG("setMinimumTimer(%i)\n",MinimumTimer);
  return 1;
}
