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
#include "AmB2BSession.h"
#include "AmSessionContainer.h"
#include "AmConfig.h"

#include <assert.h>

//
// AmB2BSession methods
//

AmB2BSession::~AmB2BSession()
{
  DBG("relayed_req.size() = %u\n",(unsigned int)relayed_req.size());
  DBG("recvd_req.size() = %u\n",(unsigned int)recvd_req.size());
}

void AmB2BSession::clear_other()
{
#if __GNUC__ < 3
  string cleared ("");
  other_id.assign (cleared, 0, 0);
#else
  other_id.clear();
#endif
}

void AmB2BSession::process(AmEvent* event)
{
  B2BEvent* b2b_e = dynamic_cast<B2BEvent*>(event);
  if(b2b_e){

    onB2BEvent(b2b_e);
    return;
  }

  AmSession::process(event);
}

void AmB2BSession::onB2BEvent(B2BEvent* ev)
{
  switch(ev->event_id){

  case B2BSipRequest:
    {   
      B2BSipRequestEvent* req_ev = dynamic_cast<B2BSipRequestEvent*>(ev);
      assert(req_ev);

      if(req_ev->forward){

	relaySip(req_ev->req);
      }
      else if( (req_ev->req.method == "BYE") ||
	       (req_ev->req.method == "CANCEL") ) {
		
	onOtherBye(req_ev->req);
      }
    }
    return;

  case B2BSipReply:
    {
      B2BSipReplyEvent* reply_ev = dynamic_cast<B2BSipReplyEvent*>(ev);
      assert(reply_ev);

      DBG("B2BSipReply: %i %s (fwd=%i)\n",reply_ev->reply.code,
	  reply_ev->reply.reason.c_str(),reply_ev->forward);

      if(reply_ev->forward){

        std::map<int,AmSipRequest>::iterator t_req = recvd_req.find(reply_ev->reply.cseq);
	if (t_req != recvd_req.end()) {
	  relaySip(t_req->second,reply_ev->reply);
		
	  if(reply_ev->reply.code >= 200){

	    if( (t_req->second.method == "INVITE") &&
		(reply_ev->reply.code == 487)){
	      
	      terminateLeg();
	    }
	    recvd_req.erase(t_req);
	  } 
	} else {
	  ERROR("Request with CSeq %u not found in recvd_req.\n",
	      reply_ev->reply.cseq);
	}
      }
    }
    return;

  case B2BTerminateLeg:
    terminateLeg();
    break;
  }

  //ERROR("unknown event caught\n");
}

void AmB2BSession::onSipRequest(const AmSipRequest& req)
{
  bool fwd = sip_relay_only &&
    (req.method != "BYE") &&
    (req.method != "CANCEL");

  if(!fwd)
    AmSession::onSipRequest(req);
  else {
    dlg.updateStatus(req);
    recvd_req.insert(std::make_pair(req.cseq,req));
  }

  relayEvent(new B2BSipRequestEvent(req,fwd));
}

void AmB2BSession::onSipReply(const AmSipReply& reply)
{
  TransMap::iterator t = relayed_req.find(reply.cseq);
  bool fwd = t != relayed_req.end();

  DBG("onSipReply: %i %s (fwd=%i)\n",reply.code,reply.reason.c_str(),fwd);
  if(fwd) {
    AmSipReply n_reply = reply;
    n_reply.cseq = t->second.cseq;
    
    dlg.updateStatus(reply);
    relayEvent(new B2BSipReplyEvent(n_reply,true));

    if(reply.code >= 200)
      relayed_req.erase(t);
  }
  else {

    AmSession::onSipReply(reply);
    relayEvent(new B2BSipReplyEvent(reply,false));
  }
}

void AmB2BSession::relayEvent(AmEvent* ev)
{
  DBG("AmB2BSession::relayEvent: id=%s\n",
      other_id.c_str());

  if(!other_id.empty())
    AmSessionContainer::instance()->postEvent(other_id,ev);
  else 
    delete ev;
}

void AmB2BSession::onOtherBye(const AmSipRequest& req)
{
  DBG("onOtherBye()\n");
  terminateLeg();
}

bool AmB2BSession::onOtherReply(const AmSipReply& reply)
{
  if(reply.code >= 300)
    terminateLeg();
  return false;
}

void AmB2BSession::terminateLeg()
{
  setStopped();
  if ((dlg.getStatus() == AmSipDialog::Pending) 
      || (dlg.getStatus() == AmSipDialog::Connected))
    dlg.bye();
}

void AmB2BSession::terminateOtherLeg()
{
  relayEvent(new B2BEvent(B2BTerminateLeg));
  clear_other();
}

void AmB2BSession::relaySip(const AmSipRequest& req)
{
  relayed_req[dlg.cseq] = AmSipTransaction(req.method,req.cseq);
  dlg.sendRequest(req.method,"application/sdp",req.body,req.hdrs);
}

void AmB2BSession::relaySip(const AmSipRequest& orig, const AmSipReply& reply)
{
  string content_type = getHeader(reply.hdrs,"Content-Type");
  dlg.reply(orig,reply.code,reply.reason,content_type,reply.body);
}

// 
// AmB2BCallerSession methods
//

AmB2BCallerSession::AmB2BCallerSession()
  : AmB2BSession(),
    callee_status(None)
{
}

void AmB2BCallerSession::terminateLeg()
{
  AmB2BSession::terminateLeg();
}

void AmB2BCallerSession::terminateOtherLeg()
{
  AmB2BSession::terminateOtherLeg();
  callee_status = None;
}

void AmB2BCallerSession::onB2BEvent(B2BEvent* ev)
{
  bool processed = false;

  if(ev->event_id == B2BSipReply){

    AmSipReply& reply = ((B2BSipReplyEvent*)ev)->reply;

    if(other_id != reply.local_tag){
      DBG("Dialog missmatch!!\n");
      return;
    }

    DBG("reply received from other leg\n");

    switch(callee_status){
    case NoReply:
    case Ringing:

      if(reply.code < 200){

	callee_status = Ringing;
      }
      else if(reply.code < 300){

	callee_status  = Connected;

	if (!sip_relay_only) {
	  sip_relay_only = true;
	  reinviteCaller(reply);
	}
      }
      else {
// 	DBG("received %i from other leg: other_id=%s; reply.local_tag=%s\n",
// 	    reply.code,other_id.c_str(),reply.local_tag.c_str());
	
	terminateOtherLeg();
      }
		
      processed = onOtherReply(reply);
      break;

    default:
      DBG("reply from callee: %i %s\n",reply.code,reply.reason.c_str());
      break;
    }
  }
   
  if (!processed)
    AmB2BSession::onB2BEvent(ev);
}

void AmB2BCallerSession::relayEvent(AmEvent* ev)
{
  if(other_id.empty()){

    if(dynamic_cast<B2BEvent*>(ev)){

      B2BSipEvent*     sip_ev = dynamic_cast<B2BSipEvent*>(ev);
      B2BConnectEvent* co_ev  = dynamic_cast<B2BConnectEvent*>(ev);
	    
      if( (sip_ev && sip_ev->forward) || co_ev )
	createCalleeSession();
    }
  }

  AmB2BSession::relayEvent(ev);
}

void AmB2BCallerSession::onSessionStart(const AmSipRequest& req)
{
  invite_req = req;
  AmB2BSession::onSessionStart(req);
}

void AmB2BCallerSession::connectCallee(const string& remote_party,
				       const string& remote_uri,
				       bool relayed_invite)
{
  if(callee_status != None)
    terminateOtherLeg();

  B2BConnectEvent* ev = new B2BConnectEvent(remote_party,remote_uri);

  ev->content_type = "application/sdp"; // FIXME
  ev->body         = invite_req.body;
  ev->hdrs         = invite_req.hdrs;
  ev->relayed_invite = relayed_invite;
  ev->r_cseq       = invite_req.cseq;

  relayEvent(ev);
  callee_status = NoReply;
}

int AmB2BCallerSession::reinviteCaller(const AmSipReply& callee_reply)
{
  string content_type = getHeader(callee_reply.hdrs,"Content-Type");
  return dlg.sendRequest("INVITE",content_type,callee_reply.body);
}

void AmB2BCallerSession::createCalleeSession()
{
  AmB2BCalleeSession* callee_session = newCalleeSession();
  AmSipDialog& callee_dlg = callee_session->dlg;

  other_id = AmSession::getNewId();
    
  callee_dlg.local_tag    = other_id;
  callee_dlg.callid       = AmSession::getNewId() + "@" + AmConfig::LocalIP;

  callee_dlg.local_party  = dlg.remote_party;
  callee_dlg.remote_party = dlg.local_party;
  callee_dlg.remote_uri   = dlg.local_uri;

  callee_session->start();

  AmSessionContainer* sess_cont = AmSessionContainer::instance();
  sess_cont->addSession(other_id,callee_session);
}

AmB2BCalleeSession* AmB2BCallerSession::newCalleeSession()
{
  return new AmB2BCalleeSession(this);
}

void AmB2BCalleeSession::onB2BEvent(B2BEvent* ev)
{
  if(ev->event_id == B2BConnectLeg){

    B2BConnectEvent* co_ev = dynamic_cast<B2BConnectEvent*>(ev);

    dlg.remote_party = co_ev->remote_party;
    dlg.remote_uri   = co_ev->remote_uri;

    if (co_ev->relayed_invite) {
      relayed_req[dlg.cseq] = AmSipTransaction("INVITE", co_ev->r_cseq);
    }

    dlg.sendRequest("INVITE",co_ev->content_type,co_ev->body,co_ev->hdrs);

    return;
  }    

  AmB2BSession::onB2BEvent(ev);
}
