/*
 * Copyright (C) 2002-2003 Fhg Fokus
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

#ifndef _ANNOUNCEMENT_H_
#define _ANNOUNCEMENT_H_

#include "AmSession.h"
#include "AmAudioFile.h"
#include "AmConfigReader.h"

#include "AmUACAuth.h"

#include <string>
using std::string;

#include <memory>

/** \brief Factory for announcement sessions */
class AnnouncementFactory: public AmSessionFactory
{
  inline string getAnnounceFile(const AmSipRequest& req);
public:
  static string AnnouncePath;
  static string AnnounceFile;
  static bool   Loop;

  AnnouncementFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      AmArg& session_params);

};

/**\brief  announcement session logic implementation */
class AnnouncementDialog : public AmSession,
			   public CredentialHolder
{
  AmAudioFile wav_file;
  string filename;

  std::auto_ptr<UACAuthCred> cred;
public:
  AnnouncementDialog(const string& filename, 
		     UACAuthCred* credentials = NULL);
  ~AnnouncementDialog();

  void onSessionStart();
  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec) {}

  void process(AmEvent* event);

  UACAuthCred* getCredentials();
};

#endif
// Local Variables:
// mode:C++
// End:

