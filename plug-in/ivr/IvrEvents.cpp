/*
 * $Id: IvrEvents.cpp,v 1.7.2.1 2005/09/02 13:47:46 rco Exp $
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "IvrEvents.h"
#include "log.h"

#include <unistd.h>

#include <string>

IvrEventProducer::IvrEventProducer() {
}

IvrMediaEvent::IvrMediaEvent(int event_id, std::string MediaFile, bool front, bool loop) 
    :    AmEvent(event_id), MediaFile(MediaFile), front(front), loop(loop)
{
    DBG("New Media Event: %d, %s\n", event_id, MediaFile.c_str());
}

IvrMediaEvent::IvrMediaEvent(int event_id, int sleep_time)
    :    AmEvent(event_id), usleep_time(sleep_time)
{
    DBG("New Media Event: %d, %d\n", event_id, sleep_time);
}

// the events we get
IvrScriptEvent::IvrScriptEvent(int event_id, int dtmf_Key)
  : AmEvent(event_id), DTMFKey(dtmf_Key)
{
  //  assert(event_id == IVR_DTMF);
}
IvrScriptEvent::IvrScriptEvent(int event_id, AmRequest* req)
  : AmEvent(event_id), req(req)
{
  // assert(event_id == IVR_Bye);
}

IvrScriptEvent::IvrScriptEvent(int event_id, AmNotifySessionEvent* event)
  : AmEvent(event_id), event(event)
{
  // assert(event_id == IVR_Notify);
}

IvrScriptEvent::IvrScriptEvent(int event_id)
  : AmEvent(event_id)
{
  // assert(event_id == IVR_MediaQueueEmpty);
}

#ifdef IVR_PERL
IvrScriptEventProcessor::IvrScriptEventProcessor(AmEventQueue* watchThisQueue) 
  : runcond(true), q(watchThisQueue)
{
}

void IvrScriptEventProcessor::on_stop() {
  runcond.set(false);
}
void IvrScriptEventProcessor::run() {
  usleep(500);
  while (runcond.get()) {
    q->processEvents();
    usleep(SCRIPT_EVENT_CHECK_INTERVAL_US);
  }
  DBG("IvrScriptEventProcessor exiting.\n");
}

IvrScriptEventProcessor::~IvrScriptEventProcessor() {
  DBG("IvrScriptEventProcessor destroyed.\n");
}
#endif //IVR_PERL
