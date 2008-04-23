#ifndef _WebconferenceDialog_H_
#define _WebconferenceDialog_H_

/*
 * $Id: PinAuthConference.h 288 2007-03-28 16:32:02Z sayer $
 *
 * Copyright (C) 2007-2008 iptego GmbH
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

#include "AmApi.h"
#include "AmSession.h"
#include "AmAudio.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"
#include "ampi/UACAuthAPI.h"
#include "AmRingTone.h"

class WebConferenceFactory;

class WebConferenceDialog 
  : public AmSession,
    public CredentialHolder
{
public:
  enum WebConferenceState {
    None,
    EnteringPin,
    EnteringConference,
    InConference,
    InConferenceRinging,
    InConferenceEarly
  }; 

private:
  AmPlaylist  play_list;
  AmPlaylistSeparator separator;

  AmPromptCollection& prompts;

  // our ring tone
  auto_ptr<AmRingTone> RingTone;

  // our connection to the conference
  auto_ptr<AmConferenceChannel> channel;
  string  conf_id;
  string pin_str;

  void connectConference(const string& room);
  void disconnectConference();

  void onKicked();
  void onMuted(bool mute);

  WebConferenceState state;

  WebConferenceFactory* factory;
  bool is_dialout; 
  UACAuthCred* cred;

  bool muted;

  time_t connect_ts;
  time_t disconnect_ts;

public:
  WebConferenceDialog(AmPromptCollection& prompts,
		      WebConferenceFactory* my_f, 
		      UACAuthCred* cred);
  WebConferenceDialog(AmPromptCollection& prompts,
		      WebConferenceFactory* my_f, 
		      const string& room);
  ~WebConferenceDialog();

  void process(AmEvent* ev);
  void onSipReply(const AmSipReply& reply);
  void onSessionStart(const AmSipRequest& req);
  void onSessionStart(const AmSipReply& rep);
  void onEarlySessionStart(const AmSipReply& rep);
  void onRinging(const AmSipReply& rep);

  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);

  UACAuthCred* getCredentials() { return cred; }

};

#endif
// Local Variables:
// mode:C++
// End:

