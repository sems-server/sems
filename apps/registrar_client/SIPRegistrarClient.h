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

#include "AmApi.h"
#include "AmSession.h"
#include "ContactInfo.h"

#include "ampi/SIPRegistrarClientAPI.h"
#include "ampi/UACAuthAPI.h"

#include <sys/time.h>

#include <map>
#include <string>
using std::map;
using std::string;

struct SIPRegistrationInfo {
  string domain;
  string user;
  string name;
  string auth_user;
  string pwd;
  string proxy;

  SIPRegistrationInfo(const string& domain,
		      const string& user,
		      const string& name,
		      const string& auth_user,
		      const string& pwd,
		      const string& proxy)
    : domain(domain),user(user),name(name),
    auth_user(auth_user),pwd(pwd),proxy(proxy)
  { }
};

class SIPRegistration : public AmSipDialogEventHandler,
			public DialogControl,
			public CredentialHolder
	
{
	
  AmSipDialog dlg;
  UACAuthCred cred;

  SIPRegistrationInfo info;

  // session to post events to 
  string sess_link;      

  AmSessionEventHandler* seh;

  AmSipRequest req;

  ContactInfo server_contact;
  ContactInfo local_contact;

  time_t reg_begin;	
  unsigned int reg_expires;
  time_t reg_send_begin; 

 public:
  SIPRegistration(const string& handle,
		  const SIPRegistrationInfo& info,
		  const string& sess_link);
  ~SIPRegistration();

  void setSessionEventHandler(AmSessionEventHandler* new_seh);

  void doRegistration();
  void doUnregister();
	
  inline bool timeToReregister(time_t now_sec);
  inline bool registerExpired(time_t now_sec);
  void onRegisterExpired();
  void onRegisterSendTimeout();

  inline bool registerSendTimeout(time_t now_sec);

  void onSendRequest(const string& method,
		     const string& content_type,
		     const string& body,
		     string& hdrs,
		     int flags,
		     unsigned int cseq);
	
  void onSendReply(const AmSipRequest& req,
		   unsigned int  code,
		   const string& reason,
		   const string& content_type,
		   const string& body,
		   string& hdrs,
		   int flags);

  // DialogControl if
  AmSipDialog* getDlg() { return &dlg; }
  // CredentialHolder	
  UACAuthCred* getCredentials() { return &cred; }

  void onSipReply(const AmSipReply& reply, int old_dlg_status, const string& trans_method);
  void onSipRequest(const AmSipRequest& req) {}
  void onInvite2xx(const AmSipReply&) {}
  void onNoAck(unsigned int) {}
  void onNoPrack(const AmSipRequest &, const AmSipReply &) {}

  /** is this registration registered? */
  bool active; 
  /** should this registration be removed from container? */
  bool remove;
  /** are we waiting for the response to a register? */
  bool waiting_result;

  enum RegistrationState {
    RegisterPending = 0,
    RegisterActive,
    RegisterExpired
  };
  /** return the state of the registration */
  RegistrationState getState(); 
  /** return the expires left for the registration */
  unsigned int getExpiresLeft(); 

  SIPRegistrationInfo& getInfo() { return info; }
  const string& getEventSink() { return sess_link; }
};

class SIPNewRegistrationEvent;
class SIPRemoveRegistrationEvent;

class SIPRegistrarClient  : public AmThread,
			    public AmEventQueue,
			    public AmEventHandler,
			    public AmDynInvoke,
			    public AmDynInvokeFactory
{
  // registrations container
  AmMutex                       reg_mut;
  std::map<std::string, SIPRegistration*> registrations;

  void add_reg(const string& reg_id, 
	       SIPRegistration* new_reg);
  SIPRegistration* remove_reg(const string& reg_id);
  SIPRegistration* remove_reg_unsafe(const string& reg_id);
  SIPRegistration* get_reg(const string& reg_id);
  SIPRegistration* get_reg_unsafe(const string& reg_id);

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
	
  bool onSipReply(const AmSipReply& rep, int old_dlg_status, const string& trans_method);
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
			    const string& proxy);
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
    : info(info), handle(handle), sess_link(sess_link), 
       AmEvent(SIPRegistrarClient::AddRegistration) { }


  string handle;
  string sess_link;
  SIPRegistrationInfo info;
};

class SIPRemoveRegistrationEvent : public AmEvent {
 public:
  string handle;
  SIPRemoveRegistrationEvent(const string& handle) 
    : handle(handle), 
    AmEvent(SIPRegistrarClient::RemoveRegistration) { }
};

#endif
