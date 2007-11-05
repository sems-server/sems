/*
 * $Id: DtmfTester.h 457 2007-09-24 17:37:04Z sayer $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _ANNOUNCEMENT_H_
#define _ANNOUNCEMENT_H_

#include "AmSession.h"
#include "AmConfigReader.h"
#include "AmPlaylist.h"

#include "ampi/UACAuthAPI.h"

#include <string>
using std::string;

#include <memory>

/** \brief Factory for announcement sessions */
class DtmfTesterFactory: public AmSessionFactory
{
  inline string getAnnounceFile(const AmSipRequest& req);
public:
  static string AnnouncePath;
  static string AnnounceFile;

  DtmfTesterFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
  AmSession* onInvite(const AmSipRequest& req,
		      AmArg& session_params);

};

/**\brief  announcement session logic implementation */
class DtmfTesterDialog : public AmSession,
			   public CredentialHolder
{
  AmAudioFile wav_file;
  AmAudioFile rec_file;
  string filename;

  AmPlaylist play_list;

  std::auto_ptr<UACAuthCred> cred;

  vector<AmAudioFile*> del_files;

public:
  DtmfTesterDialog(const string& filename, 
		     UACAuthCred* credentials = NULL);
  ~DtmfTesterDialog();

  void onSessionStart(const AmSipRequest& req);
  void onSessionStart(const AmSipReply& rep);
  void startSession();
  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec);

  void process(AmEvent* event);

  UACAuthCred* getCredentials();
};

#endif
// Local Variables:
// mode:C++
// End:

