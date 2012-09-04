#ifndef __SBCCALL_LEG_H
#define __SBCCALL_LEG_H

#include "SBC.h"
#include "ExtendedCCInterface.h"

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

  // current timer ID - cc module setting timer will use this
  int cc_timer_id;

  struct timeval call_start_ts;
  struct timeval call_connect_ts;
  struct timeval call_end_ts;

  // auth
  AmSessionEventHandler* auth;

  /** Storage for remembered payload IDs from SDP offer to be put correctly into
   * SDP answer (we avoid with this parsing SDP offer again when processing the
   * answer). We can not use call_profile.transcoder.audio_codecs for storing
   * the payload IDs because they need to be remembered per media stream. */
  PayloadIdMapping transcoder_payload_mapping;

  SBCCallProfile call_profile;

  void fixupCCInterface(const string& val, CCInterface& cc_if);

  /** handler called when the second leg is connected */
  virtual void onCallConnected(const AmSipReply& reply);

  /** handler called when call is stopped */
  virtual void onCallStopped();

  /** handler called when SST timeout occured */
  void onSessionTimeout();

  /** handler called when no ACK received */
  void onNoAck(unsigned int cseq);

  /** handler called when we receive 408/481 */
  void onRemoteDisappeared(const AmSipReply& reply);

  /** stop call (both legs, CC) */
  void stopCall();

  /* set call timer (if enabled) */
  bool startCallTimers();
  /* clear call timer */
  void stopCallTimers();

  /** initialize call control module interfaces @return sucess or not*/
  bool getCCInterfaces();
  /** call is started */
  bool CCStart(const AmSipRequest& req);
  /** connection of second leg */
  void CCConnect(const AmSipReply& reply);
  /** end call */
  void CCEnd();
  void CCEnd(const CCInterfaceListIteratorT& end_interface);

  void connectCallee(const string& remote_party, const string& remote_uri, const string &from, const AmSipRequest &invite_req);
  int filterSdp(AmMimeBody &body, const string &method);
  void appendTranscoderCodecs(AmSdp &sdp);
  void savePayloadIDs(AmSdp &sdp);

  /** apply A leg configuration from call profile */
  void applyAProfile();

  /** apply B leg configuration from call profile */
  void applyBProfile();

  virtual void onCallStatusChange();

 public:

  SBCCallLeg(const SBCCallProfile& call_profile);
  SBCCallLeg(const SBCCallLeg* caller, const SBCCallProfile& _call_profile);
  ~SBCCallLeg();

  void process(AmEvent* ev);
  void onBye(const AmSipRequest& req);
  void onInvite(const AmSipRequest& req);
  void onCancel(const AmSipRequest& cancel);

  void onDtmf(int event, int duration);

  void onSystemEvent(AmSystemEvent* ev);

  virtual void onStart();

  UACAuthCred* getCredentials();

  void setAuthHandler(AmSessionEventHandler* h) { auth = h; }

  /** save call timer; only effective before call is connected */
  void saveCallTimer(int timer, double timeout);
  /** clear saved call timer, only effective before call is connected */
  void clearCallTimer(int timer);
  /** clear all saved call timer, only effective before call is connected */
  void clearCallTimers();

  // SBC interface usable from CC modules

  void setLocalParty(const string &party, const string &uri) { dlg.local_party = party; dlg.local_uri = uri; }
  void setRemoteParty(const string &party, const string &uri) { dlg.remote_party = party; dlg.remote_uri = uri; }

  void addCallee(CallLeg *callee, const AmSipRequest &invite) { CallLeg::addCallee(callee, invite); }
  void addCallee(const string &session_tag, const AmSipRequest &invite) { CallLeg::addCallee(session_tag, invite); }
  void replaceExistingLeg(const string &session_tag, const AmSipRequest &invite) { CallLeg::replaceExistingLeg(session_tag, invite); }

  SBCCallProfile &getCallProfile() { return call_profile; }

  // media interface must be accessible from CC modules
  AmB2BMedia *getMediaSession() { return media_session; }
  virtual bool updateLocalSdp(AmSdp &sdp);
  virtual bool updateRemoteSdp(AmSdp &sdp);
  void changeRtpMode(RTPRelayMode new_mode);

  bool reinvite(const AmSdp &sdp, unsigned &request_cseq);

  virtual void terminateLeg();
  int relayEvent(AmEvent* ev);
  void onSipRequest(const AmSipRequest& req);
  bool isALeg() { return a_leg; }

 protected:

  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
  void onSendRequest(AmSipRequest& req, int flags);

  void onOtherBye(const AmSipRequest& req);

  void onControlCmd(string& cmd, AmArg& params);

  void createCalleeSession();
};

#endif
