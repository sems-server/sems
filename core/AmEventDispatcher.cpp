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

    queues_mut[queue_bucket].lock();

    if (queues[queue_bucket].find(local_tag) != queues[queue_bucket].end()) {
      queues_mut[queue_bucket].unlock();
      return false;
    }

    queues[queue_bucket][local_tag] = q;
    queues_mut[queue_bucket].unlock();
    
    return true;
}


/** @return false on error */
bool AmEventDispatcher::addEventQueue(const string& local_tag, 
				      AmEventQueueInterface* q,
				      const string& callid, 
				      const string& remote_tag,
				      const string& via_branch)
{
    if(local_tag.empty () ||callid.empty() || remote_tag.empty() | via_branch.empty()) {
      ERROR("local_tag, callid, remote_tag or via_branch is empty");
      return false;
    }

    unsigned int queue_bucket = hash(local_tag);

    queues_mut[queue_bucket].lock();

    if (queues[queue_bucket].find(local_tag) != queues[queue_bucket].end()) {
      queues_mut[queue_bucket].unlock();
      return false;
    }

    // try to find via id_lookup
    unsigned int id_bucket = hash(callid, remote_tag);
    string id = callid+remote_tag;
    if(AmConfig::AcceptForkedDialogs){
      id += via_branch;
    }

    id_lookup_mut[id_bucket].lock();
    
    if (id_lookup[id_bucket].find(id) != 
	id_lookup[id_bucket].end()) {
      id_lookup_mut[id_bucket].unlock();
      queues_mut[queue_bucket].unlock();
      return false;
    }

    queues[queue_bucket][local_tag] = q;
    id_lookup[id_bucket][id] = local_tag;

    id_lookup_mut[id_bucket].unlock();
    queues_mut[queue_bucket].unlock();
    
    return true;
}

AmEventQueueInterface* AmEventDispatcher::delEventQueue(const string& local_tag)
{
    AmEventQueueInterface* q = NULL;
    
    unsigned int queue_bucket = hash(local_tag);

    queues_mut[queue_bucket].lock();
    
    EvQueueMapIter qi = queues[queue_bucket].find(local_tag);
    if(qi != queues[queue_bucket].end()) {

	q = qi->second;
	queues[queue_bucket].erase(qi);
    }
    queues_mut[queue_bucket].unlock();
    
    return q;
}

AmEventQueueInterface* AmEventDispatcher::delEventQueue(const string& local_tag,
							const string& callid, 
							const string& remote_tag,
							const string& via_branch)
{
    AmEventQueueInterface* q = NULL;
    
    unsigned int queue_bucket = hash(local_tag);

    queues_mut[queue_bucket].lock();
    
    EvQueueMapIter qi = queues[queue_bucket].find(local_tag);
    if(qi != queues[queue_bucket].end()) {

	q = qi->second;
	queues[queue_bucket].erase(qi);

	if(!callid.empty() && !remote_tag.empty() && !via_branch.empty()) {
	  unsigned int id_bucket = hash(callid, remote_tag);
	  string id = callid+remote_tag;
	  if(AmConfig::AcceptForkedDialogs){
	    id += via_branch;
	  }

	  id_lookup_mut[id_bucket].lock();

	  DictIter di = id_lookup[id_bucket].find(id);
	  if(di != id_lookup[id_bucket].end()) {	    
	    id_lookup[id_bucket].erase(di);
	  }

	  id_lookup_mut[id_bucket].unlock();
	}
    }
    queues_mut[queue_bucket].unlock();
    
    return q;
}

bool AmEventDispatcher::post(const string& local_tag, AmEvent* ev)
{
    bool posted = false;
  
    unsigned int queue_bucket = hash(local_tag);
  
    queues_mut[queue_bucket].lock();
 
    EvQueueMapIter it = queues[queue_bucket].find(local_tag);
    if(it != queues[queue_bucket].end()){
	it->second->postEvent(ev);
	posted = true;
    }

    queues_mut[queue_bucket].unlock();
    
    return posted;
}


bool AmEventDispatcher::post(const string& callid, 
			     const string& remote_tag, 
			     const string& via_branch,
			     AmEvent* ev)
{
    unsigned int id_bucket = hash(callid, remote_tag);
    string id = callid+remote_tag;
    if(AmConfig::AcceptForkedDialogs){
      id += via_branch;
    }

    id_lookup_mut[id_bucket].lock();

    DictIter di = id_lookup[id_bucket].find(id);
    if (di == id_lookup[id_bucket].end()) {
      id_lookup_mut[id_bucket].unlock();
      return false;
    }
    string local_tag = di->second;
    id_lookup_mut[id_bucket].unlock();
 
    return post(local_tag, ev);
}

bool AmEventDispatcher::broadcast(AmEvent* ev)
{
    if (!ev)
      return false;

    bool posted = false;
    for (size_t i=0;i<EVENT_DISPATCHER_BUCKETS;i++) {
      queues_mut[i].lock();

      EvQueueMapIter it = queues[i].begin(); 
      while (it != queues[i].end()) {
	EvQueueMapIter this_evq = it;
	it++;
	queues_mut[i].unlock();
	this_evq->second->postEvent(ev->clone());
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
      queues_mut[i].lock();
      res = res&queues[i].empty();
      queues_mut[i].unlock();    
      if (!res)
	break;
    }
    return res;  
}

void AmEventDispatcher::dispose() 
{
  if(_instance != NULL) {
    // todo: add locking here
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
    unsigned int id_bucket = hash(callid, remote_tag);
    string id = callid+remote_tag;
    if(AmConfig::AcceptForkedDialogs){
      id += req.via_branch;
    }

    id_lookup_mut[id_bucket].lock();

    DictIter di = id_lookup[id_bucket].find(id);
    if (di == id_lookup[id_bucket].end()) {
      id_lookup_mut[id_bucket].unlock();
      return false;
    }
    string local_tag = di->second;
    id_lookup_mut[id_bucket].unlock();
 
    // post(local_tag)
    unsigned int queue_bucket = hash(local_tag);
  
    queues_mut[queue_bucket].lock();
 
    EvQueueMapIter it = queues[queue_bucket].find(local_tag);
    if(it != queues[queue_bucket].end()){
	it->second->postEvent(new AmSipRequestEvent(req));
	posted = true;
    }

    queues_mut[queue_bucket].unlock();
    
    return posted;
}
