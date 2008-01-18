/*
 * $Id: AmApi.h,v 1.9.2.1 2005/06/01 12:00:24 rco Exp $
 *
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

#include "SIPRegistrarClient.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"

#define MOD_NAME "registrar_client"

#define REGISTER_SEND_TIMEOUT 60 

#include <unistd.h>

EXPORT_SIP_EVENT_HANDLER_FACTORY(SIPRegistrarClient, MOD_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(SIPRegistrarClient, MOD_NAME);


SIPRegistration::SIPRegistration(const string& handle,
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
    seh(NULL)
{
  req.cmd      = "sems";
  req.user     = info.user;
  req.method   = "REGISTER";
  req.dstip    = AmConfig::LocalIP;
  req.r_uri    = "sip:"+info.domain;
  req.from     = info.name+" <sip:"+info.user+"@"+info.domain+">";
  req.from_uri = "sip:"+info.user+"@"+info.domain;
  req.from_tag = handle;
  req.to       = req.from;
  req.to_tag   = "";
  req.callid   = AmSession::getNewId() + "@" + AmConfig::LocalIP; 
  //

  // clear dlg.callid? ->reregister?
  dlg.updateStatusFromLocalRequest(req);
	
  dlg.sip_ip   = AmConfig::LocalSIPIP;
  if (AmConfig::LocalSIPPort)
    dlg.sip_port = int2str(AmConfig::LocalSIPPort);
  dlg.cseq = 50;
}

SIPRegistration::~SIPRegistration() {
  setSessionEventHandler(NULL);
}

void SIPRegistration::setSessionEventHandler(AmSessionEventHandler* new_seh) {
  if (seh)
    delete seh;
  seh = new_seh;
}
 
void SIPRegistration::doRegistration() {
  waiting_result = true;
  req.to_tag     = "";
  dlg.remote_tag = "";
  req.r_uri    = "sip:"+info.domain;
  dlg.remote_uri = req.r_uri;
  // set outbound proxy as next hop 
  if (!AmConfig::OutboundProxy.empty()) 
    dlg.next_hop = AmConfig::OutboundProxy;
  else 
    dlg.next_hop = "";
  
  dlg.sendRequest(req.method, "", "", "Expires: 1000\n");

  // save TS
  struct timeval now;
  gettimeofday(&now, NULL);
  reg_send_begin  = now.tv_sec;
}

void SIPRegistration::doUnregister() {
  waiting_result = true;
  req.to_tag     = "";
  dlg.remote_tag = "";
  req.r_uri    = "sip:"+info.domain;
  dlg.remote_uri = req.r_uri;

  // set outbound proxy as next hop 
  if (!AmConfig::OutboundProxy.empty()) 
    dlg.next_hop = AmConfig::OutboundProxy;
  else 
    dlg.next_hop = "";

  dlg.sendRequest(req.method, "", "", "Expires: 0\n");

  // save TS
  struct timeval now;
  gettimeofday(&now, NULL);
  reg_send_begin  = now.tv_sec;
}

void SIPRegistration::onSendRequest(const string& method,
				    const string& content_type,
				    const string& body,
				    string& hdrs,
				    unsigned int cseq) {
  if (seh)
    seh->onSendRequest(method, content_type, body,
		       hdrs,cseq);
}
	
void SIPRegistration::onSendReply(const AmSipRequest& req,
				  unsigned int  code,
				  const string& reason,
				  const string& content_type,
				  const string& body,
				  string& hdrs) {
  if (seh)
    seh->onSendReply(req,code,reason,
		     content_type,body,hdrs);
}

SIPRegistration::RegistrationState SIPRegistration::getState() {
  if (active) 
    return RegisterActive;
  if (waiting_result)
    return RegisterPending;
	
  return RegisterExpired;
}

unsigned int SIPRegistration::getExpiresLeft() {
  struct timeval now;
  gettimeofday(&now, NULL);
	
  int diff = reg_begin + reg_expires  - now.tv_sec;
  if (diff < 0) 
    return 0;
  else 
    return diff;
}

//-----------------------------------------------------------
SIPRegistrarClient* SIPRegistrarClient::_instance=0;

SIPRegistrarClient* SIPRegistrarClient::instance()
{
  if(_instance == NULL){
    _instance = new SIPRegistrarClient(MOD_NAME);
  }
  return _instance;
}

SIPRegistrarClient::SIPRegistrarClient(const string& name)
  : AmSIPEventHandler(name),
    AmEventQueue(this) ,
    uac_auth_i(NULL),
    AmDynInvokeFactory(MOD_NAME)
{ 
}

void SIPRegistrarClient::run() {
  DBG("SIPRegistrarClient starting...\n");
  AmDynInvokeFactory* uac_auth_f = AmPlugIn::instance()->getFactory4Di("uac_auth");
  if (uac_auth_f == NULL) {
    DBG("unable to get a uac_auth factory. registrations will not be authenticated.\n");
    DBG("(do you want to load uac_auth module?)\n");
  } else {
    uac_auth_i = uac_auth_f->getInstance();
  }

  while (true) {
    if (registrations.size()) {
      unsigned int cnt = 250;
      while (cnt > 0) {
	usleep(2000); // every 2 ms
	processEvents();
	cnt--;
      }
      checkTimeouts();
    } else {
      waitForEvent();
      processEvents();
    }
  }
}
	
void SIPRegistration::onRegisterExpired() {
  if (sess_link.length()) {
    AmSessionContainer::instance()->postEvent(sess_link,
					      new SIPRegistrationEvent(SIPRegistrationEvent::RegisterTimeout,
								       req.from_tag));
  }
  DBG("Registration '%s' expired.\n", (info.user+"@"+info.domain).c_str());
  active = false;
  remove = true;
}

void SIPRegistration::onRegisterSendTimeout() {
  if (sess_link.length()) {
    AmSessionContainer::instance()->postEvent(sess_link,
					      new SIPRegistrationEvent(SIPRegistrationEvent::RegisterSendTimeout,
								       req.from_tag));
  }
  DBG("Registration '%s' REGISTER request timeout.\n", 
      (info.user+"@"+info.domain).c_str());
  active = false;
  remove = true;
}



void SIPRegistrarClient::checkTimeouts() {
  //	DBG("checking timeouts...\n");
  struct timeval now;
  gettimeofday(&now, NULL);
  reg_mut.lock();
  vector<string> remove_regs;

  for (map<string, SIPRegistration*>::iterator it = registrations.begin();
       it != registrations.end(); it++) {
    if (it->second->active) {
      if (it->second->registerExpired(now.tv_sec)) {
	SIPRegistration* reg = it->second;
	reg->onRegisterExpired();
      } else if (!it->second->waiting_result && 
		 it->second->timeToReregister(now.tv_sec)) {
	it->second->doRegistration();
      } 
    } else if (it->second->remove) {
      remove_regs.push_back(it->first);
    } else if (it->second->waiting_result && 
	       it->second->registerSendTimeout(now.tv_sec)) {
      SIPRegistration* reg = it->second;
      reg->onRegisterSendTimeout();
    }
  }
  for (vector<string>::iterator it = remove_regs.begin(); 
       it != remove_regs.end(); it++) {
    DBG("removing registration\n");
    SIPRegistration* reg = registrations[*it];
    registrations.erase(*it);
    if (reg)
      delete reg;
  }

  reg_mut.unlock();
}

int SIPRegistrarClient::onLoad() {
  instance()->start();
  return 0;
}

void SIPRegistrarClient::process(AmEvent* ev) {
  AmSipReplyEvent* sip_rep = dynamic_cast<AmSipReplyEvent*>(ev);
  if (sip_rep) {
    onSipReplyEvent(sip_rep);
    return;
  }

  SIPNewRegistrationEvent* new_reg = dynamic_cast<SIPNewRegistrationEvent*>(ev);
  if (new_reg) {
    onNewRegistration(new_reg);
    return;
  }

  SIPRemoveRegistrationEvent* rem_reg = dynamic_cast<SIPRemoveRegistrationEvent*>(ev);
  if (rem_reg) {
    onRemoveRegistration(rem_reg);
    return;
  }

}

void SIPRegistrarClient::onSipReplyEvent(AmSipReplyEvent* ev) {
  SIPRegistration* reg = get_reg(ev->reply.local_tag);
  if (reg != NULL) {
    reg->onSipReply(ev->reply);
  }
}

bool SIPRegistration::registerSendTimeout(time_t now_sec) {
  return now_sec > reg_send_begin + REGISTER_SEND_TIMEOUT;
}

bool SIPRegistration::timeToReregister(time_t now_sec) {
  //   	if (active) 
  //   		DBG("compare %lu with %lu\n",(reg_begin+reg_expires), (unsigned long)now_sec);
  return (((unsigned long)reg_begin+ reg_expires/2) < (unsigned long)now_sec);	
}

bool SIPRegistration::registerExpired(time_t now_sec) {
  return ((reg_begin+reg_expires) < (unsigned int)now_sec);	
}

void SIPRegistration::onSipReply(AmSipReply& reply) {
  if ((seh!=NULL) && seh->onSipReply(reply)) 
    return;

  waiting_result = false;

  dlg.updateStatus(reply);

  if ((reply.code>=200)&&(reply.code<300)) {
    DBG("positive reply to REGISTER!\n");
    size_t end  = 0;
    string local_contact_hdr = dlg.getContactHdr();
    local_contact.parse_contact(local_contact_hdr, (size_t)0, end);
    local_contact.dump();

    string contacts = getHeader(reply.hdrs, "Contact", "m");
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
	  bool str2i(const string& str, unsigned int& result);
					
	  if (str2i(server_contact.params["expires"], reg_expires)) {
	    ERROR("could not extract expires value.\n");
	    reg_expires = 500;
	  }
	  DBG("got an expires of %d\n", reg_expires);
	  // save TS
	  struct timeval now;
	  gettimeofday(&now, NULL);
	  reg_begin = now.tv_sec;

	  if (sess_link.length()) {
	    DBG("posting SIPRegistrationEvent to '%s'\n", sess_link.c_str());
	    AmSessionContainer::instance()->postEvent(sess_link,
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
	AmSessionContainer::instance()->postEvent(sess_link,
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
      AmSessionContainer::instance()->postEvent(sess_link,
						new SIPRegistrationEvent(SIPRegistrationEvent::RegisterFailed,
									 req.from_tag,
									 reply.code, reply.reason));
    }
    active = false;
    remove = true;		
  }
}

void SIPRegistrarClient::onNewRegistration(SIPNewRegistrationEvent* new_reg) {

  SIPRegistration* reg = new SIPRegistration(new_reg->handle, new_reg->info, 
					     new_reg->sess_link);
  
  if (uac_auth_i != NULL) {
    DBG("enabling UAC Auth for new registration.\n");
    
    // get a sessionEventHandler from uac_auth
    AmArg di_args,ret;
    AmArg a;
    a.setBorrowedPointer(reg);
    di_args.push(a);
    di_args.push(a);
    DBG("arg type is %d\n", a.getType());

    uac_auth_i->invoke("getHandler", di_args, ret);
    if (!ret.size()) {
      ERROR("Can not add auth handler to new registration!\n");
    } else {
      ArgObject* p = ret.get(0).asObject();
      if (p != NULL) {
	AmSessionEventHandler* h = dynamic_cast<AmSessionEventHandler*>(p);	
	if (h != NULL)
	  reg->setSessionEventHandler(h);
      }
    }
  }
  
  add_reg(new_reg->handle, reg);
  reg->doRegistration();
}

void SIPRegistrarClient::onRemoveRegistration(SIPRemoveRegistrationEvent* new_reg) {
  SIPRegistration* reg = get_reg(new_reg->handle);
  if (reg)
    reg->doUnregister();
}


void SIPRegistrarClient::on_stop() { }


bool SIPRegistrarClient::onSipReply(const AmSipReply& rep) {
  DBG("got reply with tag '%s'\n", rep.local_tag.c_str());
	
  if (instance()->hasRegistration(rep.local_tag)) {
    instance()->postEvent(new AmSipReplyEvent(rep));
    return true;
  } else 
    return false;
}

bool SIPRegistrarClient::hasRegistration(const string& handle) {
  return get_reg(handle) != NULL;
}

SIPRegistration* SIPRegistrarClient::
get_reg(const string& reg_id) 
{
  DBG("get registration '%s'\n", reg_id.c_str());
  SIPRegistration* res = NULL;
  reg_mut.lock();
  map<string, SIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end())
    res = it->second;
  reg_mut.unlock();
  DBG("get registration : res = '%ld' (this = %ld)\n", (long)res, (long)this);
  return res;
}

SIPRegistration* SIPRegistrarClient::
get_reg_unsafe(const string& reg_id) 
{
  //	DBG("get registration_unsafe '%s'\n", reg_id.c_str());
  SIPRegistration* res = NULL;
  map<string, SIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end())
    res = it->second;
  //     DBG("get registration_unsafe : res = '%ld' (this = %ld)\n", (long)res, (long)this);
  return res;
}

SIPRegistration* SIPRegistrarClient::
remove_reg(const string& reg_id) {
  reg_mut.lock();
  SIPRegistration* reg = remove_reg_unsafe(reg_id);
  reg_mut.unlock();
  return reg;
}

SIPRegistration* SIPRegistrarClient::
remove_reg_unsafe(const string& reg_id) {
  DBG("removing registration '%s'\n", reg_id.c_str());
  SIPRegistration* reg = NULL;
  map<string, SIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end()) {
    reg = it->second;
    registrations.erase(it);
  }
  return reg;
}

void SIPRegistrarClient::
add_reg(const string& reg_id, SIPRegistration* new_reg) 
{
  DBG("adding registration '%s'  (this = %ld)\n", reg_id.c_str(), (long)this);
  SIPRegistration* reg = NULL;
  reg_mut.lock();
  map<string, SIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end()) {
    reg = it->second;
		
  }
  registrations[reg_id] = new_reg;
  reg_mut.unlock();

  if (reg != NULL)
    delete reg; // old one with the same ltag

}


// API
string SIPRegistrarClient::createRegistration(const string& domain, 
					      const string& user,
					      const string& name,
					      const string& auth_user,
					      const string& pwd,
					      const string& sess_link) {
	
  string handle = AmSession::getNewId();
  instance()->
    postEvent(new SIPNewRegistrationEvent(SIPRegistrationInfo(domain, user, 
							      name, auth_user, pwd),
					  handle, sess_link));
  return handle;
}

void SIPRegistrarClient::removeRegistration(const string& handle) {
  instance()->
    postEvent(new SIPRemoveRegistrationEvent(handle));

}

bool SIPRegistrarClient::getRegistrationState(const string& handle, 
					      unsigned int& state, 
					      unsigned int& expires_left) {
  bool res = false;
  reg_mut.lock();

  SIPRegistration* reg = get_reg_unsafe(handle);
  if (reg) {
    res = true;
    state = reg->getState();
    expires_left = reg->getExpiresLeft();
  }
		
  reg_mut.unlock();
  return res;
}

void SIPRegistrarClient::invoke(const string& method, const AmArg& args, 
				AmArg& ret)
{
  if(method == "createRegistration"){
    ret.push(createRegistration(args.get(0).asCStr(),
				args.get(1).asCStr(),
				args.get(2).asCStr(),
				args.get(3).asCStr(),
				args.get(4).asCStr(),
				args.get(5).asCStr()
				).c_str());
  }
  else if(method == "removeRegistration"){
    removeRegistration(args.get(0).asCStr());
  } 
  else if(method == "getRegistrationState"){
    unsigned int state;
    unsigned int expires;
    if (instance()->getRegistrationState(args.get(0).asCStr(), 
					 state, expires)){
      ret.push(1);
      ret.push((int)state);
      ret.push((int)expires);
    } else {
      ret.push(AmArg((int)0));
    }
  } else if(method == "_list"){ 
    ret.push(AmArg("createRegistration"));
    ret.push(AmArg("removeRegistration"));
    ret.push(AmArg("getRegistrationState"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}

