/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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

#ifndef _ANNOUNCEAUTH_H_
#define _ANNOUNCEAUTH_H_

#include "AmSession.h"
#include "AmAudioFile.h"
#include "AmConfigReader.h"
#include "AmUACAuth.h"

#include <string>
using std::string;

class DialerThread : public AmThread {
  string r_uri;
  string from;
  string from_uri;
  string to;
protected:
  void run();
  void on_stop();
public:
  void set_dial(const string& r, const string& f, 
		const string& fu, const string& t);


};

class AnnounceAuthFactory: public AmSessionFactory
{
  DialerThread dialer;    

  string auth_realm;
  string auth_user;
  string auth_pwd;

public:
  static string AnnouncePath;
  static string AnnounceFile;

  AnnounceAuthFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

class AnnounceAuthDialog : public AmSession,
			   public CredentialHolder
{
  AmAudioFile wav_file;
  string filename;
  UACAuthCred credentials;

public:
  AnnounceAuthDialog(const string& filename,
		     const string& auth_realm, 
		     const string& auth_user,
		     const string& auth_pwd);
  ~AnnounceAuthDialog();

  void onSessionStart();
  void startSession();
  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec) {}

  void process(AmEvent* event);
  inline UACAuthCred* getCredentials();
};


#endif
// Local Variables:
// mode:C++
// End:

