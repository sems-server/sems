/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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

#include "AmSessionContainer.h"
#include "AmPlugIn.h"
#include "AmApi.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmEventDispatcher.h"

#include <assert.h>
#include <sys/types.h>
#include <unistd.h>

#include "sems.h"

// AmSessionContainer methods

AmSessionContainer* AmSessionContainer::_SessionContainer=0;

AmSessionContainer::AmSessionContainer()
    : _run_cond(false)
      
{
}

AmSessionContainer* AmSessionContainer::instance()
{
  if(!_SessionContainer)
    _SessionContainer = new AmSessionContainer();

  return _SessionContainer;
}

void AmSessionContainer::on_stop() 
{ 
}

void AmSessionContainer::run()
{

  while(1){

    _run_cond.wait_for();

    // Let some time for the Sessions 
    // to stop by themselves
    sleep(5);

    ds_mut.lock();
    DBG("Session cleaner starting its work\n");
	
    try {
      SessionQueue n_sessions;

      while(!d_sessions.empty()){

	AmSession* cur_session = d_sessions.front();
	d_sessions.pop();

	ds_mut.unlock();

	if(cur_session->is_stopped() && cur_session->detached.get()){
		    
	    DBG("session %p has been destroyed'\n",(void*)cur_session->_pid);
	    delete cur_session;
	}
	else {
	    DBG("session %p still running\n",(void*)cur_session->_pid);
	    n_sessions.push(cur_session);
	}

	ds_mut.lock();
      }

      swap(d_sessions,n_sessions);

    }catch(std::exception& e){
      ERROR("exception caught in session cleaner: %s\n", e.what());
      throw; /* throw again as this is fatal (because unlocking the mutex fails!! */
    }catch(...){
      ERROR("unknown exception caught in session cleaner!\n");
      throw; /* throw again as this is fatal (because unlocking the mutex fails!! */
    }

    bool more = !d_sessions.empty();
    ds_mut.unlock();

    DBG("Session cleaner finished\n");
    if(!more)
      _run_cond.set(false);
  }
}

void AmSessionContainer::stopAndQueue(AmSession* s)
{
  ds_mut.lock();

  if (AmConfig::LogSessions) {    
    INFO("session cleaner about to stop %s\n",
	 s->getLocalTag().c_str());
  }

  s->stop();
  d_sessions.push(s);
  _run_cond.set(true);
    
  ds_mut.unlock();
}

void AmSessionContainer::destroySession(AmSession* s)
{
  destroySession(s->getLocalTag());
}

void AmSessionContainer::destroySession(const string& local_tag)
{
    AmSession* s = NULL;
    AmEventQueueInterface* q = AmEventDispatcher::instance()->
      delEventQueue(local_tag);
    
    if(q &&
       (s = dynamic_cast<AmSession*>(q))) {
      
      stopAndQueue(s);
    }
    else {
      DBG("could not remove session: id not found or wrong type\n");
    }
}

AmSession* AmSessionContainer::startSessionUAC(AmSipRequest& req, AmArg* session_params) {
  AmSession* session = NULL;
  try {
    if((session = createSession(req, session_params)) != 0){
      session->dlg.updateStatusFromLocalRequest(req); // sets local tag as well
      session->setCallgroup(req.from_tag);

      session->setNegotiateOnReply(true);
      if (int err = session->sendInvite(req.hdrs)) {
	ERROR("INVITE could not be sent: error code = %d.\n", 
	      err);
	delete session;
	return NULL;
      }

      if (AmConfig::LogSessions) {      
	INFO("Starting UAC session %s app %s\n",
	     session->getLocalTag().c_str(), req.cmd.c_str());
      }

      session->start();

      addSession("","",req.from_tag,session);
    }
  } 
  catch(const AmSession::Exception& e){
    ERROR("%i %s\n",e.code,e.reason.c_str());
    AmSipDialog::reply_error(req,e.code,e.reason);
  }
  catch(const string& err){
    ERROR("startSession: %s\n",err.c_str());
    AmSipDialog::reply_error(req,500,err);
  }
  catch(...){
    ERROR("unexpected exception\n");
    AmSipDialog::reply_error(req,500,"unexpected exception");
  }

  return session;
}

void AmSessionContainer::startSessionUAS(AmSipRequest& req)
{
  try {
      // Call-ID and From-Tag are unknown: it's a new session
      AmSession* session;
      if((session = createSession(req)) != 0){

	// update session's local tag (ID) if not already set
	session->setLocalTag();
	const string& local_tag = session->getLocalTag();
	// by default each session is in its own callgroup
	session->setCallgroup(local_tag);

	if (AmConfig::LogSessions) {
	  INFO("Starting UAS session %s app %s\n",
	       session->getLocalTag().c_str(), req.cmd.c_str());
	}

	session->start();

	addSession(req.callid,req.from_tag,local_tag,session);
	session->postEvent(new AmSipRequestEvent(req));
      }
  } 
  catch(const AmSession::Exception& e){
    ERROR("%i %s\n",e.code,e.reason.c_str());
    AmSipDialog::reply_error(req,e.code,e.reason);
  }
  catch(const string& err){
    ERROR("startSession: %s\n",err.c_str());
    AmSipDialog::reply_error(req,500,err);
  }
  catch(...){
    ERROR("unexpected exception\n");
    AmSipDialog::reply_error(req,500,"unexpected exception");
  }
}


bool AmSessionContainer::postEvent(const string& callid, 
				   const string& remote_tag,
				   AmEvent* event)
{
    bool posted =
	AmEventDispatcher::instance()->
	post(callid,remote_tag,event);

    if(!posted)
	delete event;

    return posted;
}

bool AmSessionContainer::postEvent(const string& local_tag,
				   AmEvent* event) 
{
    bool posted =
	AmEventDispatcher::instance()->
	post(local_tag,event);

    if(!posted)
	delete event;

    return posted;

}

AmSession* AmSessionContainer::createSession(AmSipRequest& req, 
					     AmArg* session_params)
{
  if (AmConfig::SessionLimit &&
      AmConfig::SessionLimit <= AmSession::session_num) {
      
      DBG("session_limit %d reached. Not creating session.\n", 
	  AmConfig::SessionLimit);

      AmSipDialog::reply_error(req,AmConfig::SessionLimitErrCode, 
			       AmConfig::SessionLimitErrReason);
      return NULL;
  }

  AmSessionFactory* session_factory = 
      AmPlugIn::instance()->findSessionFactory(req);

  if(!session_factory) {

      ERROR("No session factory");
      AmSipDialog::reply_error(req,500,"No session factory");

      return NULL;
  }

  AmSession* session = NULL;
  if (req.method == "INVITE") {
    if (NULL != session_params) 
      session = session_factory->onInvite(req, *session_params);
    else 
      session = session_factory->onInvite(req);
  } else if (req.method == "REFER") {
    if (NULL != session_params) 
      session = session_factory->onRefer(req, *session_params);
    else 
      session = session_factory->onRefer(req);
  }

  if(!session) {
    //  Session creation failed:
    //   application denied session creation
    //   or there was an error.
    //
    //  let's hope the createState function has replied...
    //  ... and do nothing !

    DBG("onInvite/onRefer returned NULL\n");
  }

  return session;
}

bool AmSessionContainer::addSession(const string& callid,
				    const string& remote_tag,
				    const string& local_tag,
				    AmSession* session)
{
    return AmEventDispatcher::instance()->
	addEventQueue(local_tag,(AmEventQueue*)session,
		      callid,remote_tag);
}

bool AmSessionContainer::addSession(const string& local_tag,
				    AmSession* session)
{
    return AmEventDispatcher::instance()->
	addEventQueue(local_tag,(AmEventQueue*)session);
}
