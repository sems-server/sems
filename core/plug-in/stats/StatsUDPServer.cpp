/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2007 iptego GmbH
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

#include "StatsUDPServer.h"
#include "Statistics.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "log.h"
#include "AmPlugIn.h"
#include "AmApi.h"

#include "sip/trans_table.h"

#include <string>
using std::string;

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#define CTRL_MSGBUF_SIZE 2048
// int msg_get_line(char*& msg_c, char* str, size_t len)
// {
//     size_t l;
//     char*  s=str;

//     if(!len)
// 	return 0;
    
//     for(l=len; l && (*msg_c) && (*msg_c !='\n'); msg_c++ ){
// 	if(*msg_c!='\r'){
// 	    *(s++) = *msg_c;
// 	    l--;
// 	}
//     }

//     if(*msg_c)
// 	msg_c++;

//     if(l>0){
// 	// We need one more character
// 	// for trailing '\0'.
// 	*s='\0';

// 	return int(s-str);
//     }
//     else
// 	// buffer overran.
// 	return -1;
// }

// int msg_get_param(char*& msg_c, string& p)
// {
//     char line_buf[MSG_BUF_SIZE];

//     if( msg_get_line(msg_c,line_buf,MSG_BUF_SIZE) != -1 ){

// 	if(!strcmp(".",line_buf))
// 	    line_buf[0]='\0';

// 	p = line_buf;
// 	return 0;
//     }

//     return -1;
// }

StatsUDPServer* StatsUDPServer::_instance=0;

StatsUDPServer* StatsUDPServer::instance()
{
  if(!_instance) {
    _instance = new StatsUDPServer();
    if(_instance->init() != 0){
      delete _instance;
      _instance = 0;
    }
    else {
      _instance->start();
    }
  }
  return _instance;
}

StatsUDPServer::StatsUDPServer()
  : sd(0)
{
  sc = AmSessionContainer::instance();
}

StatsUDPServer::~StatsUDPServer()
{
  if(sd)
    close(sd);
}

int StatsUDPServer::init()
{
  string udp_addr;
  int    udp_port = 0;
  int    optval;

  AmConfigReader cfg;
  if(cfg.loadFile(add2path(AmConfig::ModConfigPath,1, MOD_NAME ".conf")))
    return -1;

  udp_port = (int)cfg.getParameterInt("monit_udp_port",(unsigned int)-1);
  if(udp_port == -1){
    ERROR("invalid port number in the monit_udp_port parameter\n ");
    return -1;
  }
  if(!udp_port)
    udp_port = DEFAULT_MONIT_UDP_PORT;

  DBG("udp_port = %i\n",udp_port);
  udp_addr = cfg.getParameter("monit_udp_ip","");

  sd = socket(PF_INET,SOCK_DGRAM,0);
  if(sd == -1){
    ERROR("could not open socket: %s\n",strerror(errno));
    return -1;
  }

  /* set sock opts? */
  optval=1;

  /* tos */
  optval=IPTOS_LOWDELAY;
  if (setsockopt(sd, IPPROTO_IP, IP_TOS, (void*)&optval, sizeof(optval)) ==-1){
    ERROR("WARNING: setsockopt(tos): %s\n", strerror(errno));
    /* continue since this is not critical */
  }

  struct sockaddr_in sa;
  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(udp_port);
    
  if(!inet_aton(udp_addr.c_str(),(in_addr*)&sa.sin_addr.s_addr)){
    // non valid address
    ERROR("invalid IP in the monit_udp_ip parameter\n");
    return -1;
  }

  //bool socket_bound = false;
  //while(!socket_bound){
  if( bind(sd,(sockaddr*)&sa,sizeof(struct sockaddr_in)) == -1 ){
    ERROR("could not bind socket at port %i: %s\n",udp_port,strerror(errno));
    //udp_port += 1;
    //sa.sin_port = htons(udp_port);

    return -1;
  } else {
    INFO("stats server listening on %s:%i\n",udp_addr.c_str(), udp_port);
    //socket_bound = true;
  }
  //}

  return 0;
}

void StatsUDPServer::run()
{
  DBG("running StatsUDPServer...\n");
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  char msg_buf[MSG_BUF_SIZE];
  int  msg_buf_s;

  while(true){

    msg_buf_s = recvfrom(sd,msg_buf,MSG_BUF_SIZE,0,(sockaddr*)&addr,&addrlen);
    if(msg_buf_s == -1){

      switch(errno){
      case EINTR:
      case EAGAIN:
	continue;
      default: break;
      };

      ERROR("recvfrom: %s\n",strerror(errno));
      break;
    }

    //printf("received packet from: %s:%i\n",
    //       inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));

    string             reply;
    struct sockaddr_in reply_addr;

    if(execute(msg_buf,reply,reply_addr) == -1)
      continue;

    send_reply(reply,addr);
  }
    
}

static int msg_get_line(char*& msg_c, char* str, size_t len)
{
  size_t l;
  char*  s=str;

  if(!len)
    return 0;
    
  for(l=len; l && (*msg_c) && (*msg_c !='\n'); msg_c++ ){
    *(s++) = *msg_c;
    l--;
  }

  if(*msg_c)
    msg_c++;

  if(l>0){
    // We need one more character
    // for trailing '\0'.
    *s='\0';

    return int(s-str);
  }
  else {
    ERROR("buffer too small (size=%u)\n",(unsigned int)len);
    // buffer overran.
    return -1;
  }
}

static int msg_get_param(char*& msg_c, string& p, char* line_buf, unsigned int size)
{
  if( msg_get_line(msg_c,line_buf,size) != -1 ){

    if(!strcmp(".",line_buf))
      line_buf[0]='\0';

    p = line_buf;
  }
  else {
    ERROR("msg_get_line failed\n");
    return -1;
  }

  return 0;
}

int StatsUDPServer::execute(char* msg_buf, string& reply, 
			    struct sockaddr_in& addr)
{
  char buffer[CTRL_MSGBUF_SIZE];
  string cmd_str,reply_addr,reply_port;
  char *msg_c = msg_buf;

  msg_get_param(msg_c,cmd_str,buffer,CTRL_MSGBUF_SIZE);

  if(cmd_str == "calls")
      reply = "Active calls: " + int2str(AmSession::getSessionNum()) + "\n";
  else if (cmd_str == "which") {
    reply = 
      "calls                              -  number of active calls (Session Container size)\n"
      "which                              -  print available commands\n"
      "version                            -  return SEMS version\n"
      "set_loglevel <loglevel>            -  set log level\n"
      "get_loglevel                       -  get log level\n"
      "set_cpslimit <limit>               -  set maximum allowed CPS\n"
      "get_cpslimit                       -  get maximum allowed CPS\n"
      "set_shutdownmode <1 or 0>          -  turns on and off shutdown mode\n"
      "get_shutdownmode                   -  returns the shutdown mode's current state\n"
      "get_callsavg                       -  get number of active calls (average since the last query)\n"
      "get_callsmax                       -  get maximum of active calls since the last query\n"
      "get_cpsavg                         -  get calls per second (5 sec average)\n"
      "get_cpsmax                         -  get maximum of CPS since the last query\n"

      "dump_transactions                  -  dump transaction table to log (loglevel debug)\n"

      "DI <factory> <function> (<args>)*  -  invoke DI command\n"
      "\n"
      "When in shutdown mode, SEMS will answer with the configured 5xx errorcode to\n"
      "new INVITE and OPTIONS requests.\n"
      ;
  }
  else if (cmd_str == "version") {
    reply = SEMS_VERSION;
  }
  else if (cmd_str == "dump_transactions") {
    dumps_transactions();
    reply = "200 OK";
  }
  else if (cmd_str.length() > 4 && cmd_str.substr(0, 4) == "set_") {
    // setters 
    if (cmd_str.substr(4, 8) == "loglevel") {
      if (!AmConfig::setLogLevel(&cmd_str.c_str()[13])) 
	reply= "invalid loglevel value.\n";
      else 
	reply= "loglevel set to "+int2str(log_level)+".\n";
    }

    else if (cmd_str.substr(4, 8) == "cpslimit") {
      int tmp;
      if(sscanf(&cmd_str.c_str()[13],"%u",&tmp) != 1)
        reply= "invalid CPS limit\n";
      else {
        sc->setCPSLimit(tmp);
        reply= "CPS limit set to "+int2str(sc->getCPSLimit().first)+".\n";
      }
    }

    else if (cmd_str.substr(4, 12) == "shutdownmode") {
      int tmp;
      if(sscanf(&cmd_str.c_str()[17],"%u",&tmp) != 1)
        reply= "invalid shutdownmode\n";
      else
	{
	  if(tmp)
	    {
	      AmConfig::ShutdownMode = true;
	      reply= "Shutdownmode activated!\n";
	    }
	  else
	    {
	      AmConfig::ShutdownMode = false;
	      reply= "Shutdownmode deactivated!\n";
	    }
	}
    }

    else 	reply = "Unknown command: '" + cmd_str + "'\n";
  }
  else if (cmd_str.length() > 4 && cmd_str.substr(0, 4) == "get_") {
    // setters 
    if (cmd_str.substr(4, 8) == "loglevel") {
      reply= "loglevel is "+int2str(log_level)+".\n";
    }

    else if(cmd_str.substr(4, 8) == "callsavg")
      reply = "Average active calls: " + int2str(AmSession::getAvgSessionNum()) + "\n";
    else if(cmd_str.substr(4, 8) == "callsmax")
      reply = "Maximum active calls: " + int2str(AmSession::getMaxSessionNum()) + "\n";
    else if(cmd_str.substr(4, 6) == "cpsavg")
      reply = "Average calls per second: " + int2str(sc->getAvgCPS()) + "\n";
    else if(cmd_str.substr(4, 6) == "cpsmax")
      reply = "Maximum calls per second: " + int2str(sc->getMaxCPS()) + "\n";
    else if(cmd_str.substr(4, 8) == "cpslimit")
      reply = "CPS hard limit: " + int2str(sc->getCPSLimit().first) + ", CPS limit: " +
        int2str(sc->getCPSLimit().second) + "\n";

    else if (cmd_str.substr(4, 12) == "shutdownmode") {
      if(AmConfig::ShutdownMode)
	{
	  reply= "Shutdownmode active!\n";
	}
      else
	{
	  reply= "Shutdownmode inactive!\n";
	}
    }

    else 	reply = "Unknown command: '" + cmd_str + "'\n";
  }
  else if (cmd_str.length() > 4 && cmd_str.substr(0, 3) == "DI ") {
    // Dynamic Invocation
    size_t p = cmd_str.find(' ', 4);
    string fact_name = cmd_str.substr(3, p-3);
    if (!fact_name.length()) {
      reply = "could not parse DI factory name.\n";
      return 0;
    }

    size_t p2 = cmd_str.find(' ', p+1);
    if (p2 == string::npos)
      p2 = cmd_str.length();
    string fct_name = cmd_str.substr(p+1, p2-p-1);
    p=p2+1;
    if (!fct_name.length()) {
      reply = "could not parse function name.\n";
      return 0;
    }
    try {

      // args need to be stored in string vector, 
      // because stl copyconstructor does not copy 
      // underlying c_str
      vector<string> s_args;
      while (p<cmd_str.length()) {
	p2 = cmd_str.find(' ', p);
	if (p2 == string::npos) {
	  if (p+1<cmd_str.length())
	    p2=cmd_str.length();
	  else 
	    break;
	}
	s_args.push_back(string(cmd_str.substr(p, p2-p)));
	p=p2+1;
      }
      AmArg args;
      for (vector<string>::iterator it = s_args.begin(); 
	   it != s_args.end(); it++) {
	args.push(it->c_str());
// 	DBG("mod '%s' added arg a '%s'\n", 
// 	    fact_name.c_str(),
// 	    it->c_str());
      }

      AmDynInvokeFactory* di_f = AmPlugIn::instance()->getFactory4Di(fact_name);
      if(!di_f){
	reply = "could not get '" + fact_name + "' factory\n";
	return 0;
      }
      AmDynInvoke* di = di_f->getInstance();
      if(!di){
	reply = "could not get DI instance from factory\n";
	return 0;
      }
      AmArg ret;
      di->invoke(fct_name, args, ret);
      reply=AmArg::print(ret);
    } catch (const AmDynInvoke::NotImplemented& e) {
      reply = "Exception occured: AmDynInvoke::NotImplemented '"+
	e.what+"'\n";
      return 0;
    } catch (...) {
      reply = "Exception occured.\n";
      return 0;
    }
			
  }
  else
    reply = "Unknown command: '" + cmd_str + "'\n";

  return 0;
}

int StatsUDPServer::send_reply(const string& reply,
			       const struct sockaddr_in& reply_addr)
{
  int err = sendto(sd,reply.c_str(),reply.length()+1,0,
		   (const sockaddr*)&reply_addr,
		   sizeof(struct sockaddr_in));

  return (err <= 0) ? -1 : 0;
}
