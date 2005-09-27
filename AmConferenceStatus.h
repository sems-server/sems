/*
 * $Id: AmConferenceStatus.h,v 1.1.2.1 2005/03/07 21:34:45 sayer Exp $
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

#ifndef _ConferenceStatus_h_
#define _ConferenceStatus_h_

#include "AmThread.h"
#include "AmRtpStream.h"
#include "AmMultiPartyMixer.h"
#include "AmEventQueue.h"

#include <set>
#include <queue>
#include <string>
using std::set;
using std::pair;
using std::queue;
using std::string;

class AmConferenceDialog;
class AmConferenceStatusContainer;

class AmConferenceStatus
{
    
    string            conf_id;
    AmSharedVar<bool> runcond;

    AmMultiPartyMixer        mixer;

    map<AmSession*, unsigned int> sessions; // session, channel #
    AmMutex                sessions_mut;

    void process(AmEvent* event);

    AmConferenceStatus(const string& conference_id);
    ~AmConferenceStatus();

 public:
    friend class AmConferenceStatusContainer;
    unsigned int add_channel(AmSession* session);
    int remove_channel(AmSession* session);

    AmMultiPartyMixer* get_mixer() { return &mixer; }

    bool getStopped() { return ! runcond.get(); }
    
};

class AmConferenceStatusContainer : public AmThread
{
    static AmConferenceStatusContainer* _instance;

    map<string,AmConferenceStatus*> cid2status;
    AmMutex                       cid2s_mut;

    queue<AmConferenceStatus*> d_status;
    AmMutex                  ds_mut;

    /** the daemon only runs if this is true */
    AmCondition<bool> _run_cond;
    
    AmConferenceStatusContainer();
    
    // AmThread interface
    virtual void run();
    virtual void on_stop();

public:
    static AmConferenceStatusContainer* instance();

    AmConferenceStatus* get_status(const string& conf_id);

    AmConferenceStatus* add_status(const string& conf_id);

    void destroyStatus(const string& conf_id);
};

#endif
// Local Variables:
// mode:C++
// End:

