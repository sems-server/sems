/*
 * $Id: hash_table.h 1486 2009-08-29 14:40:38Z rco $
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
#include "../AmThread.h"
#include "../log.h"

#include <list>
using std::list;

struct sip_trans;
struct sip_msg;


template<class Value>
class ht_bucket: public AmMutex
{
public:
    typedef list<Value*> value_list;

    ht_bucket(unsigned long id) : id(id) {}
    ~ht_bucket() {}
    
    /**
     * Caution: The bucket MUST be locked before you can 
     * do anything with it.
     */

    /**
     * Searches for the value ptr in this bucket.
     * This is used to check if the value
     * still exists.
     *
     * @return true if the value still exists.
     */
    bool exist(Value* t) {
	return find(t) != elmts.end();
    }
    
    /**
     * Remove the value from this bucket,
     * if it was still present.
     */
    void remove(Value* t) {
    typename value_list::iterator it = find(t);

    if(it != elmts.end()){
	elmts.erase(it);
	delete t;
	DBG("~sip_trans()\n");
    }
    }

    /**
     * Returns the bucket id, which should be an index
     * into the corresponding hash table.
     */
    unsigned long get_id() const {
	return id;
    }

    // debug method
    void dump() const {

	if(elmts.empty())
	    return;
	
	DBG("*** Bucket ID: %i ***\n",(int)get_id());
	
	for(typename value_list::const_iterator it = elmts.begin(); it != elmts.end(); ++it) {
	    
	    (*it)->dump();
	}
    }

protected:
    /**
     * Finds a transaction ptr in this bucket.
     * This is used to check if the transaction
     * still exists.
     *
     * @return iterator pointing at the value.
     */
    typename value_list::iterator find(Value* t)
    {
	typename value_list::iterator it = elmts.begin();
	for(;it!=elmts.end();++it)
	    if(*it == t)
		break;
	
	return it;
    }

    unsigned long  id;
    value_list     elmts;
};

template<class Bucket, unsigned long size>
class hash_table
{
    Bucket* _table[size];

public:
    hash_table() {
	for(unsigned long i=0; i<size; i++)
	    _table[i] = new Bucket(i);
    }

    ~hash_table() {
	for(unsigned long i=0; i<size; i++)
	    delete _table[i];
    }

    Bucket* get_bucket(unsigned long hash) const {
	return _table[hash % size];
    }

    Bucket* operator [](unsigned long hash) const {
	return get_bucket(hash);
    }
};



#endif


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
