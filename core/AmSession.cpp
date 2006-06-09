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
//#include "AmRequest.h"
#include "AmSdp.h"
#include "AmMail.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmApi.h"
#include "AmSessionContainer.h"
#include "AmSessionScheduler.h"
#include "AmDtmfDetector.h"
/* Session Timer: -ssa */
//#include "AmSessionTimer.h"

#include "log.h"

#include <algorithm>

#include <unistd.h>
#include <assert.h>
#include <sys/time.h>


// AmSessionEventHandler methods

bool AmSessionEventHandler::process(AmEvent*)
{
    return false;
}

bool AmSessionEventHandler::onSipEvent(AmSipEvent*)
{
    return false;
}

bool AmSessionEventHandler::onSipRequest(const AmSipRequest&)
{
    return false;
}

bool AmSessionEventHandler::onSipReply(const AmSipReply& reply)
{
    return false;
}

bool AmSessionEventHandler::onSendRequest(const string& method, 
					  const string& content_type,
					  const string& body,
					  string& hdrs)
{
    return false;
}

bool AmSessionEventHandler::onSendReply(const AmSipRequest& req,
					unsigned int  code,
					const string& reason,
					const string& content_type,
					const string& body,
					string& hdrs)
{
    return false;
}


// AmSession methods


#if __GNUC__ < 3
#define CALL_EVENT_H(method,args...) \
            do{\
                vector<AmSessionEventHandler*>::iterator evh = ev_handlers.begin(); \
                bool stop = false; \
                while((evh != ev_handlers.end()) && !stop){ \
                    stop = (*evh)->method( ##args ); \
                    evh++; \
		} \
		if(stop) \
                    return; \
            }while(0)
#else
#define CALL_EVENT_H(method,...) \
            do{\
                vector<AmSessionEventHandler*>::iterator evh = ev_handlers.begin(); \
                bool stop = false; \
                while((evh != ev_handlers.end()) && !stop){ \
                    stop = (*evh)->method( __VA_ARGS__ ); \
                    evh++; \
		} \
		if(stop) \
                    return; \
            }while(0)
#endif



AmSession::AmSession()
    : AmEventQueue(this), // AmDialogState(),
      dlg(this),
      detached(true),
      sess_stopped(false),rtp_str(this),first_negotiation(true),
      input(0), output(0), payload(0),
      m_dtmfDetector(this), m_dtmfEventQueue(&m_dtmfDetector),
      m_dtmfHandler(this), m_dtmfOutputQueue(&m_dtmfHandler),
      m_dtmfDetectionEnabled(true)
{
}

AmSession::~AmSession()
{
    for(vector<AmSessionEventHandler*>::iterator evh = ev_handlers.begin();
	evh != ev_handlers.end(); evh++)
	if((*evh)->destroy)
	    delete *evh;
}

void AmSession::setCallgroup(const string& cg) {
  callgroup = cg;
}

void AmSession::addHandler(AmSessionEventHandler* sess_evh)
{
    ev_handlers.push_back(sess_evh);
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

const string& AmSession::getCallID() const
{ 
    return dlg.callid;
}

const string& AmSession::getRemoteTag() const
{ 
    return dlg.remote_tag;
}

const string& AmSession::getLocalTag() const
{
    return dlg.local_tag;
}

void AmSession::setLocalTag(const string& tag)
{
    DBG("AmSession::setLocalTag(%s)\n",tag.c_str());
    dlg.local_tag = tag;
}

const SdpPayload* AmSession::getPayload()
{
    return &payload;
}

int AmSession::getRPort()
{
    return rtp_str.getRPort();
}

void AmSession::negotiate(const string& sdp_body,
			  bool force_symmetric_rtp,
			  string* sdp_reply)
{
    string r_host = "";
    int    r_port = 0;

    sdp.setBody(sdp_body.c_str());

    if(sdp.parse())
	throw AmSession::Exception(400,"session description parsing failed");

    if(sdp.media.empty())
	throw AmSession::Exception(400,"no media line found in SDP message");
    
    SdpPayload* tmp_pl = sdp.getCompatiblePayload(MT_AUDIO,r_host,r_port);

    if(!tmp_pl)
	throw AmSession::Exception(606,"could not find compatible payload");
    
    if(payload.int_pt == -1){

	payload = *tmp_pl;
	DBG("new payload: %i\n",payload.int_pt);
    }
    else if(payload.int_pt != tmp_pl->int_pt){
	DBG("old payload: %i; new payload: %i\n",payload.int_pt,tmp_pl->int_pt);
	throw AmSession::Exception(400,"do not accept payload changes");
    }

    const SdpPayload *telephone_event_payload = sdp.telephoneEventPayload();
    if(telephone_event_payload)
    {
	DBG("remote party supports telephone events (pt=%i)\n",
	    telephone_event_payload->payload_type);
	
	lockAudio();
	rtp_str.setTelephoneEventPT(telephone_event_payload);
	unlockAudio();
    }
    else {
	DBG("remote party doesn't support telephone events\n");
    }

    bool passive_mode = false;
    if( sdp.remote_active || force_symmetric_rtp) {
	DBG("The other UA is NATed: switched to passive mode.\n");
	DBG("remote_active = %i; force_symmetric_rtp = %i\n",
	    sdp.remote_active,force_symmetric_rtp);

	passive_mode = true;
    }

    if(sdp_reply)
	sdp.genResponse(AmConfig::LocalIP,rtp_str.getLocalPort(),*sdp_reply);

    lockAudio();
    rtp_str.setLocalIP(AmConfig::LocalIP);
    rtp_str.setPassiveMode(passive_mode);
    rtp_str.setRAddr(r_host, r_port);
    unlockAudio();
}

void AmSession::run()
{
    try {
	try {

	    onStart();

	    while (!sess_stopped.get() || 
		   (dlg.getStatus() == AmSipDialog::Disconnecting)//  ||
		   // (dlg.getUACTransPending())
		   ){
		
		waitForEvent();
		processEvents();

		DBG("%s dlg.getUACTransPending() = %i\n",
		    dlg.callid.c_str(),dlg.getUACTransPending());
	    }
	    
	    if ( dlg.getStatus() != AmSipDialog::Disconnected ) {
		
		DBG("dlg '%s' not terminated: sending bye\n",dlg.callid.c_str());
		if(dlg.bye() == 0){
		    while ( dlg.getStatus() != AmSipDialog::Disconnected ){
			waitForEvent();
			processEvents();
		    }
		}
		else {
		    WARN("failed to terminate call properly\n");
		}
	    }
	    // remove pending timers
	    //AmSessionTimer::instance()->removeTimers(getLocalTag());
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
    }
	
    destroy();
    
    // wait at least until session is out of RtpScheduler
    DBG("session is stopped, waiting for detach from SessionScheduler.\n");
    //detached.wait_for();
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
    static AmMutex _m;
    
    struct timeval t;
    gettimeofday(&t,NULL);

    string id = "";

    _m.lock();
    id += int2hex(rand()) + "-";
    _m.unlock();

    id += int2hex(t.tv_sec) + int2hex(t.tv_usec) + "-";
    id += int2hex((unsigned int) pthread_self());

    return id;
}

void AmSession::postDtmfEvent(AmDtmfEvent *evt)
{
    if (m_dtmfDetectionEnabled)
    {
        if (dynamic_cast<AmSipDtmfEvent *>(evt) ||
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
    DBG("AmSession::onDtmf(%i,%i)\n",event,duration_msec);
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
    // dialog_st->
    postEvent(new AmAudioEvent(AmAudioEvent::cleared));
}

void AmSession::process(AmEvent* ev)
{
    CALL_EVENT_H(process,ev);

    DBG("AmSession::process\n");

    AmSipEvent* sip_ev = dynamic_cast<AmSipEvent*>(ev);
    if(sip_ev){
	
	DBG("received SIP Event\n");
	onSipEvent(sip_ev);
	return;
    }

    AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
    if(audio_ev && (audio_ev->event_id == AmAudioEvent::cleared)){
	setStopped();
	return;
    }

    //AmDialogState::process(ev);
}


void AmSession::onSipEvent(AmSipEvent* sip_ev)
{
    CALL_EVENT_H(onSipEvent,sip_ev);

    AmSipRequestEvent* req_ev = dynamic_cast<AmSipRequestEvent*>(sip_ev);
    if(req_ev) {
	onSipRequest(req_ev->req);
	return;
    }
    

    AmSipReplyEvent* reply_ev = dynamic_cast<AmSipReplyEvent*>(sip_ev);
    if(reply_ev) {
	onSipReply(reply_ev->reply);
	return;
    }

    ERROR("Unknown SIP Event");
}

void AmSession::onSipRequest(const AmSipRequest& req)
{
    CALL_EVENT_H(onSipRequest,req);

    dlg.updateStatus(req);

    DBG("onSipRequest: method = %s\n",req.method.c_str());
    if(req.method == "INVITE"){
	
	onInvite(req);

	if(detached.get() && !getStopped()){
	
	    onSessionStart(req);
	    
	    if(input || output)
		AmSessionScheduler::instance()->addSession(this, callgroup);
	    else {
		ERROR("missing audio input and/or ouput.\n");
	    }
	}
    }
    else if( req.method == "BYE" ){
	
	dlg.reply(req,200,"OK");
	onBye(req);
    }
    else if( req.method == "CANCEL" ){

	dlg.reply(req,200,"OK");
	onCancel();

    } else if( req.method == "INFO" ){

      if ((strip_header_params(getHeader(req.hdrs, "Content-Type"))
	   =="application/dtmf-relay")|| 
	  (strip_header_params(getHeader(req.hdrs, "c"))
	   =="application/dtmf-relay")){
	postDtmfEvent(new AmSipDtmfEvent(req.body));
	dlg.reply(req, 200, "OK");
      }
    } 
}

void AmSession::onSipReply(const AmSipReply& reply)
{
    CALL_EVENT_H(onSipReply,reply);

    dlg.updateStatus(reply);
}

void AmSession::onInvite(const AmSipRequest& req)
{
    string sdp_reply;
    if(acceptAudio(req.body,req.hdrs,&sdp_reply)!=0)
	return;

    if(dlg.reply(req,200,"OK",
		 "application/sdp",sdp_reply) != 0){

	//throw AmSession::Exception(500,"error while sending response");
	setStopped();
    }
}

void AmSession::onBye(const AmSipRequest& req)
{
    setStopped();
}

int AmSession::acceptAudio(const string& body,
			   const string& hdrs,
			   string*       sdp_reply)
{
    try {
	try {
	    // handle codec and send reply
	    string str_msg_flags = getHeader(hdrs,"P-MsgFlags");
	    unsigned int msg_flags = 0;
	    if(reverse_hex2int(str_msg_flags,msg_flags)){
		ERROR("while parsing 'P-MsgFlags' header\n");
		msg_flags = 0;
	    }
	    
	    negotiate( body,
		       msg_flags & FL_FORCE_ACTIVE,
		       sdp_reply);
	    
	    // enable RTP stream
	    lockAudio();
	    rtp_str.init(&payload);
	    unlockAudio();
	    
	    DBG("Sending Rtp data to %s/%i\n",
		rtp_str.getRHost().c_str(),rtp_str.getRPort());

	    return 0;
	}
	catch(const AmSession::Exception& e){ throw e; }
	catch(const string& str){
	    ERROR("%s\n",str.c_str());
	    throw AmSession::Exception(500,str);
	}
	catch(...){
	    throw AmSession::Exception(500,"unexpected exception.");
	}
    }
    catch(const AmSession::Exception& e){
	ERROR("%i %s\n",e.code,e.reason.c_str());
// 	if(dlg.reply(req,e.code,e.reason, "")){
// 	    dlg.bye();
// 	}
	setStopped();
    }

    return -1;
}

// string AmSession::SessionTimerException::getErrorHeaders() const {
//   return "Min-SE:" + int2str(minSE) + "\n"
//     + "Supported: timer\n";
// }

void AmSession::onSendRequest(const string& method, const string& content_type,
			      const string& body, string& hdrs)
{
    CALL_EVENT_H(onSendRequest,method,content_type,body,hdrs);
}

void AmSession::onSendReply(const AmSipRequest& req, unsigned int  code, 
			    const string& reason, const string& content_type,
			    const string& body, string& hdrs)
{
    CALL_EVENT_H(onSendReply,req,code,reason,content_type,body,hdrs);
}

void AmSession::sendUpdate() 
{
  dlg.update(""// getRequestHeaders("UPDATE")
	     );
}

void AmSession::sendReinvite() 
{
  string sdp_body;
  sdp.genResponse(AmConfig::LocalIP,rtp_str.getLocalPort(),sdp_body);
  dlg.reinvite("", "application/sdp", sdp_body);
}

