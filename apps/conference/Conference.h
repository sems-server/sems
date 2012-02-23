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

#ifndef _CONFERENCE_H_
#define _CONFERENCE_H_

#ifdef USE_MYSQL
#include <mysql++/mysql++.h>
#endif

#include "AmApi.h"
#include "AmThread.h"
#include "AmSession.h"
#include "AmAudioFile.h"
#include "AmConferenceChannel.h"
#include "AmPlaylist.h"
#include "AmRingTone.h"

#include <map>
#include <string>
using std::map;
using std::string;

class ConferenceStatus;
class ConferenceStatusContainer;


enum { CS_normal=0,
       CS_dialing_out,
       CS_dialed_out,
       CS_dialout_connected };

enum { DoConfConnect = 100,
       DoConfDisconnect,
       DoConfRinging,
       DoConfError
};

/** \brief Event to trigger connecting/disconnecting between dialout session and main conference session */
struct DialoutConfEvent : public AmEvent {

  string conf_id;
    
  DialoutConfEvent(int event_id,
		   const string& conf_id)
    : AmEvent(event_id),
      conf_id(conf_id)
  {}
};

/** \brief Factory for conference sessions */
class ConferenceFactory : public AmSessionFactory
{
  static AmSessionEventHandlerFactory* session_timer_f;
  static AmConfigReader cfg;

public:
  static string AudioPath;
  static string LonelyUserFile;
  static string JoinSound;
  static string DropSound;
  static string DialoutSuffix;
  static PlayoutType m_PlayoutType;
  static unsigned int MaxParticipants;
  static bool UseRFC4240Rooms;

  static void setupSessionTimer(AmSession* s);

#ifdef USE_MYSQL
  static mysqlpp::Connection Connection;
#endif

  ConferenceFactory(const string& _app_name);
  virtual AmSession* onInvite(const AmSipRequest&, const string& app_name,
			      const map<string,string>& app_params);
  virtual AmSession* onRefer(const AmSipRequest& req, const string& app_name,
			     const map<string,string>& app_params);
  virtual int onLoad();
};

/** \brief session logic implementation of conference sessions */
class ConferenceDialog : public AmSession
{
  AmPlaylist  play_list;

  auto_ptr<AmAudioFile> LonelyUserFile;
  auto_ptr<AmAudioFile> JoinSound;
  auto_ptr<AmAudioFile> DropSound;
  auto_ptr<AmRingTone>  RingTone;
  auto_ptr<AmRingTone>  ErrorTone;


  string                        conf_id;
  auto_ptr<AmConferenceChannel> channel;

  int                           state;
  string                        dtmf_seq;
  bool                          dialedout;
  string                        dialout_suffix;
  string                        dialout_id;
  auto_ptr<AmConferenceChannel> dialout_channel;

  bool                          allow_dialout;

  string                        from_header;
  string                        extra_headers;
  string                        language;

  bool                          listen_only;

  auto_ptr<AmSipRequest>        transfer_req;


  void createDialoutParticipant(const string& uri);
  void disconnectDialout();
  void connectMainChannel();
  void closeChannels();
  void setupAudio();

#ifdef WITH_SAS_TTS
  void sayTTS(string text);
  string last_sas;
  cst_voice* tts_voice;
  vector<AmAudioFile*> TTSFiles;
#endif

public:
  ConferenceDialog(const string& conf_id,
		   AmConferenceChannel* dialout_channel=0);

  ~ConferenceDialog();

  void process(AmEvent* ev);
  void onStart();
  void onDtmf(int event, int duration);
  void onInvite(const AmSipRequest& req);
  void onSessionStart();
  void onBye(const AmSipRequest& req);

  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);

#ifdef WITH_SAS_TTS
  void onZRTPEvent(zrtp_event_t event, zrtp_stream_ctx_t *stream_ctx);
#endif
};

#endif
// Local Variables:
// mode:C++
// End:

