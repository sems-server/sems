#include "AmSessionContainer.h"
#include "AmPlugIn.h"
#include "AmApi.h"
#include "AmConfig.h"
#include "AmUtils.h"
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

		if(cur_session->is_stopped() && cur_session->detached.get()){
		    
		    DBG("session %ld has been destroyed'\n",(unsigned long int)cur_session->_pid);
		    delete cur_session;
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
    
    DBG("session cleaner about to stop %s\n",
	s->getLocalTag().c_str());

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
    as_mut.lock();

    SessionMapIter sess_it = a_sessions.find(local_tag);
    if(sess_it != a_sessions.end()){
	
	AmSession* sess = sess_it->second;
	as_id_lookup.erase(sess->getCallID() + sess->getRemoteTag());
	a_sessions.erase(sess_it);
	
	stopAndQueue(sess);
	DBG("session stopped and queued for deletion (#sessions=%u)\n",
	    (unsigned int)a_sessions.size());
    }
    else {
	DBG("could not remove session: id not found\n");
    }

    as_mut.unlock();
}

AmSession* AmSessionContainer::getSession(const string& callid, const string& remote_tag)
{
    DictIter it = as_id_lookup.find(callid+remote_tag);
    if(it == as_id_lookup.end()){
	//ERROR("could not find session (callid='%s'; remote_tag='%s')\n",
	//      callid.c_str(),remote_tag.c_str());
	return NULL;
    }

    return getSession(it->second);
}

AmSession* AmSessionContainer::getSession(const string& local_tag)
{
    SessionMapIter it = a_sessions.find(local_tag);
    if(it == a_sessions.end()){
	//ERROR("could not find session (local_tag='%s')\n",local_tag.c_str());
	return NULL;
    }

    return it->second;
}

void AmSessionContainer::startSessionUAS(AmSipRequest& req)
{
    as_mut.lock();
    try {
	
	AmSession* session = getSession(req.callid,req.from_tag);
	if( session ){
	    
	    // it's a forked-and-merged INVITE
	    // reply 482 Loop detected
	    throw AmSession::Exception(482, "Loop detected");
	}
	else {
	    // Call-ID and From-Tag are unknown: it's a new session
	    AmSession* session;
 	    if((session = createSession(req)) != 0){

		string local_tag = AmSession::getNewId();
		session->setLocalTag(local_tag);
		session->setCallgroup(local_tag);
 		session->start();

		addSession_unsafe(req.callid,req.from_tag,local_tag,session);
		session->postEvent(new AmSipRequestEvent(req));
	    }
	    //else
	    //throw AmSession::Exception(500,"internal error");
	}
    } 
//     catch(const AmSession::SessionTimerException& e){
//  	ERROR("%i %s\n",e.code,e.reason.c_str());
// 	AmSipDialog::reply_error(req,e.code,e.reason,e.getErrorHeaders());
//     }
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
    as_mut.unlock();
}


bool AmSessionContainer::postEvent(const string& callid, 
				   const string& remote_tag,
				   AmEvent* event)
{
//     DBG("postEvent: callid = %s; remote_tag = %s\n",
// 	callid.c_str(),remote_tag.c_str());

    as_mut.lock();
    AmSession* s = getSession(callid,remote_tag);
    as_mut.unlock();
    
    if(!s){
	delete event;
	return false;
    }
    
    s->postEvent(event);
    return true;
}

bool AmSessionContainer::postEvent(const string& local_tag,
				   AmEvent* event) 
{
//     DBG("postEvent: local_tag = %s\n",local_tag.c_str());

    as_mut.lock();
    AmSession* s = getSession(local_tag);
    as_mut.unlock();
    
    if(!s){
	delete event;
	return false;
    }
    
//     DBG("posting...\n");
    s->postEvent(event);
    return true;
}

AmSession* AmSessionContainer::createSession(AmSipRequest& req)
{
    string& plugin_name = req.cmd;

    if(plugin_name.empty()){

	throw string("AmSessionContainer::createSession: req.cmd is empty!\n");
    } 
    else if(plugin_name == "sems"){

	plugin_name = getHeader(req.hdrs,"P-Iptel-App");
	if(plugin_name.empty())
	    throw string("AmSessionContainer::createSession: missing 'P-Iptel-App' header.\n");
    }

    AmSessionFactory* state_factory = AmPlugIn::instance()->getFactory4App(plugin_name);
    if(!state_factory) {

	ERROR("application '%s' not found !", plugin_name.c_str());
	throw string("application '" + plugin_name + "' not found !");
    }
	    
    AmSession* session = state_factory->onInvite(req);
    if(!session) {
	//  State creation failed:
	//   application denied session creation
	//   or there was an error.
	//
	//  let's hope the createState function has replied...
	//  ... and do nothing !

	DBG("onInvite returned NULL\n");
	return 0;
    }
    
    //state_factory->configureSession(session);
    //session->checkSessionExpires(req);

    return session;
}

bool AmSessionContainer::addSession_unsafe(const string& callid, 
					   const string& remote_tag,
					   const string& local_tag,
					   AmSession* session)
{
    if(getSession(callid,remote_tag))
       return false;

    as_id_lookup[callid+remote_tag] = local_tag;
    return addSession_unsafe(local_tag,session);
}

bool AmSessionContainer::addSession_unsafe(const string& local_tag,
					   AmSession* session)
{
    if(getSession(local_tag))
	return false;
    
    a_sessions[local_tag] = session;
    return true;
}

bool AmSessionContainer::addSession(const string& callid,
				    const string& remote_tag,
				    const string& local_tag,
				    AmSession* session)
{
    as_mut.lock();
    bool ret = addSession_unsafe(callid,remote_tag,local_tag,session);
    as_mut.unlock();
    return ret;
}

bool AmSessionContainer::addSession(const string& local_tag,
				    AmSession* session)
{
    as_mut.lock();
    bool ret = addSession_unsafe(local_tag,session);
    as_mut.unlock();
    return ret;
}

int AmSessionContainer::getSize()
{
    int res=0;
    as_mut.lock();
    res = a_sessions.size();
    as_mut.unlock();

    return res;
}
