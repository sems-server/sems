/*
 * $Id: PinAuthConference.h 288 2007-03-28 16:32:02Z sayer $
 *
 * Copyright (C) 2007 iptego GmbH
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
#ifndef _PINAUTHCONFERENCE_H_
#define _PINAUTHCONFERENCE_H_

#include "AmApi.h"
#include "AmSession.h"
#include "AmAudio.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"
#include "ampi/UACAuthAPI.h"

#include "RoomInfo.h"

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
#define ANNOUNCE_PATH "../apps/examples/webconference/"

class WebConferenceEvent : public AmEvent 
{
public:
  WebConferenceEvent(int id) : AmEvent(id) { }
  enum { Kick,
	 Mute,
	 Unmute 
  };
};

class WebConferenceFactory 
  : public AmSessionFactory,
    public AmDynInvokeFactory,
    public AmDynInvoke
{
  AmPromptCollection prompts;

  map<string, ConferenceRoom> rooms;
  AmMutex rooms_mut;

  // for DI 
  static WebConferenceFactory* _instance;
  bool configured;

  string getRandomPin();
  /** returns NULL if adminpin wrong */
  ConferenceRoom* getRoom(const string& room, 
			  const string& adminpin);
  void postConfEvent(const AmArgArray& args, AmArgArray& ret,
		     int id, int mute);
public:
  static string DigitsDir;
  static PlayoutType m_PlayoutType;

  WebConferenceFactory(const string& _app_name);
  AmSession* onInvite(const AmSipRequest&);
  AmSession* onInvite(const AmSipRequest& req,
		      AmArg& session_params);
  int onLoad();

  inline void newParticipant(const string& conf_id, 
			     const string& localtag, 
			     const string& number);
  inline void updateStatus(const string& conf_id, 
			   const string& localtag, 
			   ConferenceRoomParticipant::ParticipantStatus status,
			   const string& reason);

  // DI API
  WebConferenceFactory* getInstance(){
    return _instance;
  }
  void invoke(const string& method, const AmArgArray& args, AmArgArray& ret);

  // DI functions
  void roomCreate(const AmArgArray& args, AmArgArray& ret);
  void roomInfo(const AmArgArray& args, AmArgArray& ret);
  void dialout(const AmArgArray& args, AmArgArray& ret);
  void kickout(const AmArgArray& args, AmArgArray& ret);
  void mute(const AmArgArray& args, AmArgArray& ret);
  void unmute(const AmArgArray& args, AmArgArray& ret);
  void serverInfo(const AmArgArray& args, AmArgArray& ret);
};

class WebConferenceDialog 
  : public AmSession,
    public CredentialHolder
{
public:
  enum WebConferenceState {
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
  void disconnectConference();

  WebConferenceState state;

  WebConferenceFactory* factory;
  bool is_dialout; 
  UACAuthCred* cred;

public:
  WebConferenceDialog(AmPromptCollection& prompts,
		      WebConferenceFactory* my_f, 
		      UACAuthCred* cred);
  ~WebConferenceDialog();

  void process(AmEvent* ev);
  void onSipReply(const AmSipReply& reply);
  void onSessionStart(const AmSipRequest& req);
  void onSessionStart(const AmSipReply& rep);
  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);

  UACAuthCred* getCredentials() { return cred; }

};

#endif
// Local Variables:
// mode:C++
// End:

