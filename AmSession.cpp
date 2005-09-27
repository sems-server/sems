/*
 * $Id: AmSession.cpp,v 1.42.2.10 2005/09/02 13:47:46 rco Exp $
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

#include "AmServer.h"
#include "AmSession.h"
#include "AmRequest.h"
#include "AmSdp.h"
#include "AmMail.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmApi.h"
// #include "AmRtpScheduler.h"
#include "AmSessionScheduler.h"
#include "AmDtmfDetector.h"

#include "log.h"

#include <algorithm>

#include <unistd.h>
#include <assert.h>
AmSessionEvent::AmSessionEvent(EventType event_type, const AmRequestUAS& request)
    : AmEvent(int(event_type)), request(request)
{
}

AmNotifySessionEvent::AmNotifySessionEvent(AmRequestUAS& request)
    : AmSessionEvent(AmSessionEvent::Notify, request),
      body(request.getBody()),
      eventPackage(request.cmd.getHeader("Event")),
      subscriptionState(request.cmd.getHeader("Subscription-State")),
      contentType(request.cmd.getHeader("Content-Type")),
      contentLength(request.cmd.getHeader("Content-Length")),
      cseq(request.cmd.cseq)
{
}

// AmSession methods

AmSession::AmSession(AmRequest* _req, AmDialogState* _d_st)
    : req(_req),dialog_st(_d_st),detached(true),
      sess_stopped(false),rtp_str(this),first_negotiation(true),
      input(0), output(0),
      m_dtmfEventQueue(new AmDtmfDetector(this)),
      m_dtmfDetectionEnabled(true),
      m_dtmfOutputQueue(new AmDtmfHandler(this))
{
}

AmSession::~AmSession()
{
}

void AmSession::setInput(AmAudio* in)
{
    lockAudio();
    input = in;
    unlockAudio();
}

void AmSession::setOutput(AmAudio* out)
{
    lockAudio();
    output = out;
    unlockAudio();
}

void AmSession::setInOut(AmAudio* in,AmAudio* out)
{
    lockAudio();
    input = in;
    output = out;
    unlockAudio();
}

void AmSession::lockAudio()
{ 
    audio_mut.lock();
}

void AmSession::unlockAudio()
{
    audio_mut.unlock();
}

// void AmSession::onIdle()
// {
//     dialog_st->processEvents();
// }

const string& AmSession::getCallID() 
{ 
    return req->cmd.callid; 
}

const string& AmSession::getFromTag() 
{ 
    return req->cmd.from_tag; 
}

const string& AmSession::getToTag()
{
    return req->cmd.to_tag;
}

AmDialogState* AmSession::getDialogState()
{
  return dialog_st.get();
}

const SdpPayload* AmSession::getPayload()
{
    return payload;
}

int AmSession::getRPort()
{
    return rtp_str.getRPort();
}

void AmSession::negotiate(AmRequest* request)
{
    string r_host = "";
    int    r_port = 0;
    
    if(request->sdp.parse())
	throw AmSession::Exception(400,"session description parsing failed");
    
    if(request->sdp.media.empty())
	throw AmSession::Exception(400,"no media line found in SDP message");
    
    SdpPayload* tmp_pl = request->sdp.getCompatiblePayload(MT_AUDIO,r_host,r_port);

    if(!tmp_pl)
	throw AmSession::Exception(606,"could not find compatible payload");
    
    if(first_negotiation){
	payload = tmp_pl;
    }
    else if(payload->int_pt != tmp_pl->int_pt)
	throw AmSession::Exception(400,"do not accept payload changes");

    const SdpPayload *telephone_event_payload = request->sdp.telephoneEventPayload();
    if(telephone_event_payload)
    {
	DBG("remote party supports telephone events (pt=%i)\n",
	    telephone_event_payload->payload_type);
	rtp_str.setTelephoneEventPT(telephone_event_payload);
    }
    else {
	DBG("remote party doesn't support telephone events\n");
    }

    string str_msg_flags = request->cmd.getHeader("P-MsgFlags");
    unsigned int msg_flags;
    if(reverse_hex2int(str_msg_flags,msg_flags)){
	ERROR("while parsing 'P-MsgFlags' header\n");
	msg_flags = 0;
    }
    DBG("msg_flags=%u\n",msg_flags);
    
    bool passive_mode = false;
    if( request->sdp.remote_active
	|| (msg_flags & FL_FORCE_ACTIVE) ) {
	DBG("The other UA is NATed: switched to passive mode.\n");
	DBG("remote_active = %i; msg_flags = %i\n",
	    request->sdp.remote_active,msg_flags);
	passive_mode = true;
    }
    
    if(!request->getReplied()){
	
	//TODO: if dialog started
	if(first_negotiation)
	    dialog_st->onBeforeCallAccept(request);
	
	if (sess_stopped.get()) /* session still alive ? */
	    return;
	
	rtp_str.setLocalIP(request->cmd.dstip);
	request->accept(rtp_str.getLocalPort());
    }
    
    rtp_str.setPassiveMode(passive_mode);
    rtp_str.setRAddr(r_host, r_port);
    DBG("Sending Rtp data to %s/%i\n",r_host.c_str(),r_port);
    
    first_negotiation = false;
}

void AmSession::run()
{
    if(dialog_st.get()){

	try {
	    try {
		// handle INVITE negotiation
		negotiate(req.get());

		if(sess_stopped.get()){
		    destroy();
		    return;
		}

		// enable RTP stream
		rtp_str.init(payload);
		//rtp_str.trace(true,getCallID());
		dialog_st->onSessionStart(req.get());

		AmSessionScheduler::instance()->addSession(this);

 		while (!sess_stopped.get()){
		    DBG("dialog_st->waitForEvent()\n");
		    dialog_st->waitForEvent();
		    DBG("dialog_st->processEvents()\n");
 		    dialog_st->processEvents();
		}
	    }
	    catch(const AmSession::Exception& e){ throw e; }
	    catch(const string& str){
		ERROR("%s\n",str.c_str());
		throw AmSession::Exception(500,"unexpected exception.");
	    }
	    catch(...){
		throw AmSession::Exception(500,"unexpected exception.");
	    }
	}
	catch(const AmSession::Exception& e){
	    ERROR("%i %s\n",e.code,e.reason.c_str());
	    dialog_st->onError(e.code,e.reason);
	    req->error(e.code,e.reason);
	}
    }

    destroy();

    // wait at least until session is out of RtpScheduler
    DBG("session is stopped, waiting for detach from SessionScheduler.\n");
    detached.wait_for();
}

void AmSession::on_stop()
{
    //sess_stopped.set(true);
    DBG("AmSession::on_stop()\n");
    AmSessionScheduler::instance()->removeSession(this);
}

void AmSession::destroy()
{
    DBG("AmSession::destroy()\n");
    AmSessionContainer::instance()->destroySession(this);
}

string AmSession::getNewId()
{
    return int2hex(getpid()) + int2hex(rand());
}

void AmSession::postDtmfEvent(AmDtmfEvent *evt)
{
    if (m_dtmfDetectionEnabled)
    {
        if (dynamic_cast<AmSipDtmfEvent *>(evt) ||
//            dynamic_cast<AmInbandDtmfEvent *>(evt) ||
            dynamic_cast<AmRtpDtmfEvent *>(evt))
        {
            m_dtmfEventQueue.postEvent(evt);
        }
        else
        {
            m_dtmfOutputQueue.postEvent(evt);
        }
    }
}

void AmSession::processDtmfEvents()
{
    if (m_dtmfDetectionEnabled)
    {
        m_dtmfEventQueue.processEvents();
        m_dtmfOutputQueue.processEvents();
    }
}

void AmSession::putDtmfAudio(const unsigned char *buf, int size, int user_ts)
{
    m_dtmfEventQueue.putDtmfAudio(buf, size, user_ts);
}

void AmSession::onDtmf(int event, int duration_msec)
{
    if (dialog_st.get())
        dialog_st->onDtmf(event, duration_msec);
}

void AmSession::clearAudio()
{
    lockAudio();
    if(input){
	input->close();
	input = 0;
    }
    if(output){
	output->close();
	output = 0;
    }
    unlockAudio();
    DBG("Audio cleared !!!\n");
    dialog_st->postEvent(new AmAudioEvent(AmAudioEvent::cleared));
}

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
    int seed=0;
    FILE* fp_rand = fopen("/dev/random","r");
    if(fp_rand){
	fread(&seed,sizeof(int),1,fp_rand);
	fclose(fp_rand);
    }
    seed += getpid();
    seed += time(0);
    srand(seed);

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

		if(cur_session->getStopped()){
		    delete cur_session;
		    DBG("session %ld has been destroyed'\n",(unsigned long int)cur_session->_pid);
		}
		else {
		    DBG("session %ld still running\n",(unsigned long int)cur_session->_pid);
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

    DBG("session cleaner trying to stop %ld\n",(unsigned long int)s->_pid);
    s->stop();
    d_sessions.push(s);
    _run_cond.set(true);
    
    ds_mut.unlock();
}

void AmSessionContainer::destroySession(AmSession* s)
{
    string hash_str = string(s->getCallID()) + string(s->getFromTag());
    string to_tag = s->getToTag();
    sadSession(hash_str,to_tag);
}

bool AmSessionContainer::sadSession(const string& hash_str, string& sess_key)
{
    bool ret;
    as_mut.lock();
    try {
	DBG("searching for session to destroy (hash=%s,sess_key=%s)\n",hash_str.c_str(),sess_key.c_str());
	pair<SessionMapIter,SessionTableIter> sess_pair = findSession(hash_str,sess_key);
	ret = (sess_pair.first != a_sessions.end())
	    && (sess_pair.second != sess_pair.first->second.end());
	destroySession(sess_pair,true);
    }catch(...){
	ERROR("unexpected exception\n");
		}
    as_mut.unlock();
    return ret;
}

void AmSessionContainer::destroySession(pair<SessionMapIter,SessionTableIter>& sess_pair, bool sessionsAreLocked)
{
    if (!sessionsAreLocked) as_mut.lock();

    try {
	SessionMapIter sess_it = sess_pair.first;
	SessionTableIter it = sess_pair.second;
	
	if(sess_it != a_sessions.end()){
	    
	    DBG("sess_it != a_sessions.end()\n");
	    if(it != sess_it->second.end()){
		
		DBG("it != sess_it->second.end()\n");
		
		AmSession* am_sess = it->second;
		sess_it->second.erase(it);
		if(!sess_it->second.size())
		    a_sessions.erase(sess_it);
		
		DBG("session found: stopping session\n");
		stopAndQueue(am_sess);
		DBG("session stopped and destroyed (#sessions=%li)\n",a_sessions.size());
	    }
	    else {
		DBG("it == sess_it->second.end()\n");
	    }
	}
	else {
	    DBG("sess_it == sessions.end()\n");
	}
    }catch(...){
	ERROR("unexpected exception\n");
		}

    if (!sessionsAreLocked) as_mut.unlock();
}


AmSessionContainer::SessionSearchRes 
AmSessionContainer::findSession(const string& hash_str, string& sess_key)
{
    SessionTableIter it;
    SessionMapIter sess_it = a_sessions.find(hash_str);

    if(sess_it != a_sessions.end()){

	if(sess_key.length()){
	    
	    for(it=sess_it->second.begin();it!=sess_it->second.end();++it){
		
		if(it->first == sess_key){
		    DBG("session found with key\n");
		    break;
		}
	    }
	}
	else if(!sess_it->second.empty()){
	    DBG("sess_key is null: using the last session key\n");
	    it = sess_it->second.end()-1;
	    sess_key = it->first;
	}
	else
	    it = sess_it->second.end();
    }

    return make_pair(sess_it,it);
}

AmSession* AmSessionContainer::getSession(const string& hash_str, string& sess_key)
{
    pair<SessionMapIter,SessionTableIter> sess_pair = findSession(hash_str,sess_key);
	
    if( (sess_pair.first != a_sessions.end()) 
	&& (sess_pair.second != sess_pair.first->second.end()) ){
	    
	return sess_pair.second->second;
    }

    return NULL;
}

AmSession* AmSessionContainer::getSession(string& sess_key)
{
  // search through every session
  for (SessionMapIter map_it = a_sessions.begin(); 
       map_it != a_sessions.end(); map_it++) {
    SessionTableIter sess_it = map_it->second.begin(); 
    // search session within vector of sess_key,amsession*
    while (sess_it != map_it->second.end()) {
      if (sess_it->first == sess_key)
	break;
      sess_it++;
    }

    if (sess_it != map_it->second.end()) {
      DBG("session found with key '%s'\n", sess_key.c_str());
      return sess_it->second;
    }
  }
  return NULL;
}

void AmSessionContainer::startSession(const string& hash_str, string& sess_key,
				      AmRequestUAS* req)
{
    as_mut.lock();
    try {

	AmSession* session = AmSessionContainer::getSession(hash_str,sess_key);
	if( session ){
	    
	    DBG("AmSessionContainer: session already exists.\n");
	    // Session already exists...
	    if(!req->cmd.to_tag.empty()){
		// ...it's a RE-INVITE
		AmDialogState* state = session->getDialogState();
		assert(state);
		state->postEvent(new AmSessionEvent(AmSessionEvent::ReInvite,
						    *req));
	    }
	    else {
		req->cmd.to_tag = session->req->cmd.to_tag;
		// it's a forked-and-merged INVITE
		// reply 482 Loop detected
		throw AmSession::Exception(482, "Loop detected");
	    }
	}
	else {

	    // Call-ID and From-Tag are unknown: it's a new session

	    sess_key = AmSession::getNewId();
	    AmSession* session = createSession(req->cmd.cmd,req);

 	    if(session && !addSession(hash_str,sess_key,session)){
 		DBG( "Starting session... (hash=%s,sess_key=%s)\n",
 		     hash_str.c_str(),req->cmd.to_tag.c_str());
 		session->start();
 	    }
	    else
		throw AmSession::Exception(500,"internal error");
	}
    } 
    catch(const AmSession::Exception& e){
 	ERROR("%i %s\n",e.code,e.reason.c_str());
 	req->reply(e.code,e.reason);
    }
    catch(const string& err){
	ERROR("startSession: %s\n",err.c_str());
	req->reply(500,err);
    }
    catch(...){
	ERROR("unexpected exception\n");
		}
    as_mut.unlock();
}

// just a wrapper 
bool AmSessionContainer::postEvent(const string& hash_str,string& sess_key,
				   AmSessionEvent* event)
{
    return postGenericEvent(hash_str, sess_key, event);
}

// bool AmSessionContainer::postEvent(const string& hash_str,string& sess_key,
// 				   AmSessionEvent* event)
// {
//     bool ret = false;
//     as_mut.lock();
//     try {
// 	AmSession* session = AmSessionContainer::getSession(hash_str,sess_key);
// 	if( session ){
// 	    AmDialogState* state = session->getDialogState();
// 	    assert(state);
// 	    state->postEvent(event);
// 	    ret = true;
// 	}
//     } catch(...){
// 	ERROR("unexpected exception\n");
// 		}
//     as_mut.unlock();
//     return ret;
// }

bool AmSessionContainer::postGenericEvent(const string& hash_str,string& sess_key,
				   AmEvent* event) {
    bool ret = false;

    if (hash_str == DAEMON_TYPE_ID) {
      daemon_map_mutex.lock();
      DaemonMap::iterator it = daemon_map.find(sess_key);
      if (it != daemon_map.end()) {
	it->second->postEvent(event);
	ret = true;
      } else 
	ERROR("unable to find daemon with name '%s'\n", sess_key.c_str());

      daemon_map_mutex.unlock();
      return ret;
    }

    as_mut.lock();
    try {
	AmSession* session = AmSessionContainer::getSession(hash_str,sess_key);
	if( session ){
	    AmDialogState* state = session->getDialogState();
	    assert(state);
	    state->postEvent(event);
	    ret = true;
	}
    } catch(...){
	ERROR("unexpected exception\n");
		}
    as_mut.unlock();
    return ret;
}

bool AmSessionContainer::postGenericEvent(string& sess_key, AmEvent* event) {
  bool ret = false;
  as_mut.lock();
  try {
    AmSession* session = AmSessionContainer::getSession(sess_key);
    if( session ){
      AmDialogState* state = session->getDialogState();
      assert(state);
      state->postEvent(event);
      ret = true;
    }
  } catch(...){
    ERROR("unexpected exception\n");
  }
  as_mut.unlock();
  return ret;
}

void AmSessionContainer::registerDaemon(const string& daemon_name, AmEventQueue* event_queue) {
  if (!event_queue) {
    ERROR("trying to register non existing event queue with Session Container");
  }
  DBG("registering daemon with name '%s' as event receiver.\n", daemon_name.c_str());
  daemon_map_mutex.lock();
  daemon_map[daemon_name] = event_queue;
  daemon_map_mutex.unlock();
}

AmSession* AmSessionContainer::createSession(const string& app_name, AmRequest* req)
{
    AmStateFactory* state_factory = AmPlugIn::instance()->getFactory4App(app_name);
    if(!state_factory) {
	ERROR("application '%s' not found !", app_name.c_str());
	throw string("application '" + app_name + "' not found !");
		}
	    
    AmDialogState* st = state_factory->onInvite(req->cmd);
    if(!st) {
	//  State creation failed:
	//   application denied session creation
	//   or there was an error.
	//
	//  let's hope the createState function has replied...
	//  ... and do nothing !

	DBG("onInvite returned NULL\n");
	return 0;
    }

    // create a new session
    st->state_factory = state_factory;
    AmSession* session = new AmSession(req->duplicate(),st);
    st->session = session;
    return session;
}

bool AmSessionContainer::addSession(const string& hash_str,const string& sess_key,
				    AmSession* session)
{
    SessionMapIter sess_it = a_sessions.find(hash_str);
    bool ret = sess_it != a_sessions.end();
    a_sessions[hash_str].push_back(make_pair(sess_key,session));
    return ret;
}
