/*
 * $Id: sip_trans.cpp 1712 2010-03-30 13:05:58Z rco $
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

#include "sip_trans.h"
#include "sip_parser.h"
#include "wheeltimer.h"
#include "trans_table.h"
#include "trans_layer.h"
#include "transport.h"

#include "log.h"

#include <assert.h>

int _timer_type_lookup[] = { 
    -1,    // STIMER_INVALID
    0,1,2, // STIMER_A, STIMER_B, STIMER_D
    0,1,2, // STIMER_E, STIMER_F, STIMER_K
    0,1,2, // STIMER_G, STIMER_H, STIMER_I
    0,     // STIMER_J
    2,     // STIMER_L; shares the same slot as STIMER_D
    2,     // STIMER_M; shares the same slot as STIMER_D/STIMER_K
    1,     // STIMER_C; shares the same slot at STIMER_B (INV trans only)
};

inline timer** fetch_timer(unsigned int timer_type, timer** base)
{
    timer_type &= 0xFFFF;

    assert(timer_type < sizeof(_timer_type_lookup));

    int tl = _timer_type_lookup[timer_type];
    if(tl != -1){
	return &base[tl];
    }

    return NULL;
}

sip_trans::sip_trans()
    : msg(NULL),
      retr_buf(NULL),
      retr_socket(NULL),
      retr_len(0),
      last_rseq(0)
{
    memset(timers,0,SIP_TRANS_TIMERS*sizeof(void*));
}

sip_trans::~sip_trans() 
{
    reset_all_timers();
    delete msg;
    delete [] retr_buf;
    if((type == TT_UAC) && to_tag.s){
	delete [] to_tag.s;
    }
}

/**
 * Retransmits the content of the retry buffer (replies or non-200 ACK).
 */
void sip_trans::retransmit()
{
    if(!retr_buf || !retr_len){
	// there is nothing to re-transmit yet!!!
	return;
    }
    assert(retr_socket);

    int send_err = retr_socket->send(&retr_addr,retr_buf,retr_len);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
    }
}

/**
 * Tells if a specific timer is set
 *
 * @param timer_type @see sip_timer_type
 */
bool sip_trans::is_timer_set(unsigned int timer_type)
{
    return (*fetch_timer(timer_type,timers) != NULL);
}

/**
 * Fetches a specific timer
 *
 * @param timer_type @see sip_timer_type
 */
timer* sip_trans::get_timer(unsigned int timer_type)
{
    return *fetch_timer(timer_type,timers);
}


char _timer_name_lookup[] = {'0','A','B','D','E','F','K','G','H','I','J','L','M','C'};
#define timer_name(type) \
    (_timer_name_lookup[(type) & 0xFFFF])

/**
 * Resets a specific timer
 *
 * @param t the new timer
 * @param timer_type @see sip_timer_type
 */
void sip_trans::reset_timer(timer* t, unsigned int timer_type)
{
    timer** tp = fetch_timer(timer_type,timers);
    
    if(*tp != NULL){

	DBG("Clearing old timer of type %c (this=%p)\n",timer_name((*tp)->type),*tp);
	wheeltimer::instance()->remove_timer(*tp);
    }

    *tp = t;

    if(t)
	wheeltimer::instance()->insert_timer(t);
}

void trans_timer_cb(timer* t, unsigned int bucket_id, sip_trans* tr)
{
    trans_bucket* bucket = get_trans_bucket(bucket_id);
    if(bucket){
	bucket->lock();
	if(bucket->exist(tr)){
	    DBG("Transaction timer expired: type=%c, trans=%p, eta=%i, t=%i\n",
		timer_name(t->type),tr,t->expires,wheeltimer::instance()->wall_clock);

	    // timer_expired unlocks the bucket
	    trans_layer::instance()->timer_expired(t,bucket,tr);
	}
	else {
	    WARN("Ignoring expired timer (%p): transaction"
		 " %p does not exist anymore\n",t,tr);
	    bucket->unlock();
	}
    }
    else {
	ERROR("Invalid bucket id\n");
    }
}

/**
 * Resets a specific timer with a delay value
 *
 * @param timer_type @see sip_timer_type
 * @param expires_delay delay before expiration in millisecond
 */
void sip_trans::reset_timer(unsigned int timer_type, unsigned int expire_delay /* ms */,
			    unsigned int bucket_id)
{
    wheeltimer* wt = wheeltimer::instance();

    unsigned int expires = expire_delay / (TIMER_RESOLUTION/1000);
    expires += wt->wall_clock;
    
    DBG("New timer of type %c at time=%i (repeated=%i)\n",
	timer_name(timer_type),expires,timer_type>>16);

    timer* t = new timer(timer_type,expires,
			 (timer_cb)trans_timer_cb,
			 bucket_id,this);

    reset_timer(t,timer_type);
}

void sip_trans::clear_timer(unsigned int timer_type)
{
    reset_timer((timer*)NULL,timer_type);
}

void sip_trans::reset_all_timers()
{
    for(int i=0; i<SIP_TRANS_TIMERS; i++){
	
	if(timers[i]){
	    wheeltimer::instance()->remove_timer(timers[i]);
	    timers[i] = NULL;
	}
    }
}

void sip_trans::dump() const
{
    DBG("type=0x%x; msg=%p; to_tag=%.*s; reply_status=%i; state=%i; retr_buf=%p\n",
	type,msg,to_tag.len,to_tag.s,reply_status,state,retr_buf);
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
