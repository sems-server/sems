/*
 * $Id: AmServer.cpp,v 1.26.2.2 2005/08/25 06:55:12 rco Exp $
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

#include "AmServer.h"
#include "log.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "AmCtrlInterface.h"
#include "AmInterfaceHandler.h"

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>

#define SIP_POLL_TIMEOUT 50 /*50 ms*/

//
// AmServer methods
//

AmServer* AmServer::_instance;

AmServer* AmServer::instance()
{
    if(!_instance)
	_instance = new AmServer();
    return _instance;
}


void AmServer::run()
{
    struct pollfd* ufds=0;
    int            nfds=ifaces_map.size();
    
    ufds = new struct pollfd [nfds];
    assert(ufds);

    struct pollfd* ufds_it = ufds;
    for(CtrlInterfaces::iterator it = ifaces_map.begin();
	it != ifaces_map.end(); it++) {

	ufds_it->fd = it->first;
	ufds_it->events  = POLLIN;
	ufds_it->revents = 0;
	ufds_it++;
    }

    while(true){

	int ret = poll(ufds,nfds,SIP_POLL_TIMEOUT);
	if(ret < 0){
	    ERROR("AmServer: poll: %s\n",strerror(errno));
	    continue;
	}

	if(ret < 1)
	    continue;

	for(int i=0; i<nfds; i++) {

	    DBG("revents = %i\n",ufds[i].revents);
 	    if(!(ufds[i].revents & POLLIN))
 		continue;
	    
	    CtrlInterfaces::iterator it = ifaces_map.find(ufds[i].fd);
	    if(it == ifaces_map.end()){
		ERROR("bad fd %i\n",ufds[i].fd);
		continue;
	    }
	    
	    IfaceDesc& iface = it->second;// ifaces_map[ufds[i].fd];
	    try {
		if(iface.ctrl->cacheMsg() ||
		   (iface.handler->handleRequest(iface.ctrl) == -1))
		    iface.ctrl->consume();
		
	    } catch(const string& err) {
		ERROR("%s\n",err.c_str());
	    }
	}
    }
}

void AmServer::regIface(const IfaceDesc& i)
{
    ifaces_map[i.ctrl->getFd()] = i;
}
