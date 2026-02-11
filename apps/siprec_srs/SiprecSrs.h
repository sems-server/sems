/*
 * Minimal SIPREC SRS (Session Recording Server) for SEMS.
 *
 * Receives SIPREC INVITE with 2 sendonly m-lines (one per call leg),
 * records both RTP streams to separate WAV files, and saves the
 * SIPREC metadata XML alongside the recordings.
 */

#ifndef _SIPREC_SRS_H
#define _SIPREC_SRS_H

#include "AmSession.h"
#include "AmConfigReader.h"
#include "RtpReceiver.h"

#include <memory>

class SiprecSrsFactory : public AmSessionFactory {
  static string recording_dir_;

public:
  SiprecSrsFactory(const string& name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
                      const map<string, string>& app_params);

  static const string& getRecordingDir() { return recording_dir_; }
};

class SiprecSrsSession : public AmSession {
  RtpReceiver receiver_a_;  // A-leg (label 1)
  RtpReceiver receiver_b_;  // B-leg (label 2)
  string session_id_;       // unique recording session ID
  string recording_dir_;
  int codec_pt_;            // payload type from SDP offer

  bool parseInvite(const AmSipRequest& req);
  void saveMetadata(const string& xml, const string& suffix);

public:
  SiprecSrsSession();
  ~SiprecSrsSession();

  void onInvite(const AmSipRequest& req);
  void onBye(const AmSipRequest& req);
  void onSessionStart();

  // Override to handle 2 m-lines
  bool getSdpAnswer(const AmSdp& offer, AmSdp& answer);
  int onSdpCompleted(const AmSdp& offer, const AmSdp& answer);
};

#endif
