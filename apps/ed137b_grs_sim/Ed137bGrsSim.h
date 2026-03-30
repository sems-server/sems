/*
 * ED-137B Ground Radio Station (GRS) Simulator for SEMS
 *
 * Naming convention used throughout this module:
 *   - ED-137B = EUROCAE standard revision B for VoIP in ATC
 *   - GRS     = Ground Radio Station (the radio equipment being simulated)
 *   - R2S     = Radio-to-SIP interface profile defined in ED-137B
 *   - Module  = "ed137b_grs_sim" (SEMS application name)
 *   - Classes = "Ed137bGrs..." prefix (e.g., Ed137bGrsSimFactory)
 *   - Macros  = "ED137B_..." prefix (e.g., ED137B_HDR_WG67_VERSION)
 *
 * Simulates an ED-137B compatible Ground Radio Station at the SIP
 * signalling level. Handles ED-137B SIP headers, R2S SDP attributes,
 * and logs all signalling parameter changes to a file.
 *
 * Audio uses G.711 A-law (PCMA) with R2S fmtp, generating a 400Hz
 * side-tone or silence to keep the RTP session alive.
 */

#ifndef _ED137B_GRS_SIM_H_
#define _ED137B_GRS_SIM_H_

#include "AmSession.h"
#include "AmRingTone.h"
#include "AmConfigReader.h"
#include "Ed137bGrsState.h"

#include <string>
using std::string;

class Ed137bGrsSimFactory : public AmSessionFactory
{
public:
  // Default GRS parameters (loaded from config)
  static Ed137bGrsState DefaultState;
  static string AudioMode;   // "tone", "silence"
  static int    ToneFreq;    // Hz (default 400)

  // Shared change logger (one log file for all sessions)
  static Ed137bGrsChangeLogger Logger;

  Ed137bGrsSimFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
                      const map<string, string>& app_params);
};

class Ed137bGrsSimSession : public AmSession
{
  Ed137bGrsState state;        // current GRS state for this session
  AmRingTone*    tone;         // side-tone generator (owned)
  bool           session_started;

public:
  Ed137bGrsSimSession();
  ~Ed137bGrsSimSession();

  // SIP event handlers
  void onInvite(const AmSipRequest& req);
  void onSessionStart();
  void onBye(const AmSipRequest& req);
  void onSipRequest(const AmSipRequest& req);

  // SDP hooks -- inject ED-137B attributes
  bool getSdpOffer(AmSdp& offer);
  bool getSdpAnswer(const AmSdp& offer, AmSdp& answer);

  // SIP send hooks -- inject ED-137B headers
  void onSendRequest(AmSipRequest& req, int& flags);
  void onSendReply(const AmSipRequest& req, AmSipReply& reply, int& flags);

  void process(AmEvent* event);

private:
  // Parse incoming SDP/headers and log changes
  void updateStateFromRequest(const AmSipRequest& req);

  // Apply ED-137B SDP attributes to an SDP structure
  void applyEd137bSdp(AmSdp& sdp);
};

#endif // _ED137B_GRS_SIM_H_
