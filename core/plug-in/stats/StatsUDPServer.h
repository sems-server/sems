/*
 * Copyright (C) 2002-2005 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _StatsUDPServer_h_
#define _StatsUDPServer_h_

#include "AmThread.h"

#include <string>
using std::string;

#define DEFAULT_MONIT_UDP_PORT  5040
#define MSG_BUF_SIZE            256

class AmSessionContainer;

/** \brief UDP server running to provide statistics via simple UDP queries */
class StatsUDPServer: public AmThread
{
  static StatsUDPServer* _instance;
  AmSessionContainer*    sc;
  int sd;
    
  StatsUDPServer();
  ~StatsUDPServer();

  int init();

  int execute(char* msg_buf, string& reply, 
	      struct sockaddr_in& addr);

  int send_reply(const string& reply,
		 const struct sockaddr_in& reply_addr);
	
  void run();
  void on_stop(){}

public:
  static StatsUDPServer* instance();
};

#endif
// Local Variables:
// mode:C++
// End:
