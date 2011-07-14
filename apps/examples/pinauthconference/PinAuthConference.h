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
#ifndef _PINAUTHCONFERENCE_H_
#define _PINAUTHCONFERENCE_H_

#include "AmApi.h"
#include "AmSession.h"
#include "AmAudio.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"

#include <map>
#include <string>
using std::map;
using std::string;

class ConferenceStatus;
class ConferenceStatusContainer;

// configuration parameter names
#define ENTERING_CONFERENCE "entering_conference"
#define FIRST_PARTICIPANT   "first_participant"
#define JOIN_SOUND          "join_sound"
#define DROP_SOUND          "drop_sound"
#define ENTER_PIN           "enter_pin"
#define WRONG_PIN           "wrong_pin"

// default path for files
#define ANNOUNCE_PATH "../apps/examples/pinauthconference/wav/"

class PinAuthConferenceFactory : public AmSessionFactory
{

  AmPromptCollection prompts;
public:
  static string DigitsDir;
  static PlayoutType m_PlayoutType;

  PinAuthConferenceFactory(const string& _app_name);
  AmSession* onInvite(const AmSipRequest&, const string& app_name,
		      const map<string,string>& app_params);
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      AmArg& session_params);
  int onLoad();
};

class PinAuthConferenceDialog : public AmSession
{
public:
  enum PinAuthConferenceState {
    None,
    EnteringPin,
    EnteringConference,
    InConference
  }; 

private:
  AmPlaylist  play_list;
  AmPlaylistSeparator separator;

  AmPromptCollection& prompts;

  // our connection to the conference
  auto_ptr<AmConferenceChannel> channel;
  string  conf_id;
  string pin_str;

  void connectConference(const string& room);

  PinAuthConferenceState state;

public:
  PinAuthConferenceDialog(AmPromptCollection& prompts);
  ~PinAuthConferenceDialog();

  void process(AmEvent* ev);
  void onSessionStart();
  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);
};

#endif
// Local Variables:
// mode:C++
// End:

