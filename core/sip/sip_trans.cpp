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
#include "sip_timers.h"
#include "sip_parser.h"
#include "wheeltimer.h"
#include "trans_table.h"
#include "trans_layer.h"
#include "transport.h"
#include "msg_logger.h"
#include "ip_util.h"

#include "log.h"

#include <assert.h>

int _timer_type_lookup[] = { 
    0,1,2, // STIMER_A, STIMER_B, STIMER_D
    0,1,2, // STIMER_E, STIMER_F, STIMER_K
    0,1,2, // STIMER_G, STIMER_H, STIMER_I
    0,     // STIMER_J
    2,     // STIMER_L; shares the same slot as STIMER_D
    2,     // STIMER_M; shares the same slot as STIMER_D/STIMER_K
    2,     // STIMER_C; shares the same slot at STIMER_D (INV trans only)
    2,     // STIMER_BL; share the same slot as STIMER_D/STIMER_K
};

inline trans_timer** fetch_timer(unsigned int timer_type, trans_timer** base)
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
      last_rseq(0),
      targets(NULL),
      retr_buf(NULL),
      retr_len(0),
      retr_socket(NULL),
      logger(NULL),
      canceled(false)
{
    memset(timers,0,SIP_TRANS_TIMERS*sizeof(void*));
}

sip_trans::~sip_trans() 
{
    reset_all_timers();
    delete msg;
    delete targets;
    delete [] retr_buf;
    if(retr_socket){
	dec_ref(retr_socket);
    }
    if((type == TT_UAC) && to_tag.s){
	delete [] to_tag.s;
    }
    if(dialog_id.s) {
	delete [] dialog_id.s;
    }
    if(logger) {
	dec_ref(logger);
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

    int send_err = retr_socket->send(&retr_addr,retr_buf,retr_len,flags);
    if(send_err < 0){
	ERROR("Error from transport layer\n");
    }

    if(logger) {
	sockaddr_storage src_ip;
	retr_socket->copy_addr_to(&src_ip);
	logger->log(retr_buf,retr_len,
		    &src_ip,&retr_addr,
		    msg->u.request->method_str,
		    reply_status);
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
trans_timer* sip_trans::get_timer(unsigned int timer_type)
{
    return *fetch_timer(timer_type,timers);
}


const char* _trans_type_lookup[] = {
    "0",
    "UAS",
    "UAC"
};

#define trans_type(type) \
    (_trans_type_lookup[type])

const char* _state_name_lookup[] = {
    "0",
    "TRYING",
    "CALLING",
    "PROCEEDING",
    "PROCEEDING_REL",
    "COMPLETED",
    "CONFIRMED",
    "TERMINATED_200",
    "TERMINATED",
    "ABANDONED",
    "REMOVED"
};

#define state_name(state) \
    (_state_name_lookup[state])

/**
 * Resets a specific timer
 *
 * @param t the new timer
 * @param timer_type @see sip_timer_type
 */
void sip_trans::reset_timer(trans_timer* t, unsigned int timer_type)
{
    trans_timer** tp = fetch_timer(timer_type,timers);
    
    if(*tp != NULL){

	DBG("Clearing old timer of type %s (this=%p)\n",
	    timer_name((*tp)->type),*tp);
	wheeltimer::instance()->remove_timer((timer*)*tp);
    }

    *tp = t;

    if(t)
	wheeltimer::instance()->insert_timer((timer*)t);
}

void trans_timer::fire()
{
    trans_bucket* bucket = get_trans_bucket(bucket_id);
    if(bucket){
	bucket->lock();
	if(bucket->exist(t)){
	    DBG("Transaction timer expired: type=%s, trans=%p, eta=%i, t=%i\n",
		timer_name(type),t,expires,wheeltimer::instance()->wall_clock);

	    trans_timer* tt = t->get_timer(this->type & 0xFFFF);
	    if(tt != this) {
		// timer has been reset while very close to firing!!!
		// 1. it is not yet deleted in the wheeltimer
		// 2. we have already set a new one
		// -> anyway, don't fire the old one !!!
                bucket->unlock();
		return;
	    }

	    // timer_expired unlocks the bucket
	    trans_layer::instance()->timer_expired(this,bucket,t);
	}
	else {
	    WARN("Ignoring expired timer (%p/%s): transaction"
		 " %p does not exist anymore\n",this,timer_name(type),t);
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
    
    DBG("New timer of type %s at time=%i (repeated=%i)\n",
	timer_name(timer_type),expires,timer_type>>16);

    trans_timer* t = new trans_timer(timer_type,expires,
				     bucket_id,this);

    reset_timer(t,timer_type);
}

void sip_trans::clear_timer(unsigned int timer_type)
{
    reset_timer((trans_timer*)NULL,timer_type);
}

void sip_trans::reset_all_timers()
{
    for(int i=0; i<SIP_TRANS_TIMERS; i++){
	
	if(timers[i]){
	    wheeltimer::instance()->remove_timer((timer*)timers[i]);
	    timers[i] = NULL;
	}
    }
}

const char* sip_trans::type_str() const
{
    return trans_type(type);
}

const char* sip_trans::state_str() const
{
    return state_name(state);
}

void sip_trans::dump() const
{
    DBG("type=%s (0x%x); msg=%p; to_tag=%.*s;"
	" reply_status=%i; state=%s (%i); retr_buf=%p; timers [%s,%s,%s]\n",
	type_str(),type,msg,to_tag.len,to_tag.s,
	reply_status,state_str(),state,retr_buf,
	timers[0]==NULL?"none":timer_name(timers[0]->type),
	timers[1]==NULL?"none":timer_name(timers[1]->type),
	timers[2]==NULL?"none":timer_name(timers[2]->type));
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
