/*
 * Copyright (C) 2006 iptel.org GmbH
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

#ifndef _EarlyAnnounce_h_
#define _EarlyAnnounce_h_

#ifdef USE_MYSQL
#include <mysql++/mysql++.h>
#endif

#include "AmB2BSession.h"
#include "AmAudioFile.h"
#include "AmConfigReader.h"

#include <string>
using std::string;

/** \brief Factory for early_announce sessions */
class EarlyAnnounceFactory: public AmSessionFactory
{
public:

#ifdef USE_MYSQL
  static mysqlpp::Connection Connection;
  static string AnnounceApplication;
  static string AnnounceMessage;
  static string DefaultLanguage;
#else 
  static string AnnouncePath;
  static string AnnounceFile;
#endif
  enum ContB2B {
    Always = 0,
    Never,
    AppParam
  };
  static ContB2B ContinueB2B;

  EarlyAnnounceFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

/** \brief session logic implementation for early_announce sessions */
class EarlyAnnounceDialog : public AmB2BCallerSession
{
  AmAudioFile wav_file;
  string filename;
    
public:
  EarlyAnnounceDialog(const string& filename);
  ~EarlyAnnounceDialog();

  void onInvite(const AmSipRequest& req);
  void onEarlySessionStart();
  void onBye(const AmSipRequest& req);
  void onCancel(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec) {}

  void process(AmEvent* event);

};

#endif
// Local Variables:
// mode:C++
// End:

