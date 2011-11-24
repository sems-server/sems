/*
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _SERVICELINE_H_
#define _SERVICELINE_H_

#include "AmApi.h"
#include "AmConfigReader.h"
#include "AmAudioFile.h"
#include "AmB2ABSession.h"
#include "AmUACAuth.h"

#include <string>
using std::string;

class ServiceLineFactory: public AmSessionFactory
{
public:
  static string AnnouncePath;
  static string AnnounceFile;

  static string callee_numbers[10];
  
  static string GWDomain;
  static string GWUser;
  static string GWDisplayname;
	
  static string GWAuthuser;
  static string GWAuthrealm;
  static string GWAuthpwd;

  ServiceLineFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

class ServiceLineCallerDialog: public AmB2ABCallerSession
{
  AmAudioFile wav_file;
  // we use a playlist to keep media processing, RTP 
  // sending & DTMF detection after prompt has finished
  AmPlaylist playlist;
  string filename;

  string callee_addr;
  string callee_uri;

  bool started;
    
public:
  ServiceLineCallerDialog(const string& filename);
    
  void process(AmEvent* event);
  void onDtmf(int event, int duration);
  void onSessionStart();

  virtual AmB2ABCalleeSession* createCalleeSession();
};

// need this for auth
class ServiceLineCalleeDialog 
  : public AmB2ABCalleeSession,
    public CredentialHolder {
  UACAuthCred cred;

public:
  ServiceLineCalleeDialog(const string& other_tag, 
			  AmSessionAudioConnector* connector);
  ~ServiceLineCalleeDialog();
  UACAuthCred* getCredentials();
};

#endif
// Local Variables:
// mode:C++
// End:

