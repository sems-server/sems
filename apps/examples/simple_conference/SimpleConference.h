/*
 * $Id: Conference.h 288 2007-03-28 16:32:02Z sayer $
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
#ifndef _SIMPLECONFERENCE_H_
#define _SIMPLECONFERENCE_H_

#include "AmApi.h"
#include "AmSession.h"
#include "AmAudioFile.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"

#include <map>
#include <string>
using std::map;
using std::string;

class ConferenceStatus;
class ConferenceStatusContainer;

// of course this should be taken from config 
// - just wanted to make things simple
#define BEEP_FILE_NAME "../apps/examples/simple_conference/wav/beep.wav"

class SimpleConferenceFactory : public AmSessionFactory
{
public:
  SimpleConferenceFactory(const string& _app_name);
  AmSession* onInvite(const AmSipRequest&);
  AmSession* onInvite(const AmSipReply&);
  int onLoad();
};

class SimpleConferenceDialog : public AmSession
{
  string                        conf_id;
  // our connection to the conference
  auto_ptr<AmConferenceChannel> channel;

  // we use a playlist so we can put e.g. 
  // announcement files to be played to the 
  // user in front and after its finished we will 
  // be connected back to conference automatically 
  AmPlaylist  play_list;
  auto_ptr<AmAudioFile> BeepSound;


public:
  SimpleConferenceDialog();
  ~SimpleConferenceDialog();

  void process(AmEvent* ev);
  void onSessionStart(const AmSipRequest& req);
  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);
};

#endif
// Local Variables:
// mode:C++
// End:

