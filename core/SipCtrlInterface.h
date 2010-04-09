/*
 * $Id: SipCtrlInterface.h 1048 2008-07-15 18:48:07Z sayer $
 *
 * Copyright (C) 2007 Raphael Coeffic
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
#ifndef _SipCtrlInterface_h_
#define _SipCtrlInterface_h_

#include "sip/sip_ua.h"
#include "AmThread.h"

#include <string>
#include <list>
using std::string;
using std::list;

class AmSipRequest;
class AmSipReply;

class trans_layer;
class trans_bucket;
struct sip_msg;
struct sip_header;

class SipCtrlInterface:
    public sip_ua
{

    //string         bind_addr;
    //unsigned short bind_port;
    
    void prepare_routes_uac(const list<sip_header*>& routes, string& route_field);
    void prepare_routes_uas(const list<sip_header*>& routes, string& route_field);
    static int cancel(AmSipRequest& req);

    friend class udp_trsp;
public:

    static string outbound_host;
    static unsigned int outbound_port;
    static bool log_parsed_messages;
    static int log_raw_messages;
    static bool accept_fr_without_totag;

    SipCtrlInterface();
    ~SipCtrlInterface(){}

    int load();

    void run(const string& bind_addr, unsigned short bind_port);
    
    /**
     * From AmCtrlInterface
     */
    static int send(AmSipRequest &req);
    static int send(const AmSipReply &rep);
    
//     string getContact(const string &displayName, 
// 		    const string &userName, const string &hostName, 
// 		    const string &uriParams, const string &hdrParams);
    
    void handleSipMsg(AmSipRequest &req);
    void handleSipMsg(AmSipReply &rep);

    /**
     * From sip_ua
     */
    void handle_sip_request(trans_ticket* tt, sip_msg* msg);
    void handle_sip_reply(sip_msg* msg);
};


#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
