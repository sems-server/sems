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

#ifndef RegisterClient_h
#define RegisterClient_h

#include "AmSipRegistration.h"
#include "AmApi.h"

#include <sys/time.h>

#include <map>
#include <string>
using std::map;
using std::string;

struct SIPNewRegistrationEvent;
class SIPRemoveRegistrationEvent;

class SIPRegistrarClient  : public AmThread,
			    public AmEventQueue,
			    public AmEventHandler,
			    public AmDynInvoke,
			    public AmDynInvokeFactory
{
  // registrations container
  AmMutex                       reg_mut;
  std::map<std::string, AmSIPRegistration*> registrations;

  void add_reg(const string& reg_id, 
	       AmSIPRegistration* new_reg);
  AmSIPRegistration* remove_reg(const string& reg_id);
  AmSIPRegistration* remove_reg_unsafe(const string& reg_id);
  AmSIPRegistration* get_reg(const string& reg_id);
  AmSIPRegistration* get_reg_unsafe(const string& reg_id);

  void onSipReplyEvent(AmSipReplyEvent* ev);	
  void onNewRegistration(SIPNewRegistrationEvent* new_reg);
  void onRemoveRegistration(SIPRemoveRegistrationEvent* new_reg);
  void listRegistrations(AmArg& res);

  static SIPRegistrarClient* _instance;

  AmDynInvoke* uac_auth_i;

  AmSharedVar<bool> stop_requested;
  void checkTimeouts();
  void onServerShutdown();
 public:
  SIPRegistrarClient(const string& name);
  // DI factory
  AmDynInvoke* getInstance() { return instance(); }
  // DI API
  static SIPRegistrarClient* instance();
  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);
	
  bool onSipReply(const AmSipReply& rep, AmSipDialog::Status old_dlg_status);
  int onLoad();
	
  void run();
  void on_stop();
  void process(AmEvent* ev);


  // API
  string createRegistration(const string& domain, 
			    const string& user,
			    const string& name,
			    const string& auth_user,
			    const string& pwd,
			    const string& sess_link,
			    const string& proxy,
			    const string& contact,
			    const string& handle);
  void removeRegistration(const string& handle);

  bool hasRegistration(const string& handle);

  bool getRegistrationState(const string& handle, unsigned int& state, 
			    unsigned int& expires_left);

  enum {
    AddRegistration,
    RemoveRegistration
  } RegEvents;

};

struct SIPNewRegistrationEvent : public AmEvent {
 
  SIPNewRegistrationEvent(const SIPRegistrationInfo& info,
			  const string& handle, 
			  const string& sess_link)
    : AmEvent(SIPRegistrarClient::AddRegistration),
      handle(handle), sess_link(sess_link), info(info) { }


  string handle;
  string sess_link;
  SIPRegistrationInfo info;
};

class SIPRemoveRegistrationEvent : public AmEvent {
 public:
  string handle;
  SIPRemoveRegistrationEvent(const string& handle) 
    : AmEvent(SIPRegistrarClient::RemoveRegistration),
    handle(handle) { }
};

#endif
