/*
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

#include "AmEventDispatcher.h"
#include "AmSipEvent.h"
#include "AmConfig.h"
#include "AmThread.h"
#include "sip/hash.h"

unsigned int AmEventDispatcher::hash(const string& s1)
{
  return hashlittle(s1.c_str(),s1.length(),0) 
    & (EVENT_DISPATCHER_BUCKETS-1);
}

unsigned int AmEventDispatcher::hash(const string& s1, const string s2)
{
    unsigned int h=0;

    h = hashlittle(s1.c_str(),s1.length(),h);
    h = hashlittle(s2.c_str(),s2.length(),h);

    return h & (EVENT_DISPATCHER_BUCKETS-1);
}

AmEventDispatcher* AmEventDispatcher::_instance=NULL;

AmEventDispatcher* AmEventDispatcher::instance()
{
  return _instance ? _instance : ((_instance = new AmEventDispatcher()));
}


bool AmEventDispatcher::addEventQueue(const string& local_tag,
				      AmEventQueueInterface* q)
{
    unsigned int queue_bucket = hash(local_tag);

    AmLock queues_lock(queues_mut[queue_bucket]);

    if (queues[queue_bucket].find(local_tag) != queues[queue_bucket].end()) {
      return false;
    }

    queues[queue_bucket][local_tag] = QueueEntry(q);

    return true;
}


/** @return false on error */
bool AmEventDispatcher::addEventQueue(const string& local_tag, 
				      AmEventQueueInterface* q,
				      const string& callid, 
				      const string& remote_tag,
				      const string& via_branch)
{
    if(local_tag.empty () ||callid.empty() || remote_tag.empty() || via_branch.empty()) {
      ERROR("local_tag, callid, remote_tag or via_branch is empty");
      return false;
    }

    unsigned int queue_bucket = hash(local_tag);

    AmLock queues_lock(queues_mut[queue_bucket]);

    if (queues[queue_bucket].find(local_tag) != queues[queue_bucket].end()) {
      return false;
    }

    // try to find via id_lookup
    string id = callid+remote_tag;
    if(AmConfig::AcceptForkedDialogs){
      id += via_branch;
    }
    unsigned int id_bucket = hash(id);

    // queues_mut before id_lookup_mut, as everywhere else that takes both
    AmLock id_lookup_lock(id_lookup_mut[id_bucket]);

    if (id_lookup[id_bucket].find(id) != 
	id_lookup[id_bucket].end()) {
      return false;
    }

    queues[queue_bucket][local_tag] = QueueEntry(q,id);
    id_lookup[id_bucket][id] = local_tag;

    return true;
}

AmEventQueueInterface* AmEventDispatcher::delEventQueue(const string& local_tag)
{
    AmEventQueueInterface* q = NULL;
    unsigned int queue_bucket = hash(local_tag);

    AmLock queues_lock(queues_mut[queue_bucket]);

    EvQueueMapIter qi = queues[queue_bucket].find(local_tag);
    if(qi != queues[queue_bucket].end()) {

      QueueEntry qe(qi->second);
      queues[queue_bucket].erase(qi);
      q = qe.q;
      
      if(!qe.id.empty()) {
	unsigned int id_bucket = hash(qe.id);
	
	AmLock id_lookup_lock(id_lookup_mut[id_bucket]);
	
	DictIter di = id_lookup[id_bucket].find(qe.id);
	if(di != id_lookup[id_bucket].end()) {	    
	  id_lookup[id_bucket].erase(di);
	}
      }
    }

    return q;
}

bool AmEventDispatcher::post(const string& local_tag, AmEvent* ev)
{
    bool posted = false;
  
    unsigned int queue_bucket = hash(local_tag);

    AmLock queues_lock(queues_mut[queue_bucket]);

    EvQueueMapIter it = queues[queue_bucket].find(local_tag);
    if(it != queues[queue_bucket].end()){
	it->second.q->postEvent(ev);
	posted = true;
    }

    return posted;
}


bool AmEventDispatcher::post(const string& callid, 
			     const string& remote_tag, 
			     const string& via_branch,
			     AmEvent* ev)
{
    string id = callid+remote_tag;
    if(AmConfig::AcceptForkedDialogs){
      id += via_branch;
    }
    unsigned int id_bucket = hash(id);

    string local_tag;
    {
      AmLock id_lookup_lock(id_lookup_mut[id_bucket]);

      DictIter di = id_lookup[id_bucket].find(id);
      if (di == id_lookup[id_bucket].end()) {
	return false;
      }
      local_tag = di->second;
    }

    return post(local_tag, ev);
}

bool AmEventDispatcher::broadcast(AmEvent* ev)
{
    if (!ev)
      return false;

    bool posted = false;
    for (size_t i=0;i<EVENT_DISPATCHER_BUCKETS;i++) {
      // kept as an explicit lock/unlock pair: postEvent() is deliberately
      // called with the bucket released, so an exception escaping from it
      // leaves this mutex unlocked rather than held
      queues_mut[i].lock();

      EvQueueMapIter it = queues[i].begin(); 
      while (it != queues[i].end()) {
	EvQueueMapIter this_evq = it;
	it++;
	queues_mut[i].unlock();
	this_evq->second.q->postEvent(ev->clone());
	queues_mut[i].lock();
	posted = true;
      }
      queues_mut[i].unlock();
    }

    delete ev;

    return posted;
}

bool AmEventDispatcher::empty() {
    bool res = true;
    for (size_t i=0;i<EVENT_DISPATCHER_BUCKETS;i++) {
      {
	AmLock queues_lock(queues_mut[i]);
	res = res&queues[i].empty();
      }
      if (!res)
	break;
    }
    return res;  
}

void AmEventDispatcher::dump()
{
    DBG("*** dumping Event dispatcher buckets ***\n");
    for (size_t i=0;i<EVENT_DISPATCHER_BUCKETS;i++) {
      {
	AmLock queues_lock(queues_mut[i]);
	if(!queues[i].empty()) {
	  DBG("queues[%zu].size() = %zu",i,queues[i].size());
	  for(EvQueueMapIter it = queues[i].begin();
	      it != queues[i].end(); it++){
	    DBG("\t%s -> %p\n",it->first.c_str(),it->second.q);
	  }
	}
      }

      AmLock id_lookup_lock(id_lookup_mut[i]);
      if(!id_lookup[i].empty()) {
	DBG("id_lookup[%zu].size() = %zu",i,id_lookup[i].size());
      }
    }
    DBG("*** End of Event dispatcher bucket dump ***\n");
}

void AmEventDispatcher::dispose() 
{
  if(_instance != NULL) {
    // todo: add locking here
    _instance->dump();

    delete _instance;
    _instance = NULL;
  }
}

/** this function optimizes posting of SIP Requests 
    - if the session does not exist, no event need to be created (req copied) */
bool AmEventDispatcher::postSipRequest(const AmSipRequest& req)
{
    // get local tag
    bool posted = false;
    string callid = req.callid;
    string remote_tag = req.from_tag;

    string id = callid+remote_tag;
    if(AmConfig::AcceptForkedDialogs){
      id += req.via_branch;
    }
    unsigned int id_bucket = hash(id);

    string local_tag;
    {
      AmLock id_lookup_lock(id_lookup_mut[id_bucket]);

      DictIter di = id_lookup[id_bucket].find(id);
      if (di == id_lookup[id_bucket].end()) {
	return false;
      }
      local_tag = di->second;
    }

    // post(local_tag)
    unsigned int queue_bucket = hash(local_tag);

    AmLock queues_lock(queues_mut[queue_bucket]);

    EvQueueMapIter it = queues[queue_bucket].find(local_tag);
    if(it != queues[queue_bucket].end()){
	it->second.q->postEvent(new AmSipRequestEvent(req));
	posted = true;
    }

    return posted;
}
