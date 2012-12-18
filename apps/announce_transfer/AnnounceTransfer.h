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

#ifndef _ANNOUNCETRANSFER_H_
#define _ANNOUNCETRANSFER_H_

#include "AmSession.h"
#include "AmAudioFile.h"
#include "AmConfigReader.h"

#include <string>
using std::string;

/** \brief session factory for announce_transfer sessions */
class AnnounceTransferFactory: public AmSessionFactory
{
public:
  static string AnnouncePath;
  static string AnnounceFile;

  AnnounceTransferFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

/** \brief session logic implementation for announce_transfer sessions */
class AnnounceTransferDialog : public AmSession
{
  string callee_uri;

  AmAudioFile wav_file;
  string filename;

  unsigned int status;

  enum  { Disconnected = 0,
	  Announcing,
	  Transfering,
	  Hangup        // remote hangup 
  } AnnounceStatus; 
public:
  AnnounceTransferDialog(const string& filename);
  ~AnnounceTransferDialog();

  void onInvite(const AmSipRequest& req);
  void onSessionStart();
  void startSession();
  void onBye(const AmSipRequest& req);
  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipRequest& req, const AmSipReply& rep, 
		  AmBasicSipDialog::Status old_dlg_status);
  void onDtmf(int event, int duration_msec) {}

  void process(AmEvent* event);
};

#endif
// Local Variables:
// mode:C++
// End:

