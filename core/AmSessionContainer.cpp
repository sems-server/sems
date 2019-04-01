/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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
  : _container_closed(false), _run_cond(false), enable_unclean_shutdown(false),
    max_cps(0), CPSLimit(0), CPSHardLimit(0)
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

      while (!_instance->is_stopped())
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
      
      if(cur_session->is_stopped() && !cur_session->isProcessingMedia()){
	
	MONITORING_MARK_FINISHED(cur_session->getLocalTag().c_str());

	DBG("session [%p] has been destroyed\n",(void*)cur_session->_pid);
	delete cur_session;
      }
      else {
	DBG("session [%p] still running\n",(void*)cur_session->_pid);
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

void AmSessionContainer::initMonitoring() {
  _MONITORING_INIT;
}

void AmSessionContainer::run()
{
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

void AmSessionContainer::broadcastShutdown() {
  DBG("brodcasting ServerShutdown system event to %u sessions...\n",
      AmSession::getSessionNum());
  AmEventDispatcher::instance()->
    broadcast(new AmSystemEvent(AmSystemEvent::ServerShutdown));
}

void AmSessionContainer::on_stop() 
{ 
  _container_closed.set(true);

  if (enable_unclean_shutdown) {
    INFO("unclean shutdown requested - not broadcasting shutdown\n");
  } else {
    broadcastShutdown();
    
    DBG("waiting for active event queues to stop...\n");

    for (unsigned int i=0;
	 (!AmEventDispatcher::instance()->empty() &&
	  (!AmConfig::MaxShutdownTime ||
	   i < AmConfig::MaxShutdownTime * 1000 / 10));i++)
      usleep(10000);

    if (!AmEventDispatcher::instance()->empty()) {
      WARN("Not all calls cleanly ended!\n");
    }
    
    DBG("cleaning sessions...\n");
    while (clean_sessions()) 
      usleep(10000);
  }

  _run_cond.set(true); // so that thread stops
}

void AmSessionContainer::stopAndQueue(AmSession* s)
{

  if (AmConfig::LogSessions) {    
    INFO("session cleaner about to stop %s\n",
	 s->getLocalTag().c_str());
  }

  s->stop();

  ds_mut.lock();
  d_sessions.push(s);
  _run_cond.set(true);    
  ds_mut.unlock();
}

void AmSessionContainer::destroySession(AmSession* s)
{
    AmEventQueueInterface* q = AmEventDispatcher::instance()->
      delEventQueue(s->getLocalTag());
    
    if(q) {
	stopAndQueue(s);
    }
    else {
	WARN("could not remove session: id not found or wrong type\n");
    }
}

string AmSessionContainer::startSessionUAC(const AmSipRequest& req, string& app_name, AmArg* session_params) {

  unique_ptr<AmSession> session;
  try {
    session.reset(createSession(req, app_name, session_params));
    if(session.get() != 0) {
      session->dlg->initFromLocalRequest(req);
      session->setCallgroup(req.from_tag);
      
      switch(addSession(req.from_tag,session.get())) {
	
      case AmSessionContainer::Inserted:
	// successful case
	break;
	
      case AmSessionContainer::ShutDown:
	throw AmSession::Exception(AmConfig::ShutdownModeErrCode,
				   AmConfig::ShutdownModeErrReason);
	
      case AmSessionContainer::AlreadyExist:
	throw AmSession::Exception(482,
				   SIP_REPLY_LOOP_DETECTED);
	
      default:
	ERROR("adding session to session container\n");
	throw string(SIP_REPLY_SERVER_INTERNAL_ERROR);
      }
      
      MONITORING_LOG5(req.from_tag, 
		      "app", app_name.c_str(),
		      "dir", "out",
		      "from", req.from.c_str(),
		      "to", req.to.c_str(),
		      "ruri", req.r_uri.c_str());
      
      if (int err = session->sendInvite(req.hdrs)) {
	ERROR("INVITE could not be sent: error code = %d.\n", err);
	AmEventDispatcher::instance()->delEventQueue(req.from_tag);
	MONITORING_MARK_FINISHED(req.from_tag.c_str());
	return "";
      }
      
      if (AmConfig::LogSessions) {      
	INFO("Starting UAC session %s app %s\n",
	     req.from_tag.c_str(), app_name.c_str());
      }
      try {
	session->start();
      } catch (...) {
	AmEventDispatcher::instance()->delEventQueue(req.from_tag);
	throw;
      }
    }
  } 
  catch(const AmSession::Exception& e){
    ERROR("%i %s\n",e.code,e.reason.c_str());
    return "";
  }
  catch(const string& err){
    ERROR("startSession: %s\n",err.c_str());
    return "";
  }
  catch(...){
    ERROR("unexpected exception\n");
    return "";
  }

  session.release();
  return req.from_tag;
}

void AmSessionContainer::startSessionUAS(AmSipRequest& req)
{
  try {
      // Call-ID and From-Tag are unknown: it's a new session
      unique_ptr<AmSession> session;
      string app_name;

      session.reset(createSession(req,app_name));
      if(session.get() != 0){

	// update session's local tag (ID) if not already set
	session->setLocalTag();
	const string& local_tag = session->getLocalTag();
	// by default each session is in its own callgroup
	session->setCallgroup(local_tag);

	if (AmConfig::LogSessions) {
	  INFO("Starting UAS session %s\n",
	       local_tag.c_str());
	}

	switch(addSession(req.callid,req.from_tag,local_tag,
			  req.via_branch,session.get())) {

	case AmSessionContainer::Inserted:
	  // successful case
	  break;
	  
	case AmSessionContainer::ShutDown:
	  throw AmSession::Exception(AmConfig::ShutdownModeErrCode,
				     AmConfig::ShutdownModeErrReason);
	  
	case AmSessionContainer::AlreadyExist:
	  throw AmSession::Exception(482,
				     SIP_REPLY_LOOP_DETECTED);
	  
	default:
	  ERROR("adding session to session container\n");
	  throw string(SIP_REPLY_SERVER_INTERNAL_ERROR);
	}

	MONITORING_LOG4(local_tag.c_str(), 
			"dir", "in",
			"from", req.from.c_str(),
			"to", req.to.c_str(),
			"ruri", req.r_uri.c_str());

	try {
	  session->start();
	} catch (...) {
	  AmEventDispatcher::instance()->
	    delEventQueue(local_tag);
	  throw;
	}

	session->postEvent(new AmSipRequestEvent(req));
	session.release();
      }
  } 
  catch(const AmSession::Exception& e){
    ERROR("%i %s %s\n",e.code,e.reason.c_str(), e.hdrs.c_str());
    AmSipDialog::reply_error(req,e.code,e.reason, e.hdrs);
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
				   const string& via_branch,
				   AmEvent* event)
{
    bool posted =
      AmEventDispatcher::instance()->
        post(callid,remote_tag,via_branch,event);

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

void AmSessionContainer::setCPSLimit(unsigned int limit)
{
  AmLock lock(cps_mut);
  CPSLimit = CPSHardLimit = limit;
}

void AmSessionContainer::setCPSSoftLimit(unsigned int percent)
{
  if(!percent) {
    CPSLimit = CPSHardLimit;
    return;
  }

  struct timeval tv, res;
  gettimeofday(&tv,0);

  AmLock lock(cps_mut);

  while (cps_queue.size()) {
    timersub(&tv, &cps_queue.front(), &res);
    if (res.tv_sec >= CPS_SAMPLERATE) {
      cps_queue.pop();
    }   
    else {
      break;
    }
  }
  CPSLimit = ((float)percent / 100) * ((float)cps_queue.size() / CPS_SAMPLERATE);
  if(0 == CPSLimit) CPSLimit = 1;
}

pair<unsigned int, unsigned int> AmSessionContainer::getCPSLimit()
{
  AmLock lock(cps_mut);
  return pair<unsigned int, unsigned int>(CPSHardLimit, CPSLimit);
}

unsigned int AmSessionContainer::getAvgCPS()
{
  struct timeval tv, res;
  gettimeofday(&tv,0);

  AmLock lock(cps_mut);

  while (cps_queue.size()) {
    timersub(&tv, &cps_queue.front(), &res);
    if (res.tv_sec >= CPS_SAMPLERATE) {
      cps_queue.pop();
    }   
    else {
      break;
    }
  }

  return (float)cps_queue.size() / CPS_SAMPLERATE;
}

unsigned int AmSessionContainer::getMaxCPS()
{
  AmLock lock(cps_mut);
  unsigned int res = max_cps;
  max_cps = 0;
  return res;
}

bool AmSessionContainer::check_and_add_cps()
{
  struct timeval tv, res;
  gettimeofday(&tv,0);

  AmLock lock(cps_mut);

  while (cps_queue.size()) {
    timersub(&tv, &cps_queue.front(), &res);
    if (res.tv_sec >= CPS_SAMPLERATE) {
      cps_queue.pop();
    }   
    else {
      break;
    }
  }

  unsigned int cps = (float)cps_queue.size() / CPS_SAMPLERATE;
  if (cps > max_cps) {
    max_cps = cps;
  }

  if( CPSLimit && cps > CPSLimit ){
    DBG("cps_limit %d reached. Not creating session.\n", CPSLimit);
    return true;
  }
  else {
    cps_queue.push(tv);
    return false;
  }
}

AmSession* AmSessionContainer::createSession(const AmSipRequest& req,
					     string& app_name,
					     AmArg* session_params)
{
  if (AmConfig::ShutdownMode) {
    _run_cond.set(true); // so that thread stops
    DBG("Shutdown mode. Not creating session.\n");

    AmSipDialog::reply_error(req,AmConfig::ShutdownModeErrCode,
			     AmConfig::ShutdownModeErrReason);
    return NULL;
  }

  if (AmConfig::SessionLimit &&
      AmConfig::SessionLimit <= AmSession::session_num) {
      
      DBG("session_limit %d reached. Not creating session.\n", 
	  AmConfig::SessionLimit);

      AmSipDialog::reply_error(req,AmConfig::SessionLimitErrCode, 
			       AmConfig::SessionLimitErrReason);
      return NULL;
  }

  if (check_and_add_cps()) {
      AmSipDialog::reply_error(req,AmConfig::CPSLimitErrCode, 
			       AmConfig::CPSLimitErrReason);
      return NULL;
  }

  AmSessionFactory* session_factory = NULL;
  if(!app_name.empty())
      session_factory = AmPlugIn::instance()->getFactory4App(app_name);
  else
      session_factory = AmPlugIn::instance()->findSessionFactory(req,app_name);

  if(!session_factory) {

      ERROR("No session factory for application\n");
      AmSipDialog::reply_error(req,500,SIP_REPLY_SERVER_INTERNAL_ERROR);

      return NULL;
  }

  map<string,string> app_params;
  parse_app_params(req.hdrs,app_params);

  AmSession* session = NULL;
  if (req.method == "INVITE") {
    if (NULL != session_params) {
      session = session_factory->onInvite(req, app_name, *session_params);
    }
    else {
      session = session_factory->onInvite(req, app_name, app_params);
    }
  } else if (req.method == "REFER") {
    if (NULL != session_params) 
      session = session_factory->onRefer(req, app_name, *session_params);
    else 
      session = session_factory->onRefer(req, app_name, app_params);
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
  else {
    // save session parameters
    session->app_params = app_params;
  }

  return session;
}

AmSessionContainer::AddSessionStatus 
AmSessionContainer::addSession(const string& callid,
			       const string& remote_tag,
			       const string& local_tag,
			       const string& via_branch,
			       AmSession* session)
{
  if(_container_closed.get())
    return ShutDown;
  
  if(AmEventDispatcher::instance()->
     addEventQueue(local_tag,(AmEventQueue*)session,
		   callid,remote_tag,via_branch)) {
    return Inserted;
  }

  return AlreadyExist;
}

AmSessionContainer::AddSessionStatus 
AmSessionContainer::addSession(const string& local_tag,
			       AmSession* session)
{
  if(_container_closed.get()) 
    return ShutDown;

  if(AmEventDispatcher::instance()->
     addEventQueue(local_tag,(AmEventQueue*)session)){
    return Inserted;
  }

  return AlreadyExist;
}

void AmSessionContainer::enableUncleanShutdown() {
  enable_unclean_shutdown = true;
}
