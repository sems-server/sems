/*
 * $Id$
 *
 * Copyright (C) 2002-2005 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "Statistics.h"

#include "AmUtils.h"
#include "AmServer.h"
#include "AmSessionContainer.h"
#include "AmCtrlInterface.h"
#include "StatsUDPServer.h"
#include "log.h"


#include <string>
using std::string;

EXPORT_SESSION_FACTORY(StatsFactory,MOD_NAME);

StatsFactory::StatsFactory(const std::string& _app_name)
  : AmSessionFactory(_app_name)
{
}

AmSession* StatsFactory::onInvite(const AmSipRequest& req)
{
  return NULL;
}

int StatsFactory::onLoad()
{
  //sc = AmSessionContainer::instance();

  StatsUDPServer* stat_srv = StatsUDPServer::instance();
  if(!stat_srv){
  }
  StatsUDPServer::instance()->start();

  //AmUDPCtrlInterface* udp_ctrl = new AmUDPCtrlInterface(NULL);
  //if(udp_ctrl->init("10.36.2.69:8000")){
  //delete udp_ctrl;
  //udp_ctrl = 0;
  //return -1;
  //}
  //AmServer::instance()->regIface(IfaceDesc(udp_ctrl,this));

  return 0;
}


#define SAFECTRLCALL0(fct)\
                           {\
                               if(ctrl->fct() == -1){\
                                   ERROR("%s returned -1\n",#fct);\
                                   return -1;\
                               }\
                           }

#define SAFECTRLCALL1(fct,arg1)\
                           {\
                               if(ctrl->fct(arg1) == -1){\
                                   ERROR("%s returned -1\n",#fct);\
                                   return -1;\
                               }\
                           }

#define READ_PARAMETER(p)\
             {\
		 SAFECTRLCALL1(getParam,p);\
		 DBG("%s = <%s>\n",#p,p.c_str());\
	     }

int StatsFactory::handleRequest(AmCtrlInterface* ctrl)
{

  string cmd;
  string reply;
  string reply_fifo;

  READ_PARAMETER(cmd);
  READ_PARAMETER(reply_fifo);
  if(reply_fifo.empty())
    throw string("reply fifo parameter is empty");

  try {
    if(cmd == "calls")
      reply = "Active calls: " + int2str(sc->getSize()) + "\n";
    else
      throw string("unknown command: '" + cmd + "'");

    ctrl->sendto(reply_fifo,reply.c_str(),reply.length());
  }
  catch(const string& err){
    string msg = err + "\n";
    ctrl->sendto(reply_fifo,msg.c_str(),msg.length());
    return -1;
  }

  return 0;
}


