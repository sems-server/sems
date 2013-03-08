/*
 * Copyright (C) 2008 iptego GmbH
 * Copyright (C) 2012 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
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
#ifndef _DSM_CALL_H
#define _DSM_CALL_H
#include "AmB2BSession.h"
#include "AmPromptCollection.h"
#include "AmUACAuth.h"

#include "DSMSession.h"
#include "DSMStateEngine.h"
#include "DSMStateDiagramCollection.h"

#include <set>
/** implementation of the actual session in DSM */
class DSMCall : public AmB2BCallerSession,
		  public DSMSession,
		  public CredentialHolder
{
  std::auto_ptr<UACAuthCred> cred;
  
  DSMStateEngine engine;
  AmPromptCollection* prompts;
  AmPromptCollection* default_prompts;
  string startDiagName;
  AmPlaylist playlist;

  bool run_invite_event;

  bool process_invite;
  bool process_sessionstart;

  vector<AmAudio*> audiofiles;
  AmAudioFile* rec_file;
  map<string, AmPromptCollection*> prompt_sets;
  std::set<AmPromptCollection*> used_prompt_sets;

  // owned by this instance
  std::set<DSMDisposable*> gc_trash;
  
  bool checkVar(const string& var_name, const string& var_val);
  string getVar(const string& var_name);

public:
  DSMCall(const DSMScriptConfig& config,
	  AmPromptCollection* prompts,
	  DSMStateDiagramCollection& diags,
	  const string& startDiagName,
	  UACAuthCred* credentials = NULL);
  ~DSMCall();

  void onStart();
  void onInvite(const AmSipRequest& req);
  void onOutgoingInvite(const string& headers);
  void onRinging(const AmSipReply& reply);
  void onEarlySessionStart();
  void onSessionStart();
  int  onSdpCompleted(const AmSdp& offer, const AmSdp& answer);
  void startSession();
  void onCancel(const AmSipRequest& cancel);
  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec);
  void onBeforeDestroy();

  void onSessionTimeout();
  void onRtpTimeout();
  void onRemoteDisappeared(const AmSipReply& reply);

  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);

  bool getSdpOffer(AmSdp& offer);
  bool getSdpAnswer(const AmSdp& offer, AmSdp& answer);

  virtual void onNoAck(unsigned int cseq);

  void onSystemEvent(AmSystemEvent* ev);

  void process(AmEvent* event);

  UACAuthCred* getCredentials();

  void addPromptSet(const string& name, AmPromptCollection* prompt_set);
  void setPromptSets(map<string, AmPromptCollection*>& new_prompt_sets);

  // DSMSession interface
  void playPrompt(const string& name, bool loop = false, bool front = false);
  void flushPlaylist();
  void addToPlaylist(AmPlaylistItem* item, bool front = false);
  void playFile(const string& name, bool loop, bool front=false);
  void playSilence(unsigned int length, bool front=false);
  void recordFile(const string& name);
  unsigned int getRecordLength();
  unsigned int getRecordDataSize();
  void stopRecord();
  void setInOutPlaylist();
  void setInputPlaylist();
  void setOutputPlaylist();

  void setPromptSet(const string& name);
  void addSeparator(const string& name, bool front = false);
  void connectMedia();
  void disconnectMedia();
  void mute();
  void unmute();

  void transferOwnership(DSMDisposable* d);
  void releaseOwnership(DSMDisposable* d);
  
protected:
  // AmB2BSession methods
  void onOtherBye(const AmSipRequest& req);
  bool onOtherReply(const AmSipReply& reply);
public:
  AmB2BCalleeSession* newCalleeSession();

  void B2BterminateOtherLeg();
  void B2BconnectCallee(const string& remote_party,
			const string& remote_uri,
			bool relayed_invite = false);

  void B2BaddReceivedRequest(const AmSipRequest& req);
  void B2BsetRelayEarlyMediaSDP(bool enabled);
  void B2BsetHeaders(const string& hdr, bool replaceCRLF);
  void B2BclearHeaders();
  void B2BaddHeader(const string& hdr);
  void B2BremoveHeader(const string& hdr);
};

class DSMCallCalleeSession : public AmB2BCalleeSession,
			     public CredentialHolder
{
  std::auto_ptr<UACAuthCred> cred;
  std::auto_ptr<AmSessionEventHandler> auth;


protected:

  void onSendRequest(AmSipRequest& req, int& flags);
  void onSipReply(const AmSipRequest& req, const AmSipReply& reply,
		  AmBasicSipDialog::Status old_dlg_status);

public:
  DSMCallCalleeSession(const string& other_local_tag);
  DSMCallCalleeSession(const AmB2BCallerSession* caller);

  void setCredentials(const string& realm, const string& user, const string& pwd);
  UACAuthCred* getCredentials();
  void setAuthHandler(AmSessionEventHandler* h);

};

#endif
// Local Variables:
// mode:C++
// End:
