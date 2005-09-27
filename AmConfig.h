/*
 * $Id: AmConfig.h,v 1.12 2004/08/13 13:50:38 rco Exp $
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

/**
 * This Structure holds the current configuration.
 */
struct AmConfig
{
    /** Name of the main configuration file. */
    static string ConfigurationFile;
    /** Name of our FIFO file. */
    static string FifoName;
    /** Name of the Ser FIFO file. */
    static string SerFIFO;
    /** Name of our unix socket file. */
    static string SocketName;
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
    //static unsigned int MaxRecordTime;
    /** run the programm in daemon mode? */
    static int DaemonMode;
    /** local IP for SDP media advertising */
    static string LocalIP;
    /** Separator character for uri application prefix (ex: voicemail+jiri@iptel.org) */
    static char PrefixSep;
    /** Lowest local RTP port */
    static short RtpLowPort;
    /** Highest local RTP port */
    static short RtpHighPort;
    /** Init function. Resolves SMTP server address. */
    static int init();

    /** Read global configuration file and insert values. Maybe overwritten by
     * command line arguments */
    static int readConfiguration();

    /* following setters are used to fill config from config file */    
    /** Setter for SmtpServer Port, returns 0 on invalid value */
    static int setSmtpPort(char* port);
    /** Setter for RtpLowPort, returns 0 on invalid value */
    static int setRtpLowPort(char* port);
    /** Setter for RtpHighPort, returns 0 on invalid value */
    static int setRtpHighPort(char* port);
    /** Setter for Loglevel, returns 0 on invalid value */
    static int setLoglevel(char* level);
    /** Setter for parameter fork, returns 0 on invalid value */
    static int setFork(char* fork);
    /** Setter for parameter stderr, returns 0 on invalid value */
    static int setStderr(char* s);
};

#endif

// Local Variables:
// mode:C++
// End:
