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

#ifndef _AmConfig_h_
#define _AmConfig_h_

#include "AmSdp.h"

#include <string>
#include <map>
using std::string;
using std::map;

class AmSessionTimerConfig;

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
  /** Name of our unix socket file for requests. */
  static string SocketName;
  /** Name of our unix socket file for replies. */
  static string ReplySocketName;
  /** Name of the Ser unix socket file. */
  static string SerSocketName;
  /** Send method: 'fifo' or 'unix_socket' */
  static string SendMethod;
  /** After server start, IP of the SMTP server. */
  static string SmtpServerAddress;
  /** SMTP server port. */
  static unsigned int SmtpServerPort;
  /** Path where the plug-ins are. */
  static string PlugInPath;
  /** semicolon separated list of plugins to load */
  static string LoadPlugins;
  //static unsigned int MaxRecordTime;
  /** run the programm in daemon mode? */
  static int DaemonMode;
  /** local IP for SDP media advertising */
  static string LocalIP;
  /** Separator character for uri application prefix (ex: voicemail+jiri@iptel.org) */
  static string PrefixSep;
  /** Lowest local RTP port */
  static int RtpLowPort;
  /** Highest local RTP port */
  static int RtpHighPort;
  /* Session Timer: -ssa */
  static AmSessionTimerConfig defaultSessionTimerConfig;
  /** number of session scheduler threads */
  static int MediaProcessorThreads;
  /** the interface SIP requests are sent from - needed for registrar_client */
  static string LocalSIPIP;
  /** the port SIP requests are sent from - optional (default 5060) */
  static int LocalSIPPort;
  /** Server/User-Agent header (optional) */
  static string Signature;

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
};

class AmConfigReader;
/** \brief config for the session timer */
class AmSessionTimerConfig {
  /** Session Timer: enable? */
  int EnableSessionTimer;
  /** Session Timer: Desired Session-Expires */
  unsigned int SessionExpires;
  /** Session Timer: Minimum Session-Expires */
  unsigned int MinimumTimer;
    
public:
  AmSessionTimerConfig();
  ~AmSessionTimerConfig();
  

  /** Session Timer: Enable Session Timer?
      returns 0 on invalid value */
  int setEnableSessionTimer(const string& enable);
  /** Session Timer: Setter for Desired Session-Expires, 
      returns 0 on invalid value */
  int setSessionExpires(const string& se);
  /** Session Timer: Setter for Minimum Session-Expires, 
      returns 0 on invalid value */
  int setMinimumTimer(const string& minse);

  bool getEnableSessionTimer() { return EnableSessionTimer; }
  unsigned int getSessionExpires() { return SessionExpires; }
  unsigned int getMinimumTimer() { return MinimumTimer; }

  int readFromConfig(AmConfigReader& cfg);
};

#endif

// Local Variables:
// mode:C++
// End:
