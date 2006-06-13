/*
 * $Id: AnswerMachine.h,v 1.11.2.1 2005/08/31 13:54:30 rco Exp $
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

#ifndef _ANSWERMACHINE_H_
#define _ANSWERMACHINE_H_

#include "AmSession.h"
#include "AmConfigReader.h"
#include "EmailTemplate.h"
#include "AmMail.h"
#include "AmPlaylist.h"

#include <string>
using std::string;

/* how many seconds to wait until call is accepted with 200 */
#define DEFAULT_ACCEPT_DELAY 0

class AmMail;

class AnswerMachineFactory: public AmSessionFactory
{
    map<string, EmailTemplate> email_tmpl;

    int getEmailAddress();
    int loadEmailTemplates(const string& path);

public:
    static string RecFileExt;
    static string AnnouncePath;
    static string DefaultAnnounce;
    static int    MaxRecordTime;
    static int    AcceptDelay;
    static AmDynInvokeFactory* UserTimer;


    AnswerMachineFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req);
};

class AnswerMachineDialog : public AmSession
{
    AmAudioFile a_greeting,a_beep;
    AmAudioFile a_msg;
    AmPlaylist playlist;

    string announce_file;
    string msg_filename;

    const EmailTemplate* tmpl;
    EmailTmplDict  email_dict;

    AmDynInvoke* user_timer;

    int status;

    void request2dict(const AmSipRequest& req);
    void sendMailNotification();

 public:
    AnswerMachineDialog(const string& email, 
			const string& announce_file, 
			const EmailTemplate* tmpl);

    ~AnswerMachineDialog();

    void process(AmEvent* event);

    void onSessionStart(const AmSipRequest& req);
    void onBye(const AmSipRequest& req);
    void onDtmf(int event, int duration_msec) {}

    static void clean_up_mail(AmMail* mail);

    friend class AnswerMachineFactory;
};

#endif
// Local Variables:
// mode:C++
// End:

