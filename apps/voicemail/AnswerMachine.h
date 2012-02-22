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

#ifndef _ANSWERMACHINE_H_
#define _ANSWERMACHINE_H_

#ifdef USE_MYSQL
#include <mysql++/mysql++.h>
#endif

#include "AmSession.h"
#include "AmConfigReader.h"
#include "EmailTemplate.h"
#include "AmMail.h"
#include "AmPlaylist.h"
#include "AmAudioFile.h"

#include <string>
#include <map>
#include <vector>
using std::string;
using std::map;
using std::vector;

// defaults for config options
#define DEFAULT_RECORD_TIME 30
#define DEFAULT_ANNOUNCE    "default.wav"
#define SMTP_ADDRESS_IP     "localhost"
#define SMTP_PORT           25

struct AmMail;

#define MODE_VOICEMAIL 0
#define MODE_BOX       1
#define MODE_BOTH      2
#define MODE_ANN       3

#define MSG_SEPARATOR "+" // separates values in message name

class AnswerMachineFactory: public AmSessionFactory
{
    map<string, EmailTemplate> email_tmpl;

    int getEmailAddress();

#ifdef USE_MYSQL
    int loadEmailTemplatesFromMySQL();
#else
    int loadEmailTemplates(const string& path);
#endif

  FILE* getMsgStoreGreeting(string msgname, string user, string domain);
  AmDynInvoke* msg_storage;

public:
  static string EmailAddress;
  static string RecFileExt;
  static string AnnouncePath;
  static string DefaultAnnounce;
  static int    MaxRecordTime;
  static AmDynInvokeFactory* MessageStorage;
  static bool SaveEmptyMsg;
  static bool TryPersonalGreeting;
  static int  DefaultVMMode;
  static bool SimpleMode;

  static vector<string> MailHeaderVariables;

  /** After server start, IP of the SMTP server. */
  static string SmtpServerAddress;
  /** SMTP server port. */
  static unsigned int SmtpServerPort;

#ifdef USE_MYSQL
  static mysqlpp::Connection Connection;
#endif

  AnswerMachineFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

class AnswerMachineDialog : public AmSession
{
    AmAudioFile a_greeting,a_beep;
    AmAudioFile a_msg;
    AmPlaylist playlist;

    string announce_file;
    FILE* announce_fp;
    string msg_filename;


    const EmailTemplate* tmpl;
    EmailTmplDict  email_dict;

    AmDynInvoke* msg_storage;

    int status;
    int vm_mode; // MODE_*

    //void sendMailNotification();
    void saveMessage();
    void saveBox(FILE* fp);

    void onNoAudio();

 public:
  AnswerMachineDialog(const string& user, 
		      const string& sender, 
		      const string& domain,
		      const string& email, 
		      const string& announce_file, 
		      const string& uid,
		      const string& did,
		      FILE* announce_fp, 
		      int vm_mode,
		      const EmailTmplDict& template_variables,
		      const EmailTemplate* tmpl);

    ~AnswerMachineDialog();

    void process(AmEvent* event);

    void onSessionStart();
    void onBye(const AmSipRequest& req);
    void onDtmf(int event, int duration_msec) {}

    friend class AnswerMachineFactory;
};

#endif
// Local Variables:
// mode:C++
// End:

