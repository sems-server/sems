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
#include "atomic_types.h"

#include "parse_next_hop.h"

#include <list>
using std::list;

#include <vector>
using std::vector;

#include <string>
using std::string;

#include <map>
using std::map;

struct sip_msg;
struct sip_uri;
class  sip_trans;
struct sip_header;
struct sockaddr_storage;
struct dns_handle;
struct sip_target_set;

class trans_ticket;
class trans_bucket;
class trans_timer;
class trsp_socket;
class sip_ua;

//draft msg logging
class msg_logger;

// replace the RURI-host with next-hop IP / port
#define TR_FLAG_NEXT_HOP_RURI 1
// disable blacklist
#define TR_FLAG_DISABLE_BL    2

/* Each counter has a method for incrementing to allow changing implementation
 * of the stats class later without touching the code using it. (One possible
 * solution is to make all the numbers guarded by one mutex to have whole set of
 * transaction statistics being atomic) */
class trans_stats
{
  private:
    atomic_int sent_requests;
    atomic_int sent_replies;
    atomic_int received_requests;
    atomic_int received_replies;
    atomic_int sent_reply_retrans;
    atomic_int sent_request_retrans;

  public:

    /** increment number of sent requests */
    void inc_sent_requests() { sent_requests.inc(); }

    /** increment number of sent replies */
    void inc_sent_replies() { sent_replies.inc(); }

    /** increment number of received requests */
    void inc_received_requests() { received_requests.inc(); }

    /** increment number of received replies */
    void inc_received_replies() { received_replies.inc(); }

    /** increment number of sent request retransmissions */
    void inc_sent_request_retrans() { sent_request_retrans.inc(); }

    /** increment number of sent reply retransmissions */
    void inc_sent_reply_retrans() { sent_reply_retrans.inc(); }


    unsigned get_sent_requests() const { return sent_requests.get(); }
    unsigned get_sent_replies() const { return sent_replies.get(); }
    unsigned get_received_requests() const { return received_requests.get(); }
    unsigned get_received_replies() const { return received_replies.get(); }
    unsigned get_sent_request_retrans() const { return sent_request_retrans.get(); }
    unsigned get_sent_reply_retrans() const { return sent_reply_retrans.get(); }
};

/** 
 * The transaction layer object.
 * Uses the singleton pattern.
 */
class _trans_layer
{
private:
    trans_stats stats;
    sip_ua*     ua;

    struct less_case_i { bool operator ()(const string& lhs, const string& rhs) const; };
    typedef map<string,trsp_socket*,less_case_i> prot_collection;

    vector<prot_collection> transports;

public:

    /**
     * Config option: if true, final replies without 
     * a to-tag will be accepted for requests which do not
     * create a dialog.
     */
    static bool accept_fr_without_totag;

    /**
     * Config option: default blacklist time-to-live
     */
    static unsigned int default_bl_ttl;

    /**
     * Register a SIP UA.
     * This method MUST be called ONCE.
     */
    void register_ua(sip_ua* ua);

    /**
     * Register a transport instance.
     * This method MUST be called at least once.
     */
    int register_transport(trsp_socket* trsp);

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
    int send_reply(sip_msg* msg, const trans_ticket* tt, const cstring& dialog_id,
		   const cstring& to_tag, msg_logger* logger=NULL);

    /**
     * Sends a UAC request.
     * Caution: Route headers should not be added to the
     * general header list (msg->hdrs).
     * @param [in]  msg Pre-built message.
     * @param [out] tt transaction ticket (needed for replies & CANCEL)
     */
    int send_request(sip_msg* msg, trans_ticket* tt, const cstring& dialog_id,
		     const cstring& _next_hop, int out_interface = -1,
		     unsigned int flags=0, msg_logger* logger=NULL);

    /**
     * Cancels a request. 
     * A CANCEL request is sent if necessary.
     * @param tt transaction ticket from the original INVITE.
     */
    int cancel(trans_ticket* tt, const cstring& dialog_id,
	       unsigned int inv_cseq, const cstring& hdrs);
    
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
    void timer_expired(trans_timer* t, trans_bucket* bucket, sip_trans* tr);

    /**
     * Tries to find an interface suitable for
     * sending to the destination supplied.
     */
    int find_outbound_if(sockaddr_storage* remote_ip);

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
     * Sends a stateful error reply.
     * If a body is included, the hdrs parameter should
     * include a well-formed 'Content-Type', but no
     * 'Content-Length' header.
     */
    int send_sf_error_reply(const trans_ticket* tt, const sip_msg* req,
			    int reply_code, const cstring& reason, 
			    const cstring& hdrs = cstring(),
			    const cstring& body = cstring());

    /**
     * Allows the transport layer to signal 
     * an asynchronous error while sending out
     * a SIP message.
     */
    void transport_error(sip_msg* msg);

    /**
     * Transaction timeout
     */
    void timeout(trans_bucket* bucket, sip_trans* t);

    const trans_stats &get_stats() { return stats; }

protected:

    /**
     * Fills the address structure passed and modifies 
     * R-URI and Route headers as needed.
     */
    int set_next_hop(sip_msg* msg, cstring* next_hop,
		     unsigned short* next_port, cstring* next_trsp);

    /**
     * Fills the local_socket attribute using the given
     * transport and interface. If out_interface == -1,
     * we will try hard to find an interface based on msg->remote_ip.
     */
    int set_trsp_socket(sip_msg* msg, const cstring& next_trsp,
			int out_interface);

    sip_trans* copy_uac_trans(sip_trans* tr);

    /**
     * If the destination has multiple IPs (SRV records),
     * try the next destination IP.
     * @return 0 if the message has been re-sent.
     *        -1 if no additional destination has been found.
     */
    int try_next_ip(trans_bucket* bucket, sip_trans* tr, bool use_new_trans);

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
    int update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code);

    /** Avoid external instantiation. @see singleton. */
    _trans_layer();
    ~_trans_layer();

    /**
     * Processes a parsed SIP message
     */
    void process_rcvd_msg(sip_msg* msg);
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

    /**
     * Locks the transaction bucket before accessing the transaction pointer.
     */
    void lock_bucket() const;

    /**
     * Unlocks the transaction bucket after accessing the transaction pointer.
     */
    void unlock_bucket() const;

    /**
     * Get the transaction pointer
     * Note: the transaction bucket must be locked before
     */
    const sip_trans* get_trans() const;

    /**
     * Remove the transaction
     * Note: the transaction bucket must be locked before
     */
    void remove_trans();
};

#endif // _trans_layer_h_


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
