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

AmSessionContainer* AmSessionContainer::_instance=NULL;

_MONITORING_DECLARE_INTERFACE(AmSessionContainer);

AmSessionContainer::AmSessionContainer()
  : _run_cond(false), _container_closed(false)
      
{
}

AmSessionContainer* AmSessionContainer::instance()
{
  if(!_instance)
    _instance = new AmSessionContainer();

  return _instance;
}

void AmSessionContainer::dispose() 
{
  if(_instance != NULL) {
    if(!_instance->is_stopped()) {
      _instance->stop();

      while(!_instance->is_stopped()) 
	usleep(10000);
    }
    // todo: add locking here
    delete _instance;
    _instance = NULL;
  }
}

bool AmSessionContainer::clean_sessions() {
  ds_mut.lock();
  DBG("Session cleaner starting its work\n");
  
  try {
    SessionQueue n_sessions;
    
    while(!d_sessions.empty()){
      
      AmSession* cur_session = d_sessions.front();
      d_sessions.pop();
      
      ds_mut.unlock();
      
      if(cur_session->is_stopped() && cur_session->detached.get()){
	
	MONITORING_MARK_FINISHED(cur_session->getCallID().c_str());

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
  return more;
}

void AmSessionContainer::run()
{
  _MONITORING_INIT;

  while(!_container_closed.get()){

    _run_cond.wait_for();

    if(_container_closed.get()) 
      break;

    // Give the Sessions some time to stop by themselves
    sleep(5);

    bool more = clean_sessions();

    DBG("Session cleaner finished\n");
    if(!more  && (!_container_closed.get()))
      _run_cond.set(false);
  }
  DBG("Session cleaner terminating\n");
}

void AmSessionContainer::on_stop() 
{ 
  _container_closed.set(true);

  DBG("brodcasting ServerShutdown system event to sessions...\n");
  AmEventDispatcher::instance()->
    broadcast(new AmSystemEvent(AmSystemEvent::ServerShutdown));
    
  DBG("waiting for active event queues to stop...\n");

  while (!AmEventDispatcher::instance()->empty())
    sleep(1);
    
  DBG("cleaning sessions...\n");
  while (clean_sessions()) 
    sleep(1);

  _run_cond.set(true); // so that thread stops
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
    AmEventQueueInterface* q = AmEventDispatcher::instance()->
	delEventQueue(s->getLocalTag(),
		      s->getCallID(),
		      s->getRemoteTag());
    
    if(q) {	
	stopAndQueue(s);
    }
    else {
	WARN("could not remove session: id not found or wrong type\n");
    }
}

AmSession* AmSessionContainer::startSessionUAC(AmSipRequest& req, AmArg* session_params) {
  AmSession* session = NULL;
  try {
    if((session = createSession(req, session_params)) != 0){
      session->dlg.updateStatusFromLocalRequest(req); // sets local tag as well
      session->setCallgroup(req.from_tag);

      session->setNegotiateOnReply(true);

      if (!addSession("","",req.from_tag,session)) {
	ERROR("adding session to session container\n");
	delete session;
	return NULL;
      }

      MONITORING_LOG5(session->getCallID().c_str(), 
		      "app", req.cmd.c_str(),
		      "dir", "out",
		      "from", req.from.c_str(),
		      "to", req.to.c_str(),
		      "ruri", req.r_uri.c_str());
      
      if (int err = session->sendInvite(req.hdrs)) {
	ERROR("INVITE could not be sent: error code = %d.\n", 
	      err);
	AmEventDispatcher::instance()->
	  delEventQueue(session->getLocalTag(),
			session->getCallID(),
			session->getRemoteTag());	
	delete session;
	MONITORING_MARK_FINISHED(session->getCallID().c_str());
	return NULL;
      }

      if (AmConfig::LogSessions) {      
	INFO("Starting UAC session %s app %s\n",
	     session->getLocalTag().c_str(), req.cmd.c_str());
      }

      session->start();

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
	       local_tag.c_str(), req.cmd.c_str());
	}

	if (!addSession(req.callid,req.from_tag,local_tag,session)) {
	  ERROR("adding session to session container\n");
	  delete session;
	  throw string("internal server error");
	}

	MONITORING_LOG5(req.callid.c_str(), 
			"app", req.cmd.c_str(),
			"dir", "in",
			"from", req.from.c_str(),
			"to", req.to.c_str(),
			"ruri", req.r_uri.c_str());

	session->start();

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

      ERROR("No session factory for application\n");
      AmSipDialog::reply_error(req,500,"Server Internal Error");

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
  if(_container_closed.get())
    return false;
  
  return AmEventDispatcher::instance()->
    addEventQueue(local_tag,(AmEventQueue*)session,
		  callid,remote_tag);
}

bool AmSessionContainer::addSession(const string& local_tag,
				    AmSession* session)
{
  if(_container_closed.get()) 
    return false;

  return AmEventDispatcher::instance()->
    addEventQueue(local_tag,(AmEventQueue*)session);
}
