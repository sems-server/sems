/*
 * Copyright (C) 2006 iptego GmbH
 * Copyright (C) 2011 Stefan Sayer
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

#include "AmSipRegistration.h"
#include "AmSession.h"
#include "AmSessionContainer.h"
AmSIPRegistration::AmSIPRegistration(const string& handle,
				     const SIPRegistrationInfo& info,
				     const string& sess_link) 
  : info(info),
    dlg(this),
    cred(info.domain, info.auth_user, info.pwd),
    active(false),
    reg_begin(0),
    reg_expires(0),
    remove(false),
    sess_link(sess_link),
    reg_send_begin(0),
    waiting_result(false),
    seh(NULL),
    expires_interval(3600)
{
  req.cmd      = "sems";
  req.user     = info.user;
  req.method   = "REGISTER";
  req.r_uri    = "sip:"+info.domain;
  req.from     = info.name+" <sip:"+info.user+"@"+info.domain+">";
  req.from_uri = "sip:"+info.user+"@"+info.domain;
  req.from_tag = handle;
  req.to       = req.from;
  req.to_tag   = "";
  req.callid   = AmSession::getNewId(); 
  //

  // clear dlg.callid? ->reregister?
  dlg.updateStatusFromLocalRequest(req);
  dlg.cseq = 50;
  if(!info.contact.empty()) {
    dlg.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) "<sip:";
    dlg.contact_uri += info.contact + ">" + CRLF;
  }
}

AmSIPRegistration::~AmSIPRegistration() {
  setSessionEventHandler(NULL);
}

void AmSIPRegistration::setSessionEventHandler(AmSessionEventHandler* new_seh) {
  if (seh)
    delete seh;
  seh = new_seh;
}
 
void AmSIPRegistration::setExpiresInterval(unsigned int desired_expires) {
  expires_interval = desired_expires;
}

bool AmSIPRegistration::doRegistration() 
{
  bool res = true;

  waiting_result = true;
  req.to_tag     = "";
  dlg.remote_tag = "";
  req.r_uri    = "sip:"+info.domain;
  dlg.remote_uri = req.r_uri;
    
  // set outbound proxy as next hop 
  if (!info.proxy.empty()) {
    dlg.outbound_proxy = info.proxy;
  } else if (!AmConfig::OutboundProxy.empty()) {
    dlg.outbound_proxy = AmConfig::OutboundProxy;
  }

  if(!info.contact.empty()) {
    dlg.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) "<"
      + info.contact + ">" + CRLF;
  }
    
  if (dlg.sendRequest(req.method, "", "",
		      SIP_HDR_COLSP(SIP_HDR_EXPIRES)+
		      int2str(expires_interval)+CRLF) < 0) {
    ERROR("failed to send registration.\n");
    res = false;
    waiting_result = false;
  }
    
  // save TS
  reg_send_begin  = time(NULL);
  return res;
}

bool AmSIPRegistration::doUnregister()
{
  bool res = true;

  waiting_result = true;
  req.to_tag     = "";
  dlg.remote_tag = "";
  req.r_uri      = "sip:"+info.domain;
  dlg.remote_uri = req.r_uri;
    
  // set outbound proxy as next hop 
  if (!info.proxy.empty()) {
    dlg.outbound_proxy = info.proxy;
  } else if (!AmConfig::OutboundProxy.empty()) 
    dlg.outbound_proxy = AmConfig::OutboundProxy;
  //else 
  //    dlg.outbound_proxy = "";
  if(!info.contact.empty()) {
    dlg.contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) "<";
    dlg.contact_uri += info.contact + ">" + CRLF;
  }
    
  if (dlg.sendRequest(req.method, "", "",
		      SIP_HDR_COLSP(SIP_HDR_EXPIRES) "0" CRLF) < 0) {
    ERROR("failed to send deregistration.\n");
    res = false;
    waiting_result = false;
  }

  // save TS
  reg_send_begin  = time(NULL);
  return res;
}

void AmSIPRegistration::onSendRequest(const string& method,
				      const string& content_type,
				      const string& body,
				      string& hdrs,
				      int flags,
				      unsigned int cseq) {
  if (seh)
    seh->onSendRequest(method, content_type, body,
		       hdrs,flags,cseq);
}
	
void AmSIPRegistration::onSendReply(const AmSipRequest& req,
				    unsigned int  code,
				    const string& reason,
				    const string& content_type,
				    const string& body,
				    string& hdrs,
				    int flags) {
  if (seh)
    seh->onSendReply(req,code,reason,
		     content_type,body,hdrs,flags);
}

AmSIPRegistration::RegistrationState AmSIPRegistration::getState() {
  if (active) 
    return RegisterActive;
  if (waiting_result)
    return RegisterPending;
	
  return RegisterExpired;
}

unsigned int AmSIPRegistration::getExpiresLeft() {
  long diff = reg_begin + reg_expires  - time(NULL);
  if (diff < 0) 
    return 0;
  else 
    return diff;
}

time_t AmSIPRegistration::getExpiresTS() {
  return reg_begin + reg_expires;
}
	
void AmSIPRegistration::onRegisterExpired() {
  if (sess_link.length()) {
    AmSessionContainer::instance()->postEvent(sess_link,
					      new SIPRegistrationEvent(SIPRegistrationEvent::RegisterTimeout,
								       req.from_tag));
  }
  DBG("Registration '%s' expired.\n", (info.user+"@"+info.domain).c_str());
  active = false;
  remove = true;
}

void AmSIPRegistration::onRegisterSendTimeout() {
  if (sess_link.length()) {
    AmSessionContainer::instance()->
      postEvent(sess_link,
		new SIPRegistrationEvent(SIPRegistrationEvent::RegisterSendTimeout,
					 req.from_tag));
  }
  DBG("Registration '%s' REGISTER request timeout.\n", 
      (info.user+"@"+info.domain).c_str());
  active = false;
  remove = true;
}

bool AmSIPRegistration::registerSendTimeout(time_t now_sec) {
  return now_sec > reg_send_begin + REGISTER_SEND_TIMEOUT;
}

bool AmSIPRegistration::timeToReregister(time_t now_sec) {
  //   	if (active) 
  //   		DBG("compare %lu with %lu\n",(reg_begin+reg_expires), (unsigned long)now_sec);
  return (((unsigned long)reg_begin+ reg_expires/2) < (unsigned long)now_sec);	
}

bool AmSIPRegistration::registerExpired(time_t now_sec) {
  return ((reg_begin+reg_expires) < (unsigned int)now_sec);	
}

void AmSIPRegistration::onSipReply(const AmSipReply& reply, int old_dlg_status, const string& trans_method)
{
  if ((seh!=NULL) && seh->onSipReply(reply, old_dlg_status, trans_method))
    return;

  waiting_result = false;

  if ((reply.code>=200)&&(reply.code<300)) {
    DBG("positive reply to REGISTER!\n");
    size_t end  = 0;
    string local_contact_hdr = dlg.getContactHdr();
    local_contact.parse_contact(local_contact_hdr, (size_t)0, end);
    local_contact.dump();

    string contacts = reply.contact;
    if (contacts.empty()) 
      contacts = getHeader(reply.hdrs, "Contact", "m", true);
    bool found = false;

    if (!contacts.length()) {
      DBG("received positive reply to de-Register \n");
      active = false;
      remove = true;
    } else {
      end = 0;
      while (!found) {
	if (contacts.length() == end)
	  break;

	if (!server_contact.parse_contact(contacts, end, end)) {
	  ERROR("while parsing contact\n");
	  break;
	}
	server_contact.dump();
				
	if (server_contact.isEqual(local_contact)) {
	  DBG("contact found\n");
	  found = active = true;
	  //bool str2i(const string& str, unsigned int& result);
					
	  if (str2i(server_contact.params["expires"], reg_expires)) {
	    ERROR("could not extract expires value.\n");
	    reg_expires = 500;
	  }
	  DBG("got an expires of %d\n", reg_expires);
	  // save TS
	  reg_begin = time(0);

	  if (sess_link.length()) {
	    DBG("posting SIPRegistrationEvent to '%s'\n", sess_link.c_str());
	    AmSessionContainer::instance()->
	      postEvent(sess_link,
			new SIPRegistrationEvent(SIPRegistrationEvent::RegisterSuccess,
						 req.from_tag,
						 reply.code, reply.reason));
	  }
	  break;
	}
      }
    }
    if (!found) {
      if (sess_link.length()) {
	AmSessionContainer::instance()->
	  postEvent(sess_link,
		    new SIPRegistrationEvent(SIPRegistrationEvent::RegisterNoContact,
					     req.from_tag,
					     reply.code, reply.reason));
      }
      DBG("no matching Contact - deregistered.\n");
      active = false;
      remove = true;
    }
		
  } else if (reply.code >= 300) {
    DBG("Registration failed.\n");
    if (sess_link.length()) {
      AmSessionContainer::instance()->
	postEvent(sess_link,
		  new SIPRegistrationEvent(SIPRegistrationEvent::RegisterFailed,
					   req.from_tag,
					   reply.code, reply.reason));
    }
    active = false;
    remove = true;		
  }
}

