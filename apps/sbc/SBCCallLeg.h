/*
 * Copyright (C) 2010-2011 Stefan Sayer
 * Copyright (C) 2012-2013 FRAFOS GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef __SBCCALL_LEG_H
#define __SBCCALL_LEG_H

#include "SBC.h"
#include "ExtendedCCInterface.h"
#include "sbc_events.h"
#include "RateLimit.h"

class PayloadIdMapping
{
  private:
    std::map<int, int> mapping;
      
  public:
    void map(int stream_index, int payload_index, int payload_id);
    int get(int stream_index, int payload_index);
    void reset();
};

class SBCCallLeg : public CallLeg, public CredentialHolder
{
  enum {
    BB_Init = 0,
    BB_Dialing,
    BB_Connected,
    BB_Teardown
  } CallerState;

  int m_state;

  map<int, double> call_timers;

  // call control
  vector<AmDynInvoke*> cc_modules;
  vector<ExtendedCCInterface*> cc_ext;

  // modules to initialize
  CCInterfaceListT cc_module_queue;

  // current timer ID - cc module setting timer will use this
  int cc_timer_id;
  int ext_cc_timer_id; // for assigning IDs to timers through "extended CC interface"

  // auth
  AmSessionEventHandler* auth;
  AmDynInvoke* auth_di;

  /** Storage for remembered payload IDs from SDP offer to be put correctly into
   * SDP answer (we avoid with this parsing SDP offer again when processing the
   * answer). We can not use call_profile.transcoder.audio_codecs for storing
   * the payload IDs because they need to be remembered per media stream. */
  PayloadIdMapping transcoder_payload_mapping;

  SBCCallProfile call_profile;

  // Rate limiting
  auto_ptr<RateLimit> rtp_relay_rate_limit;
  
  // Measurements
  list<atomic_int*> rtp_pegs;

  /** common logger for RTP/RTCP and SIP packets */
  msg_logger *logger;

  void setLogger(msg_logger *_logger);

  void fixupCCInterface(const string& val, CCInterface& cc_if);

  /** handler called when call is stopped (see AmSession) */
  virtual void onStop();

  /** call is started */
  bool CCStart(const AmSipRequest& req);
  /** connection of second leg */
  void CCConnect(const AmSipReply& reply);
  /** end call */
  void CCEnd();
  void CCEnd(const CCInterfaceListIteratorT& end_interface);

  void connectCallee(const string& remote_party, const string& remote_uri, 
		     const string &from, const AmSipRequest &original_invite, 
		     const AmSipRequest &invite_req);

  int filterSdp(AmMimeBody &body, const string &method);
  void appendTranscoderCodecs(AmSdp &sdp);
  void savePayloadIDs(AmSdp &sdp);

  /** apply A leg configuration from call profile */
  void applyAProfile();

  /** apply B leg configuration from call profile */
  void applyBProfile();

  virtual void onCallStatusChange(const StatusChangeCause &cause);
  virtual void onBLegRefused(const AmSipReply& reply);

  /** handler called when the call is refused with a non-ok reply or canceled */
  virtual void onCallFailed(CallFailureReason reason, const AmSipReply *reply);

  /** handler called when the second leg is connected */
  virtual void onCallConnected(const AmSipReply& reply);

  /** Call-backs used by RTP stream(s)
   *  Note: these methods will be called from the RTP receiver thread.
   */
  virtual bool onBeforeRTPRelay(AmRtpPacket* p, sockaddr_storage* remote_addr);
  virtual void onAfterRTPRelay(AmRtpPacket* p, sockaddr_storage* remote_addr);

  void logCallStart(const AmSipReply& reply);
  void logCanceledCall();

  void alterHoldRequestImpl(AmSdp &sdp); // do the SDP update (called by alterHoldRequest)

 public:

  SBCCallLeg(const SBCCallProfile& call_profile, AmSipDialog* dlg=NULL,
	     AmSipSubscription* p_subs=NULL);
  SBCCallLeg(SBCCallLeg* caller, AmSipDialog* dlg=NULL,
	     AmSipSubscription* p_subs=NULL);
  SBCCallLeg(AmSipDialog* dlg=NULL,AmSipSubscription* p_subs=NULL);
  ~SBCCallLeg();

  void process(AmEvent* ev);
  void onInvite(const AmSipRequest& req);

  void onDtmf(int event, int duration);

  virtual void onStart();
  virtual void onBeforeDestroy();

  UACAuthCred* getCredentials();

  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }
  void setAuthDI(AmDynInvoke* di_inst) { auth_di = di_inst; }

  bool initCCExtModules(const CCInterfaceListT& cc_module_list, const vector<AmDynInvoke*>& cc_module_di);
  bool initPendingCCExtModules();
  void addPendingCCExtModule(const string& cc_name, const string& cc_module, const map<string, string>& cc_values);

  /** save call timer; only effective before call is connected */
  void saveCallTimer(int timer, double timeout);
  /** clear saved call timer, only effective before call is connected */
  void clearCallTimer(int timer);
  /** clear all saved call timer, only effective before call is connected */
  void clearCallTimers();

  // SBC interface usable from CC modules

  void setLocalParty(const string &party, const string &uri) { 
    dlg->setLocalParty(party); dlg->setLocalUri(uri);
  }

  void setRemoteParty(const string &party, const string &uri) { 
    dlg->setRemoteParty(party); dlg->setRemoteUri(uri);
  }

  SBCCallProfile &getCallProfile() { return call_profile; }
  CallStatus getCallStatus() { return CallLeg::getCallStatus(); }

  void setRTPMeasurements(const list<atomic_int*>& rtp_meas) { rtp_pegs = rtp_meas; }
  const RateLimit* getRTPRateLimit() { return rtp_relay_rate_limit.get(); }
  void setRTPRateLimit(RateLimit* rl) { rtp_relay_rate_limit.reset(rl); }

  // media interface must be accessible from CC modules
  AmB2BMedia *getMediaSession() { return AmB2BSession::getMediaSession(); }
  virtual void updateLocalSdp(AmSdp &sdp);
  void changeRtpMode(RTPRelayMode new_mode) { CallLeg::changeRtpMode(new_mode); }

  bool reinvite(const AmSdp &sdp, unsigned &request_cseq);

  int relayEvent(AmEvent* ev);
  void onSipRequest(const AmSipRequest& req);
  bool isALeg() { return a_leg; }

  // timers accessible from CC modules
  int startTimer(double timeout) { setTimer(ext_cc_timer_id, timeout); return ext_cc_timer_id++; }

  virtual void setMediaSession(AmB2BMedia *new_session);
  virtual void computeRelayMask(const SdpMedia &m, bool &enable, PayloadMask &mask);

 protected:
  /** set to true once CCStart passed to call CCEnd implicitly (from onStop)
   * only when CCStart was called */
  bool cc_started;

  // Call duration measurements
  struct timeval call_start_ts;
  struct timeval call_connect_ts;
  struct timeval call_end_ts;

  void setOtherId(const AmSipReply& reply);
  void setOtherId(const string& n_other_id) { CallLeg::setOtherId(n_other_id); }

  void onSipReply(const AmSipRequest& req, const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
  void onSendRequest(AmSipRequest& req, int &flags);

  virtual void onInitialReply(B2BSipReplyEvent *e);

  void onRemoteDisappeared(const AmSipReply& reply);
  void onBye(const AmSipRequest& req);
  void onOtherBye(const AmSipRequest& req);

  void onControlCmd(string& cmd, AmArg& params);

  /* set call timer (if enabled) */
  virtual bool startCallTimers();
  /* clear call timer */
  virtual void stopCallTimers();

  const map<int, double> getCallTimers() { return call_timers; }

  void createCalleeSession();

  /** initialize call control module interfaces @return sucess or not*/
  bool getCCInterfaces();
  vector<AmDynInvoke*>& getCCModules() { return cc_modules; }

  virtual void createHoldRequest(AmSdp &sdp);
  virtual void alterHoldRequest(AmSdp &sdp);
  virtual void holdRequested();
  virtual void holdAccepted();
  virtual void holdRejected();
  virtual void resumeRequested();
  virtual void resumeAccepted();
  virtual void resumeRejected();

  int applySSTCfg(AmConfigReader& sst_cfg, const AmSipRequest* p_req);

  bool openLogger(const std::string &path);
  msg_logger *getLogger() { return logger; }

  virtual double get491RetryTime() { return (get_random() % call_profile.max_491_retry_time) / 1000.0; }
};

#endif
