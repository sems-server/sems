/*
 * $Id$
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
/** @file AmEventQueue.h */
#ifndef _AMEVENTQUEUE_H_
#define _AMEVENTQUEUE_H_

#include "AmThread.h"
#include "AmEvent.h"

#include <queue>

/** 
 * \brief Asynchronous event queue implementation 
 * 
 * This class implements an event queue; a queue into which
 * \ref AmEvent can safely be posted at any time from any 
 * thread, which are then processed by the registered event
 *  handler.
 */
class AmEventQueue
{
protected:
  AmEventHandler*   handler;
  std::queue<AmEvent*>   ev_queue;
  AmMutex           m_queue;
  AmCondition<bool> ev_pending;

public:
  AmEventQueue(AmEventHandler*);
  ~AmEventQueue();

  void postEvent(AmEvent*);
  void processEvents();
  void waitForEvent();
  void processSingleEvent();
};

#endif

