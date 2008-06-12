/*
 * $Id: $
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

#include "AmEventDispatcher.h"
#include "AmSipEvent.h"

AmEventDispatcher* AmEventDispatcher::_instance=NULL;

AmEventDispatcher* AmEventDispatcher::instance()
{
  return _instance ? _instance : ((_instance = new AmEventDispatcher()));
}

bool AmEventDispatcher::addEventQueue(const string& local_tag, 
				      AmEventQueueInterface* q,
				      const string& callid, 
				      const string& remote_tag)
{
    bool exists = false;

    m_queues.lock();

    exists = queues.find(local_tag) != queues.end();

    if(!callid.empty() && !remote_tag.empty()) {
	exists = exists ||
	    (id_lookup.find(callid+remote_tag) != id_lookup.end());
    }

    if(!exists){
	queues[local_tag] = q;

	if(!callid.empty() && !remote_tag.empty())
	    id_lookup[callid+remote_tag] = local_tag;
    }

    m_queues.unlock();
    
    return exists;
}

AmEventQueueInterface* AmEventDispatcher::delEventQueue(const string& local_tag,
							const string& callid, 
							const string& remote_tag)
{
    AmEventQueueInterface* q = NULL;
    
    m_queues.lock();
    
    EvQueueMapIter qi = queues.find(local_tag);
    if(qi != queues.end()) {

	q = qi->second;
	queues.erase(qi);

	if(!callid.empty() && !remote_tag.empty()) {

	    DictIter di = id_lookup.find(callid+remote_tag);
	    if(di != id_lookup.end()) {
		
		id_lookup.erase(di);
	    }
	}
    }
    m_queues.unlock();
    
    return q;
}

bool AmEventDispatcher::post(const string& local_tag, AmEvent* ev)
{
    bool posted = false;
    m_queues.lock();

    EvQueueMapIter it = queues.find(local_tag);
    if(it != queues.end()){
	it->second->postEvent(ev);
	posted = true;
    }

    m_queues.unlock();
    
    return posted;
}

bool AmEventDispatcher::post(const string& callid, const string& remote_tag, AmEvent* ev)
{
    bool posted = false;
    m_queues.lock();

    DictIter di = id_lookup.find(callid+remote_tag);
    if(di != id_lookup.end()) {

	EvQueueMapIter it = queues.find(di->second);
	if(it != queues.end()){
	    it->second->postEvent(ev);
	    posted = true;
	}
    }
    m_queues.unlock();
    
    return posted;
}

bool AmEventDispatcher::postSipRequest(const string& callid, const string& remote_tag, 
				       const AmSipRequest& req)
{
    bool posted = false;
    m_queues.lock();

    DictIter di = id_lookup.find(callid+remote_tag);
    if(di != id_lookup.end()) {

	EvQueueMapIter it = queues.find(di->second);
	if(it != queues.end()){
	  it->second->postEvent(new AmSipRequestEvent(req));
	    posted = true;
	}
    }
    m_queues.unlock();
    
    return posted;
}
