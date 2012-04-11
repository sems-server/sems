/*
 * $Id: trans_layer.h 1486 2009-08-29 14:40:38Z rco $
 *
 * Copyright (C) 2007 Raphael Coeffic
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
#ifndef _trans_layer_h_
#define _trans_layer_h_

#include "cstring.h"
#include "singleton.h"

#include <list>
using std::list;

#include <vector>
using std::vector;

struct sip_msg;
struct sip_uri;
class sip_trans;
struct sip_header;
struct sockaddr_storage;

class trans_ticket;
class trans_bucket;
class trsp_socket;
class sip_ua;
class timer;

/** 
 * The transaction layer object.
 * Uses the singleton pattern.
 */
class _trans_layer
{
public:

    /**
     * Config option: if true, final replies without 
     * a to-tag will be accepted for requests which do not
     * create a dialog.
     */
    static bool accept_fr_without_totag;

    /**
     * Register a SIP UA.
     * This method MUST be called ONCE.
     */
    void register_ua(sip_ua* ua);

    /**
     * Register a transport instance.
     * This method MUST be called at least once.
     */
    void register_transport(trsp_socket* trsp);

    /**
     * Clears all registered transport instances.
     */
    void clear_transports();

    /**
     * Sends a UAS reply.
     * If a body is included, the hdrs parameter should
     * include a well-formed 'Content-Type', but no
     * 'Content-Length' header.
     */
    int send_reply(trans_ticket* tt,
		   int reply_code, const cstring& reason,
		   const cstring& to_tag, const cstring& hdrs, 
		   const cstring& body,
		   int out_interface = -1);

    /**
     * Sends a UAC request.
     * Caution: Route headers should not be added to the
     * general header list (msg->hdrs).
     * @param [in]  msg Pre-built message.
     * @param [out] tt transaction ticket (needed for replies & CANCEL)
     */
    int send_request(sip_msg* msg, trans_ticket* tt,
		     const cstring& _next_hop,
		     int out_interface = -1);

    /**
     * Cancels a request. 
     * A CANCEL request is sent if necessary.
     * @param tt transaction ticket from the original INVITE.
     */
    int cancel(trans_ticket* tt);
    
    /**
     * Called by the transport layer
     * when a new message has been recived.
     */
    void received_msg(sip_msg* msg);

    /**
     * This is called by the transaction timer callback.
     * At this place, the bucket is already locked, so
     * please be quick.
     */
    void timer_expired(timer* t, trans_bucket* bucket, sip_trans* tr);

    sip_ua*              ua;
    vector<trsp_socket*> transports;

    /**
     * Tries to find a registered transport socket
     * suitable for sending to the destination supplied.
     */
    trsp_socket* find_transport(sockaddr_storage* remote_ip);

    /**
     * Implements the state changes for the UAC state machine
     * @return -1 if errors
     * @return transaction state if successfull
     */
    int update_uac_reply(trans_bucket* bucket, sip_trans* t, sip_msg* msg);
    int update_uac_request(trans_bucket* bucket, sip_trans*& t, sip_msg* msg);

    /**
     * Implements the state changes for the UAS state machine
     */
    int update_uas_request(trans_bucket* bucket, sip_trans* t, sip_msg* msg);
    void update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code);

    /**
     * Send ACK coresponding to error replies
     */
    void send_non_200_ack(sip_msg* reply, sip_trans* t);

    /**
     * Sends a stateless reply. Useful for error replies.
     * If a body is included, the hdrs parameter should
     * include a well-formed 'Content-Type', but no
     * 'Content-Length' header.
     */
    int send_sl_reply(sip_msg* req, int reply_code, 
		      const cstring& reason, 
		      const cstring& hdrs, const cstring& body);
    
    /**
     * Transaction timeout
     */
    void timeout(trans_bucket* bucket, sip_trans* t);

protected:

    /**
     * Fills the address structure passed and modifies 
     * R-URI and Route headers as needed.
     */
    int set_next_hop(sip_msg* msg, cstring* next_hop,
		     unsigned short* next_port);

    /**
     * Fills msg->remote_ip according to next_hop and next_port.
     */
    int set_destination_ip(sip_msg* msg, cstring* next_hop, unsigned short next_port);    

    /** Avoid external instantiation. @see singleton. */
    _trans_layer();
    ~_trans_layer();
    
};

typedef singleton<_trans_layer> trans_layer;

class trans_ticket
{
    sip_trans*    _t;
    trans_bucket* _bucket;
    
    friend class _trans_layer;

public:
    trans_ticket()
	: _t(0), _bucket(0) {}

    trans_ticket(sip_trans* t, trans_bucket* bucket)
	: _t(t), _bucket(bucket) {}

    trans_ticket(const trans_ticket& ticket)
	: _t(ticket._t), _bucket(ticket._bucket) {}
};

#endif // _trans_layer_h_


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
