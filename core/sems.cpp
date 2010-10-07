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

#include "sems.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"
#include "AmMediaProcessor.h"
#include "AmRtpReceiver.h"
#include "AmEventDispatcher.h"

#ifdef WITH_ZRTP
# include "AmZRTP.h"
#endif

#include "SipCtrlInterface.h"

#include "log.h"

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include <grp.h>
#include <pwd.h>

#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
using std::string;
using std::make_pair;


const char* progname = NULL;    /**< Program name (actually argv[0])*/
int main_pid = 0;               /**< Main process PID */

/** SIP stack (controller interface) */
static SipCtrlInterface sip_ctrl;


static void print_usage(bool short_=false)
{
  if (short_) {
    printf("Usage: %s [OPTIONS]\n"
           "Try `%s -h' for more information.\n",
           progname, progname);
  }
  else {
    printf(
        DEFAULT_SIGNATURE "\n"
        "Usage: %s [OPTIONS]\n"
        "Available options:\n"
        "    -f <file>       Set configuration file\n"
        "    -x <dir>        Set path for plug-ins\n"
        "    -d <device/ip>  Set network device (or IP address) for media advertising\n"
#ifndef DISABLE_DAEMON_MODE
        "    -E              Enable debug mode (do not daemonize, log to stderr).\n"
        "    -P <file>       Set PID file\n"
        "    -u <uid>        Set user ID\n"
        "    -g <gid>        Set group ID\n"
#else
        "    -E              Enable debug mode (log to stderr)\n"
#endif
        "    -D <level>      Set log level (0=error, 1=warning, 2=info, 3=debug; default=%d)\n"
        "    -v              Print version\n"
        "    -h              Print this help\n",
        progname, AmConfig::LogLevel
    );
  }
}

/* Note: The function should not use log because it is called before logging is initialized. */
static bool parse_args(int argc, char* argv[], std::map<char,string>& args)
{
#ifndef DISABLE_DAEMON_MODE
    static const char* opts = ":hvEf:x:d:D:u:g:P:";
#else    
    static const char* opts = ":hvEf:x:d:D:";
#endif    

    opterr = 0;
    
    while (true) {
	int c = getopt(argc, argv, opts);
	switch (c) {
	case -1:
	    return true;
	    
	case ':':
	    fprintf(stderr, "%s: missing argument for option '-%c'\n", progname, optopt);
	    return false;
	    
	case '?':
	    fprintf(stderr, "%s: unknown option '-%c'\n", progname, optopt);
	    return false;
	    
	default:
	    args[c] = (optarg ? optarg : "yes");
	}
    }
}

/* Note: The function should not use logging because it is called before
   the logging is initialized. */
static bool apply_args(std::map<char,string>& args)
{
  for(std::map<char,string>::iterator it = args.begin();
      it != args.end(); ++it){

    switch( it->first ){
    case 'd':
      AmConfig::LocalIP = it->second;
      break;

    case 'D':
      if (!AmConfig::setLogLevel(it->second)) {
        fprintf(stderr, "%s: invalid log level: %s\n", progname, it->second.c_str());
        return false;
      }
      break;

    case 'E':
#ifndef DISABLE_DAEMON_MODE
     AmConfig::DaemonMode = false;
#endif
     if (!AmConfig::setLogStderr("yes")) {
       return false;
     }
     break;

    case 'f':
      AmConfig::ConfigurationFile = it->second;
      break;

    case 'x':
      AmConfig::PlugInPath = it->second;
      break;

#ifndef DISABLE_DAEMON_MODE
    case 'P':
      AmConfig::DaemonPidFile = it->second;
      break;

    case 'u':
      AmConfig::DaemonUid = it->second;
      break;

    case 'g':
      AmConfig::DaemonGid = it->second;
      break;
#endif

    case 'h':
    case 'v':
    default:
      /* nothing to apply */
      break;
    }
  }

  return true;
}

/** Flag to mark the shutdown is in progress (in the main process) */
static AmCondition<bool> is_shutting_down(false);

static void signal_handler(int sig)
{
  if (sig == SIGUSR1 || sig == SIGUSR2) {
    DBG("brodcasting User event to %u sessions...\n",
	AmSession::getSessionNum());
    AmEventDispatcher::instance()->
      broadcast(new AmSystemEvent(sig == SIGUSR1? 
				  AmSystemEvent::User1 : AmSystemEvent::User2));
    return;
  }

  if (sig == SIGCHLD && AmConfig::IgnoreSIGCHLD) {
    return;
  }

  if (sig == SIGPIPE && AmConfig::IgnoreSIGPIPE) {
    return;
  }

  WARN("Signal %s (%d) received.\n", strsignal(sig), sig);

  if (sig == SIGHUP) {
    AmSessionContainer::instance()->broadcastShutdown();
    return;
  }

  if (main_pid == getpid()) {
    if(!is_shutting_down.get()) {
      is_shutting_down.set(true);

      INFO("Stopping SIP stack after signal\n");
      sip_ctrl.stop();
    }
  }
  else {
    /* exit other processes immediately */
    exit(0);
  }
}

int set_sighandler(void (*handler)(int))
{
  static int sigs[] = {
    SIGHUP, SIGPIPE, SIGINT, SIGTERM, SIGCHLD, SIGUSR1, SIGUSR2, 0
  };

  for (int* sig = sigs; *sig; sig++) {
    if (signal(*sig, handler) == SIG_ERR ) {
      ERROR("Cannot install signal handler for %s.\n", strsignal(*sig));
      return -1;
    }
  }

  return 0;
}

#ifndef DISABLE_DAEMON_MODE

static int write_pid_file()
{
  FILE* fpid = fopen(AmConfig::DaemonPidFile.c_str(), "w");

  if (fpid) {
    string spid = int2str((int)getpid());
    fwrite(spid.c_str(), spid.length(), 1, fpid);
    fclose(fpid);
    return 0;
  }
  else {
    ERROR("Cannot write PID file '%s': %s.\n",
        AmConfig::DaemonPidFile.c_str(), strerror(errno));
  }

  return -1;
}

#endif /* !DISABLE_DAEMON_MODE */


/** Get the list of network interfaces with the associated PF_INET addresses */
static bool getInterfaceList(int sd, std::vector<std::pair<string,string> >& if_list)
{
  struct ifconf ifc;
  struct ifreq ifrs[MAX_NET_DEVICES];

  ifc.ifc_len = sizeof(struct ifreq) * MAX_NET_DEVICES;
  ifc.ifc_req = ifrs;
  memset(ifrs, 0, ifc.ifc_len);

  if(ioctl(sd, SIOCGIFCONF, &ifc)!=0){
    ERROR("getInterfaceList: ioctl: %s.\n", strerror(errno));
    return false;
  }

#if !defined(BSD44SOCKETS)
  int n_dev = ifc.ifc_len / sizeof(struct ifreq);
  for(int i=0; i<n_dev; i++){
    if(ifrs[i].ifr_addr.sa_family==PF_INET){
      struct sockaddr_in* sa = (struct sockaddr_in*)&ifrs[i].ifr_addr;

      // avoid dereferencing type-punned pointer below
      struct sockaddr_in sa4;
      memcpy(&sa4, sa, sizeof(struct sockaddr_in));
      if_list.push_back(make_pair((char*)ifrs[i].ifr_name,
                                  inet_ntoa(sa4.sin_addr)));
    }
  }
#else // defined(BSD44SOCKETS)
  struct ifreq* p_ifr = ifc.ifc_req;
  while((char*)p_ifr - (char*)ifc.ifc_req < ifc.ifc_len){

    if(p_ifr->ifr_addr.sa_family == PF_INET){
      struct sockaddr_in* sa = (struct sockaddr_in*)&p_ifr->ifr_addr;
      if_list.push_back(make_pair((const char*)p_ifr->ifr_name,
                                  inet_ntoa(sa->sin_addr)));
    }

    p_ifr = (struct ifreq*)(((char*)p_ifr) + IFNAMSIZ + p_ifr->ifr_addr.sa_len);
  }
#endif

  return true;
}

/** Get the PF_INET address associated with the network interface */
static string getLocalIP(const string& dev_name)
{
  string local_ip;
  struct ifreq ifr;
  std::vector<std::pair<string,string> > if_list;

#ifdef SUPPORT_IPV6
  struct sockaddr_storage ss;
  if(inet_aton_v6(dev_name.c_str(), &ss))
#else
    struct in_addr inp;
  if(inet_aton(dev_name.c_str(), &inp))
#endif
    {
      return dev_name;
    }

  int sd = socket(PF_INET, SOCK_DGRAM, 0);
  if(sd == -1){
    ERROR("socket: %s.\n", strerror(errno));
    goto error;
  }

  if(dev_name.empty()) {
    if (!getInterfaceList(sd, if_list)) {
      goto error;
    }
  }
  else {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, dev_name.c_str(), IFNAMSIZ-1);

    if(ioctl(sd, SIOCGIFADDR, &ifr)!=0){
      ERROR("ioctl(SIOCGIFADDR): %s.\n", strerror(errno));
      goto error;
    }

    if(ifr.ifr_addr.sa_family==PF_INET){
      struct sockaddr_in* sa = (struct sockaddr_in*)&ifr.ifr_addr;

      // avoid dereferencing type-punned pointer below
      struct sockaddr_in sa4;
      memcpy(&sa4, sa, sizeof(struct sockaddr_in));
      if_list.push_back(make_pair((char*)ifr.ifr_name,
				  inet_ntoa(sa4.sin_addr)));
    }
  }

  for( std::vector<std::pair<string,string> >::iterator it = if_list.begin();
       it != if_list.end(); ++it) {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, it->first.c_str(), IFNAMSIZ-1);

    if(ioctl(sd, SIOCGIFFLAGS, &ifr)!=0){
      ERROR("ioctl(SIOCGIFFLAGS): %s.\n", strerror(errno));
      goto error;
    }

    if( (ifr.ifr_flags & IFF_UP) &&
        (!dev_name.empty() || !(ifr.ifr_flags & IFF_LOOPBACK)) ) {
      local_ip = it->second;
      break;
    }
  }

  if(ifr.ifr_flags & IFF_LOOPBACK){
    WARN("Media advertising using loopback address!\n"
         "Try to use another network interface if your SEMS "
         "should be accessible from the rest of the world.\n");
  }

 error:
  close(sd);
  return local_ip;
}

/*
 * Main
 */
int main(int argc, char* argv[])
{
  int success = false;
  std::map<char,string> args;

  progname = strrchr(argv[0], '/');
  progname = (progname == NULL ? argv[0] : progname + 1);

  if(!parse_args(argc, argv, args)){
    print_usage(true);
    return 1;
  }

  if(args.find('h') != args.end()){
    print_usage();
    return 0;
  }

  if(args.find('v') != args.end()){
    printf("%s\n", DEFAULT_SIGNATURE);
    return 0;
  }

  /* apply command-line options */
  if(!apply_args(args)){
    print_usage(true);
    goto error;
  }

  init_logging();

  /* load and apply configuration file */
  if(AmConfig::readConfiguration()){
    ERROR("Errors occured while reading configuration file: exiting.");
    return -1;
  }

  log_level = AmConfig::LogLevel;
  log_stderr = AmConfig::LogStderr;

  /* re-apply command-line options to override configuration file */
  if(!apply_args(args)){
    goto error;
  }

  AmConfig::LocalIP = getLocalIP(AmConfig::LocalIP);
  if (AmConfig::LocalIP.empty()) {
    ERROR("Cannot determine proper local address for media advertising!\n"
          "Try using 'ifconfig -a' to find a proper interface and configure SEMS to use it.\n");
    goto error;
  }

  if (AmConfig::LocalSIPIP.empty()) {
    AmConfig::LocalSIPIP = AmConfig::LocalIP;
  }

  printf("Configuration:\n"
#ifdef _DEBUG
         "       log level:           %s (%i)\n"
         "       log to stderr:       %s\n"
#endif
	 "       configuration file:  %s\n"
	 "       plug-in path:        %s\n"
#ifndef DISABLE_DAEMON_MODE
         "       daemon mode:         %s\n"
         "       daemon UID:          %s\n"
         "       daemon GID:          %s\n"
#endif
	 "       local SIP IP:        %s\n"
         "       public media IP:     %s\n"
	 "       local SIP port:      %i\n"
	 "       local media IP:      %s\n"
	 "       out-bound proxy:     %s\n"
	 "       application:         %s\n"
	 "\n",
#ifdef _DEBUG
	 log_level2str[AmConfig::LogLevel], AmConfig::LogLevel,
         AmConfig::LogStderr ? "yes" : "no",
#endif
	 AmConfig::ConfigurationFile.c_str(),
	 AmConfig::PlugInPath.c_str(),
#ifndef DISABLE_DAEMON_MODE
	 AmConfig::DaemonMode ? "yes" : "no",
	 AmConfig::DaemonUid.empty() ? "<not set>" : AmConfig::DaemonUid.c_str(),
	 AmConfig::DaemonGid.empty() ? "<not set>" : AmConfig::DaemonGid.c_str(),
#endif
	 AmConfig::LocalSIPIP.c_str(),
	 AmConfig::PublicIP.c_str(),
	 AmConfig::LocalSIPPort,
	 AmConfig::LocalIP.c_str(),
	 AmConfig::OutboundProxy.c_str(),
	 AmConfig::Application.empty() ? "<not set>" : AmConfig::Application.c_str());

#ifndef DISABLE_DAEMON_MODE

  if(AmConfig::DaemonMode){
    if(!AmConfig::DaemonGid.empty()){
      unsigned int gid;
      if(str2i(AmConfig::DaemonGid, gid)){
	struct group* grnam = getgrnam(AmConfig::DaemonGid.c_str());
	if(grnam != NULL){
	  gid = grnam->gr_gid;
	}
	else{
	  ERROR("Cannot find group '%s' in the group database.\n",
		AmConfig::DaemonGid.c_str());
	  goto error;
	}
      }
      if(setgid(gid)<0){
	ERROR("Cannot change GID to %i: %s.",
	      gid,
	      strerror(errno));
	goto error;
      }
    }

    if(!AmConfig::DaemonUid.empty()){
      unsigned int uid;
      if(str2i(AmConfig::DaemonUid, uid)){
	struct passwd* pwnam = getpwnam(AmConfig::DaemonUid.c_str());
	if(pwnam != NULL){
	  uid = pwnam->pw_uid;
	}
	else{
	  ERROR("Cannot find user '%s' in the user database.\n",
		AmConfig::DaemonUid.c_str());
	  goto error;
	}
      }
      if(setuid(uid)<0){
	ERROR("Cannot change UID to %i: %s.",
	      uid,
	      strerror(errno));
	goto error;
      }
    }

    /* fork to become!= group leader*/
    int pid;
    if ((pid=fork())<0){
      ERROR("Cannot fork: %s.\n", strerror(errno));
      goto error;
    }else if (pid!=0){
      /* parent process => exit*/
      return 0;
    }
    /* become session leader to drop the ctrl. terminal */
    if (setsid()<0){
      ERROR("setsid failed: %s.\n", strerror(errno));
    }
    /* fork again to drop group  leadership */
    if ((pid=fork())<0){
      ERROR("Cannot fork: %s.\n", strerror(errno));
      goto error;
    }else if (pid!=0){
      /*parent process => exit */
      return 0;
    }
	
    if(write_pid_file()<0) {
      goto error;
    }

    /* try to replace stdin, stdout & stderr with /dev/null */
    if (freopen("/dev/null", "r", stdin)==0){
      ERROR("Cannot replace stdin with /dev/null: %s.\n",
	    strerror(errno));
      /* continue, leave it open */
    };
    if (freopen("/dev/null", "w", stdout)==0){
      ERROR("Cannot replace stdout with /dev/null: %s.\n",
	    strerror(errno));
      /* continue, leave it open */
    };
    /* close stderr only if log_stderr=0 */
    if ((!log_stderr) && (freopen("/dev/null", "w", stderr)==0)){
      ERROR("Cannot replace stderr with /dev/null: %s.\n",
	    strerror(errno));
      /* continue, leave it open */
    };
  }

#endif /* DISABLE_DAEMON_MODE */

  main_pid = getpid();

  init_random();

  if(set_sighandler(signal_handler))
    goto error;
    
  INFO("Loading plug-ins\n");
  AmPlugIn::instance()->init();
  if(AmPlugIn::instance()->load(AmConfig::PlugInPath, AmConfig::LoadPlugins))
    goto error;

#ifdef WITH_ZRTP
  if (AmZRTP::init()) {
    ERROR("Cannot initialize ZRTP\n");
    goto error;
  }
#endif

  INFO("Starting session container\n");
  AmSessionContainer::instance()->start();
  
#ifdef SESSION_THREADPOOL
  INFO("Starting session processor threads\n");
  AmSessionProcessor::addThreads(AmConfig::SessionProcessorThreads);
#endif 

  INFO("Starting media processor\n");
  AmMediaProcessor::instance()->init();

  INFO("Starting RTP receiver\n");
  AmRtpReceiver::instance()->start();

  INFO("Starting SIP stack (control interface)\n");
  sip_ctrl.load();
  sip_ctrl.run(AmConfig::LocalSIPIP, AmConfig::LocalSIPPort);
  
  success = true;

  // session container stops active sessions
  INFO("Disposing session container\n");
  AmSessionContainer::dispose();

  INFO("Disposing RTP receiver\n");
  AmRtpReceiver::dispose();

  INFO("Disposing media processor\n");
  AmMediaProcessor::dispose();

  INFO("Disposing event dispatcher\n");
  AmEventDispatcher::dispose();

 error:
  INFO("Disposing plug-ins\n");
  AmPlugIn::dispose();

#ifndef DISABLE_DAEMON_MODE
  if (AmConfig::DaemonMode) {
    unlink(AmConfig::DaemonPidFile.c_str());
  }
#endif

  sip_ctrl.cleanup();

  INFO("Exiting (%s)\n", success ? "success" : "failure");
  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
