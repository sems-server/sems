/*
 * $Id: IvrEvents.h,v 1.7.2.1 2005/09/02 13:47:46 rco Exp $
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

#ifndef _IVR_EVENTS_H_
#define _IVR_EVENTS_H_

#include "AmThread.h"
#include "AmSession.h"

#include <string>

/*
 * "interface" EventProducer
 * events generated will be fed into the registered event queue
 */
class IvrEventProducer {
 public: 
  IvrEventProducer();
  virtual ~IvrEventProducer() { };
  virtual int registerForeignEventQueue(AmEventQueue* destinationEventQueue)=0;
  virtual void unregisterForeignEventQueue()=0;
};


struct IvrScriptEvent: public AmEvent {
  int DTMFKey;
  AmNotifySessionEvent* event;
  AmRequest* req;

  IvrScriptEvent(int event_id, int dtmf_Key);
  IvrScriptEvent(int event_id, AmRequest* req);
  IvrScriptEvent(int event_id, AmNotifySessionEvent* event);
  IvrScriptEvent(int event_id);
  
  enum Action { 
    IVR_Bye,
    IVR_Notify,
    IVR_DTMF,
    IVR_MediaQueueEmpty,
    IVR_ReferStatus,
    IVR_ReferResponse
  };
};

struct IvrScriptEventReferResponse : public IvrScriptEvent {
    std::string method;
    int cseq;
    int code;
    std::string reason;
    IvrScriptEventReferResponse(std::string method_, int cseq_, 
				int code_, std::string reason_)
	: IvrScriptEvent(IvrScriptEvent::IVR_ReferResponse), method(method_), 
          cseq(cseq_), code(code_), reason(reason_) { }
};


struct IvrMediaEvent: public AmEvent {
    std::string MediaFile; 
    bool front;
    bool loop;
    
    int usleep_time;
  
    IvrMediaEvent(int event_id, std::string MediaFile = "" , bool front = true, bool loop = false); 
    
    IvrMediaEvent(int event_id, int sleep_time);
  enum Action { 
    IVR_enqueueMediaFile,
    IVR_emptyMediaQueue,
    IVR_startRecording, 
    IVR_stopRecording, 
    IVR_enableDTMFDetection, 
    IVR_disableDTMFDetection,
    IVR_pauseDTMFDetection,
    IVR_resumeDTMFDetection,
    IVR_mthr_usleep 
  };
};

struct IvrDialogControlEvent : public AmEvent {
  bool bool_value;
  
  IvrDialogControlEvent(int event_id, bool send_bye) : 
    AmEvent(event_id), bool_value(send_bye) { }
  
  enum Action {
    ChangeSendByePolicy
  };
};

/*
 * class IvrScriptEventProcessor 
 * Another thread to process events in the script interpreter. 
 */
#ifdef IVR_PERL
#define SCRIPT_EVENT_CHECK_INTERVAL_US 500 
class IvrScriptEventProcessor : public AmThread {
  AmSharedVar<bool> runcond;
  AmEventQueue* q;
 public:
  IvrScriptEventProcessor(AmEventQueue* watchThisQueue);
  ~IvrScriptEventProcessor();
  void run();
  void on_stop();
};
#endif // IVR_PERL



#endif
