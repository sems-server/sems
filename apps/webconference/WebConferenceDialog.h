#ifndef _WebconferenceDialog_H_
#define _WebconferenceDialog_H_

/*
 * Copyright (C) 2007-2008 iptego GmbH
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

#include "AmApi.h"
#include "AmSession.h"
#include "AmAudio.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"
#include "AmUACAuth.h"
#include "AmRingTone.h"

class WebConferenceFactory;

class WebConferenceDialog 
  : public AmSession,
    public CredentialHolder
{
public:
  enum WebConferenceState {
    None = 0,
    EnteringPin,
    EnteringConference,
    InConference,
    InConferenceRinging,
    InConferenceEarly,
    PlayErrorFinish
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

  // ID from X-ParticipantID header
  string participant_id;

  AmAudio *local_input;
  void setLocalInput(AmAudio* in);

  /** flag to indicate whether user was joined by anyone in the room */
  bool lonely_user;

public:
  WebConferenceDialog(AmPromptCollection& prompts,
		      WebConferenceFactory* my_f, 
		      UACAuthCred* cred);
  WebConferenceDialog(AmPromptCollection& prompts,
		      WebConferenceFactory* my_f, 
		      const string& room);
  ~WebConferenceDialog();

  void process(AmEvent* ev);
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);

  void onInvite(const AmSipRequest& req);

  void onSessionStart();
  void onEarlySessionStart();
  void onRinging(const AmSipReply& rep);

  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);

  void onSessionTimeout();
  void onRtpTimeout();

  UACAuthCred* getCredentials() { return cred; }

  // overriden media processing (local_input)
  virtual int readStreams(unsigned long long ts, unsigned char *buffer);
  virtual bool isAudioSet();
  virtual void clearAudio();
};

#endif
// Local Variables:
// mode:C++
// End:

