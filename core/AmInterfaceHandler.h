/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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

#ifndef AmInterfaceHandler_h
#define AmInterfaceHandler_h

#include "AmThread.h"
#include "AmSipRequest.h"

#include <string>
#include <map>
#include <vector>

using std::string;
using std::map;
using std::vector;

class AmCtrlInterface;
class AmSIPEventHandler;

/** \brief interface of an InterfaceHandler */
class AmInterfaceHandler
{
public:
    virtual ~AmInterfaceHandler();

    /** @return -1 on parsing error, 0 on success. 
     *  Throws string on other error cases.
     */
    virtual int handleRequest(AmCtrlInterface* ctrl)=0;
};

/**
 * \brief interface of a Server function
 *
 * Derive your class from this if you want
 * to implement a request handler.
 */
class AmRequestHandlerFct
{
public:
    virtual int execute(AmCtrlInterface* ctrl, const string& cmd)=0;
    virtual ~AmRequestHandlerFct(){}
};

/**
 * \brief SIP request handler
 *
 * Handles SIP requests that are received by the Server
 */
class AmRequestHandler: public AmInterfaceHandler, 
			public AmRequestHandlerFct
{
    AmMutex                          fct_map_mut;
    map<string,AmRequestHandlerFct*> fct_map;

public:
    static AmRequestHandler* get();

    int  handleRequest(AmCtrlInterface* ctrl);
    int  execute(AmCtrlInterface* ctrl, const string& cmd);
    void dispatch(AmSipRequest& req);
    
    AmRequestHandlerFct* getFct(const string& name);
    void registerFct(const string& name, AmRequestHandlerFct* fct);
};

/**
 * \brief SIP reply handler
 *
 * Handles SIP replys that are received by the Server
 */
class AmReplyHandler: public AmInterfaceHandler
{
    static AmReplyHandler* _instance;
    AmCtrlInterface* m_ctrl;

    AmReplyHandler(AmCtrlInterface* ctrl);
	vector<AmSIPEventHandler*> reply_handlers;

public:
    static AmReplyHandler* get();

    AmCtrlInterface* getCtrl() { return m_ctrl; }
    
    int handleRequest(AmCtrlInterface* ctrl);

	/** register a reply handler for incoming replies pertaining 
	 * to a dialog without a session (not in SessionContainer) */
	void registerReplyHandler(AmSIPEventHandler* eh);
};

#endif
