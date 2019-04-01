/*
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

#include "SIPRegistrarClient.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"
#include "AmEventDispatcher.h"

#define MOD_NAME "registrar_client"

#include <unistd.h>

//EXPORT_SIP_EVENT_HANDLER_FACTORY(SIPRegistrarClient, MOD_NAME);
//EXPORT_PLUGIN_CLASS_FACTORY(SIPRegistrarClient, MOD_NAME);

extern "C" void* plugin_class_create()
{
    SIPRegistrarClient* reg_c = SIPRegistrarClient::instance();
    assert(dynamic_cast<AmDynInvokeFactory*>(reg_c));

    return (AmPluginFactory*)reg_c;
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
  : AmEventQueue(this),
    AmDynInvokeFactory(MOD_NAME),
    uac_auth_i(NULL),
    stop_requested(false)
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

  while (!stop_requested.get()) {
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

void SIPRegistrarClient::checkTimeouts() {
  //	DBG("checking timeouts...\n");
  struct timeval now;
  gettimeofday(&now, NULL);
  reg_mut.lock();
  vector<string> remove_regs;

  for (map<string, AmSIPRegistration*>::iterator it = registrations.begin();
       it != registrations.end(); it++) {
    if (it->second->active) {
      if (it->second->registerExpired(now.tv_sec)) {
	AmSIPRegistration* reg = it->second;
	reg->onRegisterExpired();
      } else if (!it->second->waiting_result && 
		 it->second->timeToReregister(now.tv_sec)) {
	it->second->doRegistration();
      } 
    } else if (it->second->remove) {
      remove_regs.push_back(it->first);
    } else if (it->second->waiting_result && 
	       it->second->registerSendTimeout(now.tv_sec)) {
      AmSIPRegistration* reg = it->second;
      reg->onRegisterSendTimeout();
    }
  }
  for (vector<string>::iterator it = remove_regs.begin(); 
       it != remove_regs.end(); it++) {
    DBG("removing registration\n");
    AmSIPRegistration* reg = registrations[*it];
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

void SIPRegistrarClient::onServerShutdown() {
  // TODO: properly wait until unregistered, with timeout
  DBG("shutdown SIP registrar client: deregistering\n");
  for (std::map<std::string, AmSIPRegistration*>::iterator it=
	 registrations.begin(); it != registrations.end(); it++) {
    it->second->doUnregister();
    AmEventDispatcher::instance()->delEventQueue(it->first);
  }

  stop_requested.set(true);
//   
//   setStopped();
//   return;
}

void SIPRegistrarClient::process(AmEvent* ev) 
{
  if (ev->event_id == E_SYSTEM) {
    AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(ev);
    if(sys_ev){	
      DBG("Session received system Event\n");
      if (sys_ev->sys_event == AmSystemEvent::ServerShutdown) {
	onServerShutdown();
      }
      return;
    }
  }

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
  AmSIPRegistration* reg = get_reg(ev->reply.from_tag);
  if (reg != NULL) {
    reg->getDlg()->onRxReply(ev->reply);
  }
}

void SIPRegistrarClient::onNewRegistration(SIPNewRegistrationEvent* new_reg) {

  AmSIPRegistration* reg = new AmSIPRegistration(new_reg->handle, new_reg->info, 
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
      AmObject* p = ret.get(0).asObject();
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
  AmSIPRegistration* reg = get_reg(new_reg->handle);
  if (reg)
    reg->doUnregister();
}


void SIPRegistrarClient::on_stop() { }


bool SIPRegistrarClient::onSipReply(const AmSipReply& rep, AmSipDialog::Status old_dlg_status) {
  DBG("got reply with tag '%s'\n", rep.from_tag.c_str());
	
  if (instance()->hasRegistration(rep.from_tag)) {
    instance()->postEvent(new AmSipReplyEvent(rep));
    return true;
  } else 
    return false;
}

bool SIPRegistrarClient::hasRegistration(const string& handle) {
  return get_reg(handle) != NULL;
}

AmSIPRegistration* SIPRegistrarClient::
get_reg(const string& reg_id) 
{
  DBG("get registration '%s'\n", reg_id.c_str());
  AmSIPRegistration* res = NULL;
  reg_mut.lock();
  map<string, AmSIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end())
    res = it->second;
  reg_mut.unlock();
  DBG("get registration : res = '%ld' (this = %ld)\n", (long)res, (long)this);
  return res;
}

AmSIPRegistration* SIPRegistrarClient::
get_reg_unsafe(const string& reg_id) 
{
  //	DBG("get registration_unsafe '%s'\n", reg_id.c_str());
  AmSIPRegistration* res = NULL;
  map<string, AmSIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end())
    res = it->second;
  //     DBG("get registration_unsafe : res = '%ld' (this = %ld)\n", (long)res, (long)this);
  return res;
}

AmSIPRegistration* SIPRegistrarClient::
remove_reg(const string& reg_id) {
  reg_mut.lock();
  AmSIPRegistration* reg = remove_reg_unsafe(reg_id);
  reg_mut.unlock();
  return reg;
}

AmSIPRegistration* SIPRegistrarClient::
remove_reg_unsafe(const string& reg_id) {
  DBG("removing registration '%s'\n", reg_id.c_str());
  AmSIPRegistration* reg = NULL;
  map<string, AmSIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end()) {
    reg = it->second;
    registrations.erase(it);
  }

  AmEventDispatcher::instance()->delEventQueue(reg_id);

  return reg;
}

void SIPRegistrarClient::
add_reg(const string& reg_id, AmSIPRegistration* new_reg) 
{
  DBG("adding registration '%s'  (this = %ld)\n", reg_id.c_str(), (long)this);
  AmSIPRegistration* reg = NULL;
  reg_mut.lock();
  map<string, AmSIPRegistration*>::iterator it = 
    registrations.find(reg_id);
  if (it!=registrations.end()) {
    reg = it->second;
		
  }
  registrations[reg_id] = new_reg;

  AmEventDispatcher::instance()->addEventQueue(reg_id,this);
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
					      const string& sess_link,
					      const string& proxy,
                                              const string& contact,
					      const string& handle) {
	
  string l_handle = handle.empty() ? AmSession::getNewId() : handle;
  instance()->
    postEvent(new SIPNewRegistrationEvent(SIPRegistrationInfo(domain, user, 
							      name, auth_user, pwd, 
							      proxy, contact),
					  l_handle, sess_link));

  return l_handle;
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

  AmSIPRegistration* reg = get_reg_unsafe(handle);
  if (reg) {
    res = true;
    state = reg->getState();
    expires_left = reg->getExpiresLeft();
  }
		
  reg_mut.unlock();
  return res;
}

void SIPRegistrarClient::listRegistrations(AmArg& res) {
  reg_mut.lock();

  for (map<string, AmSIPRegistration*>::iterator it = 
	 registrations.begin(); it != registrations.end(); it++) {
    AmArg r;
    r["handle"] = it->first;
    r["domain"] = it->second->getInfo().domain;
    r["user"] = it->second->getInfo().user;
    r["name"] = it->second->getInfo().name;
    r["auth_user"] = it->second->getInfo().auth_user;
    r["proxy"] = it->second->getInfo().proxy;
    r["event_sink"] = it->second->getEventSink();
    r["contact"] = it->second->getInfo().contact;
    res.push(r);
  }

  reg_mut.unlock();
}


void SIPRegistrarClient::invoke(const string& method, const AmArg& args, 
				AmArg& ret)
{
  if(method == "createRegistration"){
    string proxy, contact, handle;
    if (args.size() > 6)
      proxy = args.get(6).asCStr();
    if (args.size() > 7)
      contact = args.get(7).asCStr();
    if (args.size() > 8)
      handle = args.get(8).asCStr();

    ret.push(createRegistration(args.get(0).asCStr(),
				args.get(1).asCStr(),
				args.get(2).asCStr(),
				args.get(3).asCStr(),
				args.get(4).asCStr(),
				args.get(5).asCStr(),
				proxy, contact, handle
				).c_str());
  }
  else if(method == "removeRegistration"){
    removeRegistration(args.get(0).asCStr());
  } else if(method == "getRegistrationState"){
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
  } else if(method == "listRegistrations"){
    listRegistrations(ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("createRegistration"));
    ret.push(AmArg("removeRegistration"));
    ret.push(AmArg("getRegistrationState"));
    ret.push(AmArg("listRegistrations"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}

