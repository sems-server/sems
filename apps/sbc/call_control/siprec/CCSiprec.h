/*
 * SIPREC (RFC 7865/7866) Session Recording Client for SEMS SBC.
 *
 * SBC call_control module that forks RTP streams to a Session Recording
 * Server (SRS) using SIPREC signaling.
 */

#ifndef _CC_SIPREC_H
#define _CC_SIPREC_H

#include "AmApi.h"
#include "AmThread.h"
#include "SBCCallProfile.h"
#include "SBCCallLeg.h"
#include "ExtendedCCInterface.h"

#include "SiprecDialog.h"

#include <map>
#include <string>
#include <memory>

using std::string;
using std::map;

/** Per-call recording state */
struct SiprecCallState {
  string call_id;
  string caller_uri;
  string callee_uri;
  string a_leg_ltag;
  string b_leg_ltag;
  SiprecDialog* dialog;   // owned, deleted on cleanup
  bool is_aleg;           // which leg this state belongs to

  SiprecCallState()
    : dialog(NULL), is_aleg(true) {}
  ~SiprecCallState() { delete dialog; }
};

class CCSiprec : public AmDynInvoke,
                 public ExtendedCCInterface,
                 public AmObject
{
  static CCSiprec* _instance;

  // Global configuration
  string srs_uri_;
  bool   recording_mandatory_;
  string codec_name_;          // e.g. "PCMA", "PCMU"
  int    codec_payload_type_;  // RTP payload type (8=PCMA, 0=PCMU)
  int    codec_clock_rate_;    // clock rate (8000)
  int    transport_;           // TP_RTPAVP or TP_RTPSAVP (RFC 7866 Section 12.2)

  // Active recording sessions, keyed by A-leg local tag
  map<string, SiprecCallState*> sessions_;
  AmMutex sessions_mut_;

  // DynInvoke callbacks
  void start(const string& cc_name, const string& ltag,
             SBCCallProfile* call_profile,
             int start_ts_sec, int start_ts_usec,
             const AmArg& values, int timer_id, AmArg& res);
  void connect(const string& cc_name, const string& ltag,
               SBCCallProfile* call_profile,
               const string& other_ltag,
               int connect_ts_sec, int connect_ts_usec);
  void end(const string& cc_name, const string& ltag,
           SBCCallProfile* call_profile,
           int end_ts_sec, int end_ts_usec);

  /** Find or create recording state for a call leg */
  SiprecCallState* getState(const string& ltag);
  void removeState(const string& ltag);

public:
  CCSiprec();
  ~CCSiprec();
  static CCSiprec* instance();

  // AmDynInvoke
  void invoke(const string& method, const AmArg& args, AmArg& ret) override;
  int onLoad();

  // ExtendedCCInterface - call lifecycle
  bool init(SBCCallLeg *call, const map<string, string> &values) override;
  void onStateChange(SBCCallLeg *call,
                     const CallLeg::StatusChangeCause &cause) override;
  void onDestroyLeg(SBCCallLeg *call) override;
  CCChainProcessing onInitialInvite(SBCCallLeg *call,
                                    InitialInviteHandlerParams &params) override;

  // ExtendedCCInterface - in-dialog reply (for recording indication)
  CCChainProcessing onInDialogReply(SBCCallLeg *call,
                                    const AmSipReply &reply) override;

  // ExtendedCCInterface - RTP relay hook
  void onAfterRTPRelay(SBCCallLeg *call, AmRtpPacket* p,
                       sockaddr_storage* remote_addr) override;

  // ExtendedCCInterface - hold/resume
  void holdRequested(SBCCallLeg *call) override;
  void resumeAccepted(SBCCallLeg *call) override;
};

#endif
