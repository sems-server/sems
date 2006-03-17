/*
 * $Id: Conference.h,v 1.6 2005/06/17 10:41:06 sayer Exp $
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

#ifndef _CONFERENCE_H_
#define _CONFERENCE_H_

#include "AmApi.h"
#include "AmThread.h"
#include "AmSession.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"

#include <map>
#include <string>
using std::map;
using std::string;

class ConferenceStatus;
class ConferenceStatusContainer;

class ConferenceFactory : public AmSessionFactory
{
public:
    static string LonelyUserFile;
    static string JoinSound;
    static string DropSound;

    ConferenceFactory(const string& _app_name);
    virtual AmSession* onInvite(const AmSipRequest&);
    virtual int onLoad();
};

class ConferenceDialog : public AmSession
{
    AmPlaylist  play_list;

    auto_ptr<AmAudioFile> LonelyUserFile;
    auto_ptr<AmAudioFile> JoinSound;
    auto_ptr<AmAudioFile> DropSound;

    string                        conf_id;
    auto_ptr<AmConferenceChannel> channel;

public:
    ConferenceDialog(const string& conf_id);
    ~ConferenceDialog();

    void process(AmEvent* ev);
    void onStart();
    void onSessionStart(const AmSipRequest& req);
    void onBye(const AmSipRequest& req);
};

#endif
// Local Variables:
// mode:C++
// End:

