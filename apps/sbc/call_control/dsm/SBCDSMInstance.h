/*
 * Copyright (C) 2013 Stefan Sayer
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

#ifndef _DSMSBCInstance_H
#define _DSMSBCInstance_H

#include <string>
#include <map>

#include "ExtendedCCInterface.h"
#include "DSMSession.h"
#include "DSMStateEngine.h"
#include "AmPlaylist.h"

/** DSM interpreter instance for one call leg */
class SBCDSMInstance 
: public AmObject, public DSMSession
{
  private:
  DSMStateEngine engine;
  string appBundle;
  string startDiagName;

  // owned by this instance
  std::set<DSMDisposable*> gc_trash;
  vector<AmAudio*> audiofiles;

  auto_ptr<AmSession> dummy_session;  

  auto_ptr<AmPlaylist> playlist;

  void resetDummySession(SimpleRelayDialog *relay);

  SBCCallLeg *call;

  bool local_media_connected;

  public:
    SBCDSMInstance(SBCCallLeg *call, const VarMapT& values);
    ~SBCDSMInstance();
    CCChainProcessing onInitialInvite(SBCCallLeg* call, InitialInviteHandlerParams& params);
    void onStateChange(SBCCallLeg* call, const CallLeg::StatusChangeCause& cause);
    CCChainProcessing onBLegRefused(SBCCallLeg* call, const AmSipReply& reply);

    CCChainProcessing onInDialogRequest(SBCCallLeg* call, const AmSipRequest& req);
    CCChainProcessing onInDialogReply(SBCCallLeg* call, const AmSipReply& reply);

    CCChainProcessing onEvent(SBCCallLeg* call, AmEvent* event);
    CCChainProcessing onDtmf(SBCCallLeg *call, int event, int duration);

    CCChainProcessing putOnHold(SBCCallLeg* call);
    CCChainProcessing resumeHeld(SBCCallLeg* call, bool send_reinvite);
    CCChainProcessing createHoldRequest(SBCCallLeg* call, AmSdp& sdp);
    CCChainProcessing handleHoldReply(SBCCallLeg* call, bool succeeded);

    AmPlaylist* getPlaylist();

    // ------------ simple relay interface --------------------------------------- */
    bool init(SBCCallProfile &profile, SimpleRelayDialog *relay);
    void initUAC(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest &req);
    void initUAS(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest &req);
    void finalize(SBCCallProfile &profile, SimpleRelayDialog *relay);
    void onSipRequest(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest& req);
    void onSipReply(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest& req,
		    const AmSipReply& reply,
		    AmBasicSipDialog::Status old_dlg_status);
    void onB2BRequest(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipRequest& req);
    void onB2BReply(SBCCallProfile &profile, SimpleRelayDialog *relay, const AmSipReply& reply);

    /* -------- DSM session API - mostly not implemented -------------------------- */
    void playPrompt(const string& name, bool loop = false, bool front = false);
    void playFile(const string& name, bool loop, bool front = false);
    void playSilence(unsigned int length, bool front = false);
    void recordFile(const string& name);
    unsigned int getRecordLength();
    unsigned int getRecordDataSize();
    void stopRecord();
    void setInOutPlaylist();
    void setInputPlaylist();
    void setOutputPlaylist();
    
    void addToPlaylist(AmPlaylistItem* item, bool front = false);
    void flushPlaylist();
    void setPromptSet(const string& name);
    void addSeparator(const string& name, bool front = false);
    void connectMedia();
    void disconnectMedia();
    void mute();
    void unmute();

  /** B2BUA functions */
  void B2BconnectCallee(const string& remote_party,
				const string& remote_uri,
				bool relayed_invite = false);
  void B2BterminateOtherLeg();
  void B2BaddReceivedRequest(const AmSipRequest& req);
  void B2BsetRelayEarlyMediaSDP(bool enabled);
  void B2BsetHeaders(const string& hdr, bool replaceCRLF);
  void B2BclearHeaders();
  void B2BaddHeader(const string& hdr);
  void B2BremoveHeader(const string& hdr);

  void transferOwnership(DSMDisposable* d);
  void releaseOwnership(DSMDisposable* d);
};

#endif
