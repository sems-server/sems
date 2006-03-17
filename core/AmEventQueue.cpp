/*
 * $Id: AmEventQueue.cpp,v 1.4 2004/09/28 10:56:26 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmEventQueue.h"
#include "log.h"

AmEventQueue::AmEventQueue(AmEventHandler* handler)
    : handler(handler),ev_pending(false)
{
}

AmEventQueue::~AmEventQueue()
{
  m_queue.lock();
  while(!ev_queue.empty()){
	delete ev_queue.front();
	ev_queue.pop();
  }
  m_queue.unlock();
}

void AmEventQueue::postEvent(AmEvent* event)
{
    DBG("AmEventQueue: trying to post event\n");

    m_queue.lock();
    if(event)
	ev_queue.push(event);
    ev_pending.set(true);
    m_queue.unlock();

    DBG("AmEventQueue: event posted\n");
}

void AmEventQueue::processEvents()
{
    m_queue.lock();

    while(!ev_queue.empty()) {
	
	AmEvent* event = ev_queue.front();
	ev_queue.pop();
	m_queue.unlock();

	DBG("before processing event\n");
	handler->process(event);
	DBG("event processed\n");
	delete event;
	m_queue.lock();
    }
    
    ev_pending.set(false);
    m_queue.unlock();
}

void AmEventQueue::waitForEvent()
{
    ev_pending.wait_for();
}

void AmEventQueue::processSingleEvent()
{
    m_queue.lock();

    if (!ev_queue.empty()) {

	AmEvent* event = ev_queue.front();
	ev_queue.pop();
	m_queue.unlock();

	DBG("before processing event\n");
	handler->process(event);
	DBG("event processed\n");
	delete event;

	m_queue.lock();
	if (ev_queue.empty())
	    ev_pending.set(false);
    }

    m_queue.unlock();
}

