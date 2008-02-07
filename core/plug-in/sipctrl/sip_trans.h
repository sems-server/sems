/*
 * $Id$
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

#ifndef _sip_trans_h
#define _sip_trans_h

#include "cstring.h"

#include <sys/socket.h>

struct sip_msg;

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
    TS_COMPLETED,  // UAC:INV,!INV; UAS:INV,!INV
    TS_CONFIRMED,  //               UAS:INV
    TS_TERMINATED_200,
    TS_TERMINATED, // UAC:INV,!INV; UAS:INV,!INV

    TS_REMOVED
};


enum sip_timer_type {

    STIMER_INVALID=0,

    // INVITE client transaction
    STIMER_A,  // Calling: (re-)send INV
    STIMER_B,  // Calling: -> Terminated
    STIMER_D,  // Completed: -> Terminated

    // non-INVITE client transaction
    STIMER_E,  // Trying/Proceeding: (re-)send request
    STIMER_F,  // Trying/Proceeding: terminate transaction
    STIMER_K,  // Completed: terminate transaction  

    // INVITE server transaction
    STIMER_G,  // Completed: (re-)send response
    STIMER_H,  // Completed: -> Terminated
    STIMER_I,  // Confirmed: -> Terminated

    // non-INVITE server transaction
    STIMER_J   // Completed: -> Terminated

};


/**
 * We support at most 3 timer per transaction,
 * which is okay according to the standard
 */
#define SIP_TRANS_TIMERS 3

class timer;


class sip_trans
{
    timer* timers[SIP_TRANS_TIMERS];

    /**
     * Resets a specific timer
     *
     * @param t the new timer
     * @param timer_type @see sip_timer_type
    */
    void reset_timer(timer* t, unsigned int timer_type);    

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

    /**
     * Retransmission buffer
     *  - UAS transaction: ACK
     *  - UAC transaction: final reply
     */
    char* retr_buf;

    /** Length of the retransmission buffer */
    int   retr_len;

    /** Destination for retransmissions */
    sockaddr_storage retr_addr;

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
    timer* get_timer(unsigned int timer_type);

    
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
     * Clears a specfic timer
     *
     * @param timer_type @see sip_timer_type
     */
    void clear_timer(unsigned int timer_type);

    /**
     * Resets every timer
    */
    void reset_all_timers();

    sip_trans();
    ~sip_trans();
};

#endif
