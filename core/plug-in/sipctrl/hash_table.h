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


#ifndef _hash_table_h
#define _hash_table_h

#include "cstring.h"

#include <pthread.h>

#include <list>
using std::list;

struct sip_trans;
struct sip_msg;


#define H_TABLE_POWER   10
#define H_TABLE_ENTRIES (1<<H_TABLE_POWER)


class trans_bucket
{
public:
    typedef list<sip_trans*> trans_list;

private:

    unsigned long   id;

    pthread_mutex_t m;
    trans_list      elmts;
    
    /**
     * Finds a transaction ptr in this bucket.
     * This is used to check if the transaction
     * still exists.
     *
     * @return iterator pointing at the transaction.
     */
    trans_list::iterator find_trans(sip_trans* t);

    sip_trans* match_200_ack(sip_trans* t,sip_msg* msg);

public:

    /**
     * Kept public to allow for static construction.
     * !!! DO CREATE ANY BUCKETS ON YOUR OWN !!!
     */
    trans_bucket();
    ~trans_bucket();
    
    /**
     * The bucket MUST be locked before you can 
     * do anything with it.
     */
    void lock();

    /**
     * Unlocks the bucket after work has been done.
     */
    void unlock();
    
    // Match a request to UAS transactions
    // in this bucket
    sip_trans* match_request(sip_msg* msg);

    // Match a reply to UAC transactions
    // in this bucket
    sip_trans* match_reply(sip_msg* msg);

    sip_trans* add_trans(sip_msg* msg, int ttype);

    /**
     * Searches for a transaction ptr in this bucket.
     * This is used to check if the transaction
     * still exists.
     *
     * @return true if the transaction still exists.
     */
    bool exist(sip_trans* t);
    
    /**
     * Remove a transaction from this bucket,
     * if it was still present.
     */
    void remove_trans(sip_trans* t);

    unsigned long get_id() {
	return id;
    }


    // debug method
    void dump();
};

trans_bucket* get_trans_bucket(const cstring& callid, const cstring& cseq_num);
trans_bucket* get_trans_bucket(unsigned int h);

unsigned int hash(const cstring& ci, const cstring& cs);


#define BRANCH_BUF_LEN 8

// char branch[BRANCH_BUF_LEN]
void compute_branch(char* branch, const cstring& callid, const cstring& cseq);

void dumps_transactions();

#endif
