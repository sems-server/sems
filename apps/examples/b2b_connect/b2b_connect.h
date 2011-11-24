/*
 * Copyright (C) 2008 iptego GmbH
 * Based on the concept of sw_prepaid_sip, Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of mycc, Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _B2B_CONNECT_H
#define _B2B_CONNECT_H

#include "AmB2ABSession.h"
#include "AmUACAuth.h"

using std::string;

class b2b_connectFactory: public AmSessionFactory
{

 public:
  b2b_connectFactory(const string& _app_name);
  
  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);

  static bool TransparentHeaders; // default
  static bool TransparentDestination; // default

};

class b2b_connectDialog : public AmB2ABCallerSession
{
  string domain;
  string user;
  string password;
  string from;
  string to;

  AmSipRequest invite_req;
  
 public:

  b2b_connectDialog();
  ~b2b_connectDialog();
  
  void onSessionStart();
  void onB2ABEvent(B2ABEvent* ev);
  void process(AmEvent* ev);
  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel(const AmSipRequest& req);
  

 protected:
  
  AmB2ABCalleeSession* createCalleeSession();
};

class b2b_connectCalleeSession 
: public AmB2ABCalleeSession, public CredentialHolder
{
  UACAuthCred credentials;
  AmSipRequest invite_req;

 protected:
  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
 
public:
  b2b_connectCalleeSession(const string& other_tag, 
			   AmSessionAudioConnector* connector,
			   const string& user, const string& pwd); 
  ~b2b_connectCalleeSession();

  inline UACAuthCred* getCredentials();
};
#endif                           
