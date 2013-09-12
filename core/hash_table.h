/*
 * $Id: hash_table.h 1486 2009-08-29 14:40:38Z rco $
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


#ifndef _hash_table_h
#define _hash_table_h

#include "AmThread.h"
#include "log.h"

#include <list>
#include <map>
using std::list;
using std::map;
using std::less;


template<class Value>
class ht_bucket: public AmMutex
{
public:
    typedef list<Value*> value_list;

    ht_bucket(unsigned long id) : id(id) {}
    virtual ~ht_bucket() {}
    
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

template<class Value>
class ht_delete
{
public:
    Value* new_elmt(Value* v) {
	return v;
    }
    void dispose(Value* v) {
	delete v;
    }
};

template<class Value>
class ht_ref_cnt
{
public:
    Value* new_elmt(Value* v) {
	inc_ref(v);
	return v;
    }
    void dispose(Value* v) {
	dec_ref(v);
    }
};

template<class Key, class Value, 
	 class ElmtAlloc = ht_delete<Value>,
	 class ElmtCompare = less<Key> >
class ht_map_bucket: public AmMutex
{
public:
    typedef map<Key,Value*,ElmtCompare> value_map;
    typedef ElmtAlloc                   allocator;

    ht_map_bucket(unsigned long id) : id(id) {}
    virtual ~ht_map_bucket() {}
    
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
    bool exist(const Key& k) {
	return find(k) != elmts.end();
    }

    /**
     * Insert the value into this bucket.
     */
    virtual bool insert(const Key& k, Value* v) {
	v = ElmtAlloc().new_elmt(v);
	bool res = elmts.insert(typename value_map::value_type(k,v)).second;
	if(!res) ElmtAlloc().dispose(v);
	return res;
    }

    /**
     * Remove the value from this bucket,
     * if it was still present.
     */
    virtual bool remove(const Key& k) {
	typename value_map::iterator it = find(k);

	if(it != elmts.end()){
	    Value* v = it->second;
	    elmts.erase(it);
	    ElmtAlloc().dispose(v);
	    return true;
	}
	return false;
    }

    Value* get(const Key& k) {
	typename value_map::iterator it = find(k);
	if(it != elmts.end())
	    return it->second;
	return NULL;
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
	
	for(typename value_map::const_iterator it = elmts.begin(); 
	    it != elmts.end(); ++it) {
	    dump_elmt(it->first,it->second);
	}
    }

    virtual void dump_elmt(const Key& k, const Value* v) const {}

protected:
    typename value_map::iterator find(const Key& k)
    {
	return elmts.find(k);
    }

    unsigned long  id;
    value_map     elmts;
};

template<class Bucket>
class hash_table
{
    unsigned long size;
    Bucket**    _table;

public:
    hash_table(unsigned long size)
	: size(size)
    {
	_table = new Bucket* [size];
	for(unsigned long i=0; i<size; i++)
	    _table[i] = new Bucket(i);
    }

    ~hash_table() {
	for(unsigned long i=0; i<size; i++)
	    delete _table[i];
	delete [] _table;
    }

    Bucket* operator [](unsigned long hash) const {
	return get_bucket(hash);
    }

    Bucket* get_bucket(unsigned long hash) const {
	return _table[hash % size];
    }

    void dump() const {
	for(unsigned long l=0; l<size; l++){
	    _table[l]->lock();
	    _table[l]->dump();
	    _table[l]->unlock();
	}
    }

    unsigned long get_size() { return size; }
};



#endif


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
