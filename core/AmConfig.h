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
/** @file AmConfig.h */
#ifndef _AmConfig_h_
#define _AmConfig_h_

#include "AmSdp.h"
#include "AmDtmfDetector.h"

#include <string>
using std::string;
#include <utility>

#include <sys/types.h>
#include <regex.h>


/**
 * \brief holds the current configuration.
 *
 * This Structure holds the current configuration.
 */
struct AmConfig
{
  /** Name of the main configuration file. */
  static string ConfigurationFile;
  /** Path to the plug-in configuration files. */
  static string ModConfigPath;
  /** Path where the plug-ins are. */
  static string PlugInPath;
  /** semicolon separated list of plugins to load */
  static string LoadPlugins;
  /** semicolon separated list of plugins to exclude from loading */
  static string ExcludePlugins;
  /** semicolon separated list of payloads to exclude from loading */
  static string ExcludePayloads;  
  //static unsigned int MaxRecordTime;
  /** run the programm in daemon mode? */
  static int DaemonMode;
  
  /** local IP for SDP media advertising */
  static string LocalIP;
  
  /** public IP for SDP media advertising; we actually
   *  bind to local IP, but advertise public IP. */ 
  static string PublicIP;
  
  /** Separator character for uri application prefix (ex: voicemail+jiri@iptel.org) */
  static string PrefixSep;
  /** Lowest local RTP port */
  static int RtpLowPort;
  /** Highest local RTP port */
  static int RtpHighPort;
  /** number of session scheduler threads */
  static int MediaProcessorThreads;
  /** the interface SIP requests are sent from - needed for registrar_client */
  static string LocalSIPIP;
  /** the port SIP requests are sent from - optional (default 5060) */
  static int LocalSIPPort;
  /** Outbound Proxy (optional, outgoing calls only) */
  static string OutboundProxy;
  /** Server/User-Agent header (optional) */
  static string Signature;
  /** If 200 OK reply should be limited to preferred codec only */
  static bool SingleCodecInOK;
  static vector <string> CodecOrder;
  
  enum ApplicationSelector {
    App_RURIUSER,
    App_RURIPARAM,
    App_APPHDR,
    App_MAPPING,
    App_SPECIFIED
  };
  
  /** "application" config value */ 
  static string Application;
  /** type of application selection (parsed from Application) */
  static ApplicationSelector AppSelect;

  /* this is regex->application mapping is used if  App_MAPPING */
  typedef vector<std::pair<regex_t, string> > AppMappingVector;
  static AppMappingVector AppMapping; 

  static unsigned int SessionLimit;
  static unsigned int SessionLimitErrCode;
  static string SessionLimitErrReason;

  static unsigned int OptionsSessionLimit;
  static unsigned int OptionsSessionLimitErrCode;
  static string OptionsSessionLimitErrReason;

  /** Time of no RTP after which Session is regarded as dead, 0 for no Timeout */
  static unsigned int DeadRtpTime;

  /** Ignore RTP Extension headers? */
  static bool IgnoreRTPXHdrs;

  static Dtmf::InbandDetectorType DefaultDTMFDetector;

  static bool IgnoreSIGCHLD;

  static bool LogSessions;

  static bool LogEvents;

  static int UnhandledReplyLoglevel;

  /** Init function. Resolves SMTP server address. */
  static int init();

  /** Read global configuration file and insert values. Maybe overwritten by
   * command line arguments */
  static int readConfiguration();

  /* following setters are used to fill config from config file */  
	
  /** Setter for SIP Port, returns 0 on invalid value */
  static int setSIPPort(const string& port);  
  /** Setter for SmtpServer Port, returns 0 on invalid value */
  static int setSmtpPort(const string& port);
  /** Setter for RtpLowPort, returns 0 on invalid value */
  static int setRtpLowPort(const string& port);
  /** Setter for RtpHighPort, returns 0 on invalid value */
  static int setRtpHighPort(const string& port);
  /** Setter for Loglevel, returns 0 on invalid value */
  static int setLoglevel(const string& level);
  /** Setter for parameter fork, returns 0 on invalid value */
  static int setFork(const string& fork);
  /** Setter for parameter stderr, returns 0 on invalid value */
  static int setStderr(const string& s);
  /** Setter for parameter MediaProcessorThreads, returns 0 on invalid value */
  static int setMediaProcessorThreads(const string& th);
  /** Setter for parameter DeadRtpTime, returns 0 on invalid value */
  static int setDeadRtpTime(const string& drt);
};

#endif

// Local Variables:
// mode:C++
// End:
