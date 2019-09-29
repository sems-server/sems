/*
 * $Id: hash_table.cpp 1713 2010-03-30 14:11:14Z rco $
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


#include "trans_table.h"
#include "sip_parser.h"
#include "parse_header.h"
#include "parse_cseq.h"
#include "parse_via.h"
#include "parse_from_to.h"
#include "parse_100rel.h"
#include "sip_trans.h"
#include "hash.h"

#include "log.h"

#include <assert.h>

//
// Global transaction table
//

hash_table<trans_bucket> _trans_table(H_TABLE_ENTRIES);

trans_bucket::trans_bucket(unsigned long id)
    : ht_bucket<sip_trans>::ht_bucket(id)
{
}

trans_bucket::~trans_bucket()
{
}

// return true if equal
static inline bool compare_branch(sip_trans* t, sip_msg* msg,
				  const char* branch, unsigned int branch_len)
{
    if(t->msg->via_p1->branch.len != branch_len + MAGIC_BRANCH_LEN)
	return false;

    if(t->msg->via_p1->host.len != 
       msg->via_p1->host.len)
	return false;

    if(t->msg->via_p1->port.len != 
       msg->via_p1->port.len)
	return false;

    if(memcmp(branch,
	      t->msg->via_p1->branch.s+MAGIC_BRANCH_LEN,branch_len))
	return false;

    if(memcmp(t->msg->via_p1->host.s,
	      msg->via_p1->host.s,msg->via_p1->host.len))
	return false;

    if(memcmp(t->msg->via_p1->port.s,
	      msg->via_p1->port.s,msg->via_p1->port.len))
	return false;

    return true;
}


sip_trans* trans_bucket::match_request(sip_msg* msg, unsigned int ttype)
{
    // assert(msg && msg->cseq && msg->callid);
    // sip_cseq* cseq  = dynamic_cast<sip_cseq*>(msg->cseq->p);
    // assert(cseq);

    DBG("Matching %.*s request\n",
	msg->u.request->method_str.len,
	msg->u.request->method_str.s);

    //this should have been checked before
    assert(msg->via_p1);

    if(elmts.empty())
	return NULL;

    bool do_3261_match = false;
    sip_trans* t = NULL;

    // Try first RFC 3261 matching
    if(msg->via_p1->branch.len > MAGIC_BRANCH_LEN){

	do_3261_match = !memcmp(msg->via_p1->branch.s,
				MAGIC_BRANCH_COOKIE,
				MAGIC_BRANCH_LEN);
    }

    DBG("do_3261_match = %i\n",do_3261_match);
    if(do_3261_match){
	
	const char* branch = msg->via_p1->branch.s + MAGIC_BRANCH_LEN;
	int   len = msg->via_p1->branch.len - MAGIC_BRANCH_LEN;
	
	trans_list::iterator it = elmts.begin();
	for(;it!=elmts.end();++it) {
	    
	    if( ((*it)->msg->type != SIP_REQUEST) ||
		((*it)->type != ttype)){
		continue;
	    }

	    if(msg->u.request->method != (*it)->msg->u.request->method) {

		// ACK is the only request that should match an existing
		// transaction without being a re-transmission
		if( ((*it)->msg->u.request->method == sip_request::INVITE)
		    && (msg->u.request->method == sip_request::ACK)) {
		
		    // match non-200 ACK first
		    if(compare_branch(*it,msg,branch,(unsigned int)len)) {
			t = *it;
			break;
		    }

		    // branches do not match,
		    // try to match a 200-ACK
		    if((t = match_200_ack(*it,msg)) != NULL)
			break;
		}

		continue;
	    }

	    if(!compare_branch(*it,msg,branch,(unsigned int)len))
		continue;

	    // found matching transaction
	    t = *it; 
	    break;
	}
    }
    else {

	// Pre-3261 matching

	sip_from_to* from = dynamic_cast<sip_from_to*>(msg->from->p);
	sip_from_to* to = dynamic_cast<sip_from_to*>(msg->to->p);
	sip_cseq* cseq = dynamic_cast<sip_cseq*>(msg->cseq->p);

	assert(from && to && cseq);

	trans_list::iterator it = elmts.begin();
	for(;it!=elmts.end();++it) {

	    
	    //Request matching:
	    // Request-URI
	    // From-tag
	    // Call-ID
	    // Cseq
	    // top Via
	    // To-tag
	    
	    //ACK matching:
	    // Request-URI
	    // From-tag
	    // Call-ID
	    // Cseq (number only)
	    // top Via
	    // + To-tag of reply

	    if( ((*it)->msg->type != SIP_REQUEST) ||
		((*it)->type != ttype)){
		continue;
	    }

	    if( (msg->u.request->method != (*it)->msg->u.request->method) &&
		( (msg->u.request->method != sip_request::ACK) ||
		  ((*it)->msg->u.request->method != sip_request::INVITE) ) )
		continue;

	    sip_from_to* it_from = dynamic_cast<sip_from_to*>((*it)->msg->from->p);
	    if(from->tag.len != it_from->tag.len)
		continue;

	    sip_cseq* it_cseq = dynamic_cast<sip_cseq*>((*it)->msg->cseq->p);
	    if(cseq->num_str.len != it_cseq->num_str.len)
		continue;

	    if(memcmp(from->tag.s,it_from->tag.s,from->tag.len))
		continue;

	    if(memcmp(cseq->num_str.s,it_cseq->num_str.s,cseq->num_str.len))
		continue;

	    
	    if(msg->u.request->method == sip_request::ACK){
		
		// ACKs must include To-tag from previous reply
		if(to->tag.len != (*it)->to_tag.len)
		    continue;

		if(memcmp(to->tag.s,(*it)->to_tag.s,to->tag.len))
		    continue;

		if((*it)->reply_status < 300){

		    // 2xx ACK matching

		    // TODO: additional work for dialog matching???
		    //      R-URI should match reply Contact ...
		    //      Anyway, we don't keep the contact from reply.
		    t = *it;
		    break;
		}
	    }
	    else { 
		// non-ACK
		sip_from_to* it_to = dynamic_cast<sip_from_to*>((*it)->msg->to->p);
		if(to->tag.len != it_to->tag.len)
		    continue;

		if(memcmp(to->tag.s,it_to->tag.s,to->tag.len))
		    continue;
	    }

	    // non-ACK and non-2xx ACK matching

	    if((*it)->msg->u.request->ruri_str.len != 
	       msg->u.request->ruri_str.len )
		continue;
	    
	    if(memcmp(msg->u.request->ruri_str.s,
		      (*it)->msg->u.request->ruri_str.s,
		      msg->u.request->ruri_str.len))
		continue;
	    
	    //TODO: missing top-Via matching
	    
	    // found matching transaction
	    t = *it;
	    break;
	}
    }

    return t;
}

sip_trans* trans_bucket::match_reply(sip_msg* msg)
{

    if(elmts.empty())
	return NULL;

    assert(msg->via_p1);
    if(msg->via_p1->branch.len <= MAGIC_BRANCH_LEN){
	// this cannot match...
	return NULL;
    }
    
    sip_trans* t = NULL;

    const char* branch = msg->via_p1->branch.s + MAGIC_BRANCH_LEN;
    int   len = msg->via_p1->branch.len - MAGIC_BRANCH_LEN;
    
    assert(get_cseq(msg));

    trans_list::iterator it = elmts.begin();
    for(;it!=elmts.end();++it) {
	
	if((*it)->type != TT_UAC){
	    continue;
	}

	if((*it)->msg->via_p1->branch.len != msg->via_p1->branch.len)
	    continue;
	
	if(get_cseq((*it)->msg)->num_str.len != get_cseq(msg)->num_str.len)
	    continue;

	if(get_cseq((*it)->msg)->method_str.len != get_cseq(msg)->method_str.len)
	    continue;

	if(memcmp((*it)->msg->via_p1->branch.s+MAGIC_BRANCH_LEN,
		  branch,len))
	    continue;

	if(memcmp(get_cseq((*it)->msg)->num_str.s,get_cseq(msg)->num_str.s,
		  get_cseq(msg)->num_str.len))
	    continue;

	if(memcmp(get_cseq((*it)->msg)->method_str.s,get_cseq(msg)->method_str.s,
		  get_cseq(msg)->method_str.len))
	    continue;

	// found matching transaction
	t = *it;
	break;
    }

    return t;
}

sip_trans* trans_bucket::match_200_ack(sip_trans* t, sip_msg* msg)
{
    sip_from_to* from = dynamic_cast<sip_from_to*>(msg->from->p);
    sip_from_to* to = dynamic_cast<sip_from_to*>(msg->to->p);
    sip_cseq* cseq = dynamic_cast<sip_cseq*>(msg->cseq->p);

    assert(from && to && cseq);

    sip_from_to* t_from = dynamic_cast<sip_from_to*>(t->msg->from->p);
    if(from->tag.len != t_from->tag.len)
	return NULL;
    
    sip_cseq* t_cseq = dynamic_cast<sip_cseq*>(t->msg->cseq->p);
    if(cseq->num != t_cseq->num)
	return NULL;

    if(msg->callid->value.len != t->msg->callid->value.len)
	return NULL;

    if(to->tag.len != t->to_tag.len)
	return NULL;
    
    if(memcmp(from->tag.s,t_from->tag.s,from->tag.len))
	return NULL;

    if(memcmp(msg->callid->value.s,t->msg->callid->value.s,
	      msg->callid->value.len))
	return NULL;
    
    if(memcmp(to->tag.s,t->to_tag.s,to->tag.len))
	return NULL;
    
    return t;
}

sip_trans* trans_bucket::match_1xx_prack(sip_msg* msg)
{
    DBG("Matching %.*s request\n",
	msg->u.request->method_str.len,
	msg->u.request->method_str.s);

    if(elmts.empty())
	return NULL;
    
    trans_list::iterator it = elmts.begin();
    for(;it!=elmts.end();++it) {
	    
	if( (*it)->msg->type != SIP_REQUEST ){
	    continue;
	}
	sip_trans* t = *it;

	/* first, check quickly if lenghts match (From tag, To tag, Call-ID) */

	sip_from_to* from = dynamic_cast<sip_from_to*>(msg->from->p);
	sip_from_to* t_from = dynamic_cast<sip_from_to*>(t->msg->from->p);
	if(from->tag.len != t_from->tag.len)
	    continue;

	sip_from_to* to = dynamic_cast<sip_from_to*>(msg->to->p);
	if(to->tag.len != t->to_tag.len)
	    continue;
	
	if(msg->callid->value.len != t->msg->callid->value.len)
	    continue;

	sip_rack *rack = dynamic_cast<sip_rack *>(msg->rack->p);
	sip_cseq* t_cseq = dynamic_cast<sip_cseq*>(t->msg->cseq->p);
	if (rack->cseq != t_cseq->num)
	    continue;
	
	if (rack->rseq != t->last_rseq)
	    continue;
	
	if (rack->method != t->msg->u.request->method)
	    continue;
		
	/* numbers fit, try content */
	if(memcmp(from->tag.s,t_from->tag.s,from->tag.len))
	    continue;
	
	if(memcmp(msg->callid->value.s,t->msg->callid->value.s,
		  msg->callid->value.len))
	    continue;
	
	if(memcmp(to->tag.s,t->to_tag.s,to->tag.len))
	    continue;

	return t;
    }

    return NULL;
}

sip_trans* trans_bucket::find_uac_trans(const cstring& dialog_id,
					unsigned int inv_cseq)
{
    DBG("Matching dialog_id = '%.*s'\n",
	dialog_id.len, dialog_id.s);

    if(elmts.empty())
	return NULL;
    
    trans_list::reverse_iterator it = elmts.rbegin();
    for(;it!=elmts.rend();++it) {
	    
	sip_trans* t = *it;
	if( t->type != TT_UAC ||
	    t->msg->type != SIP_REQUEST ){
	    continue;
	}
	sip_cseq* t_cseq = dynamic_cast<sip_cseq*>(t->msg->cseq->p);
	if(t->dialog_id == dialog_id &&
	   t_cseq && t_cseq->num == inv_cseq &&
	   t->state != TS_ABANDONED)
	    return t;
    }

    return NULL;
}

sip_trans* trans_bucket::add_trans(sip_msg* msg, unsigned int ttype)
{
    sip_trans* t = new sip_trans();

    t->msg  = msg;
    t->type = ttype;

    t->reply_status = 0;

    assert(msg->type == SIP_REQUEST);
    if(msg->u.request->method == sip_request::INVITE){
	
	if(t->type == TT_UAS)
	    t->state = TS_PROCEEDING;
	else
	    t->state = TS_CALLING;
    }
    else {
	t->state = TS_TRYING;
    }

    elmts.push_back(t);
    
    return t;
}

void trans_bucket::append(sip_trans* t)
{
    elmts.push_back(t);
}

unsigned int hash(const cstring& ci, const cstring& cs)
{
    unsigned int h=0;

    h = hashlittle(ci.s,ci.len,h);
    h = hashlittle(cs.s,cs.len,h);

    return h;
}

char _tag_lookup[] = {
    'a','b','c','d','e','f','g','h',
    'i','j','k','l','m','n','o','p',
    'q','r','s','t','u','v','w','x',
    'y','z','A','B','C','D','E','F',
    'G','H','I','J','K','L','M','N',
    'O','P','Q','R','S','T','U','V',
    'W','X','Y','Z','0','1','2','3',
    '4','5','6','7','8','9','.','~'
};

inline void compute_tag(char* tag, unsigned int hl, unsigned int hh)
{
    hl += hh >> 16;
    hh &= 0xFFFF;

    tag[0] = _tag_lookup[hl&0x3F];
    tag[1] = _tag_lookup[(hl >> 6)&0x3F];
    tag[2] = _tag_lookup[(hl >> 12)&0x3F];
    tag[3] = _tag_lookup[(hl >> 18)&0x3F];
    tag[4] = _tag_lookup[(hl >> 24)&0x3F];
    tag[5] = _tag_lookup[(hl >> 30)&((hh << 2) & 0x3F)];
    tag[6] = _tag_lookup[(hh >> 4)&0x3F];
    tag[7] = _tag_lookup[(hh >> 10)&0x3F];
}

static atomic_int __branch_cnt;

void compute_sl_to_tag(char* to_tag/*[8]*/, const sip_msg* msg)
{
    unsigned int hl = __branch_cnt.inc();
    unsigned int hh = __branch_cnt.inc();
    
    assert(msg->type == SIP_REQUEST);
    assert(msg->u.request);

    hl = hashlittle(msg->u.request->method_str.s,
		    msg->u.request->method_str.len,hl);

    hl = hashlittle(msg->u.request->ruri_str.s,
		    msg->u.request->ruri_str.len,hl);

    if(msg->callid){
	hh = hashlittle(msg->callid->value.s,
			msg->callid->value.len,hh);
    }

    if(msg->from){
	hh = hashlittle(msg->from->value.s,
			msg->from->value.len,hh);
    }

    if(msg->cseq){
	hh = hashlittle(msg->cseq->value.s,
			msg->cseq->value.len,hh);
    }
    
    compute_tag(to_tag,hl,hh);
}

void compute_branch(char* branch/*[8]*/, const cstring& callid, const cstring& cseq)
{
    unsigned int hl=0;
    unsigned int hh=0;

    hh = __branch_cnt.inc();
    hl = __branch_cnt.inc();

    hl = hashlittle(callid.s,callid.len,hl);
    hh = hashlittle(cseq.s,cseq.len,hh);

    compute_tag(branch,hl,hh);
}

trans_bucket* get_trans_bucket(const cstring& callid, const cstring& cseq_num)
{
    return _trans_table[hash(callid,cseq_num)];
}

trans_bucket* get_trans_bucket(unsigned int h)
{
    return _trans_table[h];
}

void dumps_transactions()
{
    _trans_table.dump();
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
