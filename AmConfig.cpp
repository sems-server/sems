/*
 * $Id: AmConfig.cpp,v 1.18 2004/08/13 13:50:38 rco Exp $
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
#include "AmConfig.h"
#include "sems.h"
#include "log.h"
#include "SemsConfiguration.h"

string       AmConfig::ConfigurationFile    = CONFIG_FILE;
string       AmConfig::FifoName             = ""; //FIFO_NAME;
string       AmConfig::SerFIFO              = ""; //SER_FIFO;
string       AmConfig::SocketName           = "";
string       AmConfig::SerSocketName        = "";
string       AmConfig::SendMethod           = SEND_METHOD;
string       AmConfig::SmtpServerAddress    = SMTP_ADDRESS_IP;
unsigned int AmConfig::SmtpServerPort       = SMTP_PORT;
string       AmConfig::PlugInPath           = PLUG_IN_PATH;
int          AmConfig::DaemonMode           = DEFAULT_DAEMON_MODE;
string       AmConfig::LocalIP              = "";
char         AmConfig::PrefixSep            = PREFIX_SEPARATOR;
short        AmConfig::RtpLowPort           = RTP_LOWPORT;
short        AmConfig::RtpHighPort          = RTP_HIGHPORT;

int AmConfig::setSmtpPort(char *port) 
{
    if(sscanf(port,"%u",&AmConfig::SmtpServerPort) != 1) {
	return 0;
    }
    return 1;
}

int AmConfig::setRtpLowPort(char* port)
{
    if(sscanf(port,"%hu",&AmConfig::RtpLowPort) != 1) {
	return 0;
    }
    return 1;
}

int AmConfig::setRtpHighPort(char* port)
{
    if(sscanf(port,"%hu",&AmConfig::RtpHighPort) != 1) {
	return 0;
    }
    return 1;
}

int AmConfig::setLoglevel(char *ll) {
    if(sscanf(ll,"%u",&log_level) != 1) {
	return 0;
    }
    return 1;
}

int AmConfig::setFork(char *fork) {
	if ( strcasecmp(fork, "yes") == 0 ) {
		DaemonMode = 1;
	} else if ( strcasecmp(fork, "no") == 0 ) {
		DaemonMode = 0;
	} else {
		return 0;
	}	
	return 1;
}		

int AmConfig::setStderr(char *s) {
	if ( strcasecmp(s, "yes") == 0 ) {
		log_stderr = 1;
		AmConfig::DaemonMode = 0;
	} else if ( strcasecmp(s, "no") == 0 ) {
		log_stderr = 0;
	} else {
		return 0;
	}	
	return 1;
}		

int AmConfig::readConfiguration()
{
	// take values from global configuration file
	// they will be overwritten by command line args
	char* temp;
	// smtp_server
	temp = semsConfig.getValueForKey("smtp_server");
	if (temp) AmConfig::SmtpServerAddress = temp;
	// smtp_port
	temp = semsConfig.getValueForKey("smtp_port");
	if (temp) {
		if (!AmConfig::setSmtpPort(temp)) {
			semsConfig.reportConfigError("smtp_port", 
						     "invalid smtp port specified", 
						     true);
			return 0;
		}	
	}	
	// local_ip
 	temp = semsConfig.getValueForKey("local_ip");
 	if (temp) AmConfig::LocalIP = temp;
	// fifo_name
	temp = semsConfig.getValueForKey("fifo_name");
	if (temp) AmConfig::FifoName = temp;
	// ser_fifo_name
	temp = semsConfig.getValueForKey("ser_fifo_name");
	if (temp) AmConfig::SerFIFO = temp;
	// socket_name
	temp = semsConfig.getValueForKey("socket_name");
	if (temp) AmConfig::SocketName = temp;
	// ser_fifo_name
	temp = semsConfig.getValueForKey("ser_socket_name");
	if (temp) AmConfig::SerSocketName = temp;
	// send_method
	temp = semsConfig.getValueForKey("send_method");
	if (temp) AmConfig::SendMethod = temp;
	if ( AmConfig::SendMethod != "fifo" && 
	     AmConfig::SendMethod != "unix_socket" ){
	    semsConfig.reportConfigError("send_method",
					 "invalid send_method specified "
					 "(valid values are: fifo, unix_socket).",
					 true);
	    return 0;
	}
	// plugin_path
	temp = semsConfig.getValueForKey("plugin_path");
	if (temp) AmConfig::PlugInPath = temp;
	// log_level
	temp = semsConfig.getValueForKey("loglevel");
	if (temp) {
		if(!AmConfig::setLoglevel(temp)){
			semsConfig.reportConfigError("loglevel", 
						     "invalid log level specified", 
						     true);
			return 0;
		}	
	}	
	// fork 
	temp = semsConfig.getValueForKey("fork");
	if (temp) {
		if (!AmConfig::setFork(temp)) {
			semsConfig.reportConfigError("fork", 
						     "invalid value specified,"
						     " valid are only yes or no", 
						     true);
			return 0;
		}	
	}	
	// stderr 
	temp = semsConfig.getValueForKey("stderr");
	if (temp) {
		if (!AmConfig::setStderr(temp)) {
			semsConfig.reportConfigError("stderr", 
						     "invalid value specified,"
						     " valid are only yes or no", 
						     true);
			return 0;
		}	
	}	
	// user_prefix_separator
	temp = semsConfig.getValueForKey("user_prefix_separator");
	if(temp) AmConfig::PrefixSep = *temp;

	// rtp_low_port
	temp = semsConfig.getValueForKey("rtp_low_port");
	if (temp) {
		if (!AmConfig::setRtpLowPort(temp)) {
			semsConfig.reportConfigError("rtp_low_port", 
						     "invalid rtp low"
						     " port specified", 
						     true);
			return 0;
		}	
	}	

	// rtp_high_port
	temp = semsConfig.getValueForKey("rtp_high_port");
	if (temp) {
		if (!AmConfig::setRtpHighPort(temp)) {
			semsConfig.reportConfigError("rtp_high_port", 
						     "invalid rtp high"
						     " port specified", true);
			return 0;
		}	
	}	

	return 1;
}	

// static string lookup_addr(const string& addr);

int AmConfig::init()
{
    // Replace smtp server name through its IP
//     string ip = lookup_addr(SmtpServerAddress);
//     if(ip.empty()){
// 	ERROR( "could not resolv smtp host '%s:%i'\n",
// 	       SmtpServerAddress.c_str(),SmtpServerPort );
// 	return -1;
//     }
//     SmtpServerAddress = lookup_addr(SmtpServerAddress);

    if(PlugInPath.empty())
	PlugInPath = string(getenv("PWD")) + "/plug-in";

    return 0;
}

// static string lookup_addr(const string& addr)
// {
//     struct hostent * he = gethostbyname(addr.c_str());
//     if(he && he->h_addr_list && he->h_addr_list[0]){
// 	struct in_addr* addr = (struct in_addr*)he->h_addr_list[0];
// 	return inet_ntoa(*addr);
//     }
//     return "";
// }


