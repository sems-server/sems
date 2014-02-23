/*
 * $Id: sip_trans.h 1001 2008-06-02 10:19:47Z rco $
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

#ifndef _sip_trans_h
#define _sip_trans_h

#include "cstring.h"
#include "wheeltimer.h"

#include <sys/socket.h>

#include <list>
using std::list;

struct sip_msg;
struct sip_target_set;

class trsp_socket;
class msg_logger;

/**
 * Transaction types
 */
enum {

    TT_UAS=1,
    TT_UAC
};

/**
 * Transaction states
 */
enum {

    TS_TRYING=1,   // UAC:!INV;     UAS:!INV
    TS_CALLING,    // UAC:INV
    TS_PROCEEDING, // UAC:INV,!INV; UAS:INV,!INV
    TS_PROCEEDING_REL, // UAS:INV
    TS_COMPLETED,  // UAC:INV,!INV; UAS:INV,!INV
    TS_CONFIRMED,  //               UAS:INV
    TS_TERMINATED_200,
    TS_TERMINATED, // UAC:INV,!INV; UAS:INV,!INV

    TS_ABANDONED,
    TS_REMOVED
};


/**
 * We support at most 3 timer per transaction,
 * which is okay according to the standard
 */
#define SIP_TRANS_TIMERS 3

class sip_trans;

class trans_timer
    : protected timer
{
    trans_timer(const trans_timer& ti) : timer() {}

public:
    unsigned int type;
    unsigned int bucket_id;
    sip_trans*   t;

    trans_timer(unsigned int timer_type, unsigned int expires,
		int bucket_id, sip_trans* t)
        : timer(expires), type(timer_type),
	  bucket_id(bucket_id), t(t)
    {}

    trans_timer(const trans_timer& ti, int bucket_id, sip_trans* t)
        : timer(ti.expires), type(ti.type),
	  bucket_id(bucket_id), t(t)
    {}

    void fire();
};

class sip_trans
{
    trans_timer* timers[SIP_TRANS_TIMERS];

 public:
    /** Transaction type */
    unsigned int type;
    
    /** Request that initiated 
	the transaction */
    sip_msg* msg;

    /** To-tag included in reply.
	(useful for ACK matching) */
    cstring to_tag;

    /** reply code of last
        sent/received reply */
    int reply_status;

    /** Transaction state */
    int state;

    /** used by UAS only; keeps RSeq of last sent reliable 1xx */
    unsigned int last_rseq;

    /** Dialog-ID used for UAC transactions */
    cstring dialog_id;

    /** Destination list for requests */
    sip_target_set* targets;
    
    /**
     * Retransmission buffer
     *  - UAC transaction: ACK
     *  - UAS transaction: last reply
     */
    char* retr_buf;

    /** Length of the retransmission buffer */
    int   retr_len;

    /** Destination for retransmissions */
    sockaddr_storage retr_addr;
    trsp_socket*     retr_socket;

    /** flags used by send_request() */
    unsigned int flags;

    /** message logging */
    msg_logger* logger;

    /**
     * Tells if a specific timer is set
     *
     * @param timer_type @see sip_timer_type
     */
    bool is_timer_set(unsigned int timer_type);

    /**
     * Fetches a specific timer
     *
     * @param timer_type @see sip_timer_type
     */
    trans_timer* get_timer(unsigned int timer_type);
    
    /**
     * Resets a specfic timer with a delay value
     *
     * @param timer_type @see sip_timer_type
     * @param expires_delay delay before expiration in millisecond
     * @param bucket_id id of the transaction's bucket 
     */
    void reset_timer(unsigned int timer_type, 
		     unsigned int expire_delay /* ms */,
		     unsigned int bucket_id);

    /**
     * Resets a specific timer
     *
     * @param t the new timer
     * @param timer_type @see sip_timer_type
    */
    void reset_timer(trans_timer* t, unsigned int timer_type);

    /**
     * Clears a specfic timer
     *
     * @param timer_type @see sip_timer_type
     */
    void clear_timer(unsigned int timer_type);

    /**
     * Resets every timer
    */
    void reset_all_timers();

    /**
     * Retransmits the content of the retry buffer (replies or non-200 ACK).
     */
    void retransmit();

    sip_trans();
    ~sip_trans();

    const char* type_str() const;
    const char* state_str() const;

    void dump() const;
};

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
