/*
 * $Id: AmServer.h,v 1.10.2.1 2005/04/13 10:57:09 rco Exp $
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

#ifndef _AmServer_h_
#define _AmServer_h_

#include <stdio.h>
#include <sys/stat.h>

#include "AmCmd.h"
#include "AmThread.h"
#include "AmSipReply.h"

#include <algorithm>
#include <deque>
#include <map>

using std::map;
using std::deque;
using std::greater;

class AmCtrlInterface;
class AmInterfaceHandler;

struct IfaceDesc
{
    AmCtrlInterface*    ctrl;
    AmInterfaceHandler* handler;

    IfaceDesc(AmCtrlInterface* ctrl, AmInterfaceHandler* handler)
	: ctrl(ctrl),handler(handler)
    {}

    IfaceDesc(const IfaceDesc& i)
	: ctrl(i.ctrl), handler(i.handler)
    {}

    IfaceDesc()
	: ctrl(0),handler(0)
    {}
};


class AmServer
{
 private:
    /** 
     * Singleton pointer.
     * @see instance()
     */
    static AmServer* _instance;


    typedef map<int,IfaceDesc,greater<int> > CtrlInterfaces;

    CtrlInterfaces          ifaces_map;

    /** Avoid external instantiation. @see instance(). */
    AmServer(){}
    /** Avoid external instantiation. @see instance(). */
    ~AmServer(){}

public:
    /** Get a fifo server instance. */
    static AmServer* instance();

    /** Runs the fifo server. */
    void run();

    /** 
     * Register an interface.
     * WARNING: only before the server starts up.
     */
    void regIface(const IfaceDesc& i);
};

#endif

// Local Variables:
// mode:C++
// End:



